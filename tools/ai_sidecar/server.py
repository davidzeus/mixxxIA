"""FastAPI sidecar that turns audio files into embeddings + tags for Mixxx.

Run it with:

    python -m uvicorn server:app --host 127.0.0.1 --port 8765

Mixxx talks to it over HTTP (see src/aifeatures/). Endpoints:

    GET  /health                      -> backend info / readiness
    POST /embed      {path|samples}   -> single track analysis
    POST /embed_batch {items:[...]}   -> several tracks in one request

Environment variables:
    MIXXX_AI_BACKEND   auto|clap|librosa   (default: auto)
"""

from __future__ import annotations

import logging
import os
from typing import Optional

import numpy as np
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from embedder import EmbedResult, TARGET_SR, build_embedder

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("ai_sidecar.server")

app = FastAPI(title="Mixxx AI sidecar", version="1.0")

_embedder = None


def get_embedder():
    global _embedder
    if _embedder is None:
        prefer = os.environ.get("MIXXX_AI_BACKEND", "auto")
        _embedder = build_embedder(prefer=prefer)
    return _embedder


class EmbedRequest(BaseModel):
    """Either a file path (preferred) or raw mono samples must be given."""

    path: Optional[str] = None
    samples: Optional[list[float]] = None
    sample_rate: Optional[int] = Field(default=None)


class EmbedResponse(BaseModel):
    embedding: list[float]
    dim: int
    model_version: str
    genre: Optional[str] = None
    genre_confidence: Optional[float] = None
    mood: Optional[str] = None
    energy: Optional[float] = None
    danceability: Optional[float] = None
    bpm_estimate: Optional[float] = None


class BatchRequest(BaseModel):
    items: list[EmbedRequest]


class BatchResponse(BaseModel):
    results: list[Optional[EmbedResponse]]
    errors: list[Optional[str]]


def _to_response(r: EmbedResult) -> EmbedResponse:
    return EmbedResponse(
        embedding=r.embedding,
        dim=r.dim,
        model_version=r.model_version,
        genre=r.genre,
        genre_confidence=r.genre_confidence,
        mood=r.mood,
        energy=r.energy,
        danceability=r.danceability,
        bpm_estimate=r.bpm_estimate,
    )


def _analyze(req: EmbedRequest) -> EmbedResult:
    embedder = get_embedder()
    if req.path:
        return embedder.embed_file(req.path)
    if req.samples is not None:
        # Write samples to a temp WAV-like buffer the embedder can read.
        import soundfile as sf
        import tempfile

        sr = req.sample_rate or TARGET_SR
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
            sf.write(tmp.name, np.asarray(req.samples, dtype=np.float32), sr)
            tmp_path = tmp.name
        try:
            return embedder.embed_file(tmp_path)
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass
    raise ValueError("request must contain either 'path' or 'samples'")


@app.get("/health")
def health() -> dict:
    embedder = get_embedder()
    return {
        "status": "ok",
        "backend": embedder.model_version,
        "target_sample_rate": TARGET_SR,
    }


@app.post("/embed", response_model=EmbedResponse)
def embed(req: EmbedRequest) -> EmbedResponse:
    try:
        return _to_response(_analyze(req))
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        logger.exception("embed failed")
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@app.post("/embed_batch", response_model=BatchResponse)
def embed_batch(req: BatchRequest) -> BatchResponse:
    results: list[Optional[EmbedResponse]] = []
    errors: list[Optional[str]] = []
    for item in req.items:
        try:
            results.append(_to_response(_analyze(item)))
            errors.append(None)
        except Exception as exc:  # noqa: BLE001 - report per-item, keep going
            results.append(None)
            errors.append(str(exc))
    return BatchResponse(results=results, errors=errors)
