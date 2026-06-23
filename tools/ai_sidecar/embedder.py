"""Audio embedding + tagging backends for the Mixxx AI sidecar.

Two backends are provided:

* ``ClapEmbedder``   -- LAION-CLAP. Produces a 512-d audio embedding and can do
                        zero-shot genre/mood classification by comparing the
                        audio embedding against text-prompt embeddings.
* ``LibrosaEmbedder`` -- pure-DSP fallback. Produces a deterministic feature
                        vector (MFCC + chroma + spectral stats). No genre, but
                        still gives a usable similarity space and energy/
                        danceability estimates so the whole pipeline works
                        without the heavy ML stack.

The active backend is chosen at startup by :func:`build_embedder`, which tries
CLAP first and silently falls back to librosa if torch / laion_clap are not
importable or the model weights cannot be loaded.
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import Optional

import numpy as np

logger = logging.getLogger("ai_sidecar.embedder")

# Audio is resampled to this rate before analysis. CLAP expects 48 kHz.
TARGET_SR = 48000

# Genre / mood vocabularies used for zero-shot classification (CLAP only).
GENRE_PROMPTS = [
    "house", "techno", "trance", "drum and bass", "dubstep", "hip hop",
    "rap", "reggaeton", "pop", "rock", "metal", "jazz", "blues", "funk",
    "soul", "disco", "ambient", "classical", "latin", "salsa", "cumbia",
    "afrobeat", "r&b", "electro", "minimal", "deep house", "tech house",
]
MOOD_PROMPTS = [
    "energetic", "dark", "uplifting", "melancholic", "aggressive",
    "chill", "romantic", "euphoric", "groovy", "dreamy",
]


@dataclass
class EmbedResult:
    """Result of analysing a single track."""

    embedding: list[float]
    dim: int
    model_version: str
    genre: Optional[str] = None
    genre_confidence: Optional[float] = None
    mood: Optional[str] = None
    energy: Optional[float] = None
    danceability: Optional[float] = None
    bpm_estimate: Optional[float] = None
    extra: dict = field(default_factory=dict)


def select_device() -> str:
    """Pick the compute device automatically.

    Uses an NVIDIA GPU (CUDA) when one is available and the installed torch
    build supports it; otherwise falls back to CPU. Only NVIDIA/CUDA is
    targeted on purpose — it's the most plug-and-play GPU path (the CUDA torch
    wheel bundles the runtime, so only the NVIDIA driver is needed).
    """
    try:
        import torch

        if torch.cuda.is_available():
            return "cuda"
    except Exception:  # noqa: BLE001 - torch missing/broken -> CPU
        pass
    return "cpu"


def _l2_normalize(vec: np.ndarray) -> np.ndarray:
    norm = np.linalg.norm(vec)
    if norm < 1e-9:
        return vec
    return vec / norm


def _load_audio(path: str) -> tuple[np.ndarray, int]:
    """Load an audio file as mono float32 at TARGET_SR."""
    import librosa

    samples, sr = librosa.load(path, sr=TARGET_SR, mono=True)
    return samples.astype(np.float32), sr


def _dsp_dynamics(samples: np.ndarray, sr: int) -> dict:
    """Compute energy / danceability / tempo estimates from raw audio.

    These are cheap heuristics shared by both backends so the values are
    consistent regardless of which embedder is active.
    """
    import librosa

    if samples.size == 0:
        return {"energy": 0.0, "danceability": 0.0, "bpm_estimate": None}

    rms = float(np.sqrt(np.mean(np.square(samples))))
    # Map RMS (roughly 0..0.3 for music) onto a 0..1 energy scale.
    energy = float(np.clip(rms / 0.3, 0.0, 1.0))

    try:
        tempo, _ = librosa.beat.beat_track(y=samples, sr=sr)
        bpm = float(np.atleast_1d(tempo)[0])
    except Exception:  # noqa: BLE001 - tempo estimation is best-effort
        bpm = None

    try:
        onset_env = librosa.onset.onset_strength(y=samples, sr=sr)
        # Pulse clarity ~ how strongly a steady beat is present.
        pulse = librosa.beat.plp(onset_envelope=onset_env, sr=sr)
        danceability = float(np.clip(np.mean(pulse) * 2.0, 0.0, 1.0))
    except Exception:  # noqa: BLE001
        danceability = energy  # fall back to energy as a proxy

    return {"energy": energy, "danceability": danceability, "bpm_estimate": bpm}


class LibrosaEmbedder:
    """Deterministic DSP feature embedder (no external model)."""

    model_version = "librosa-mfcc-chroma-v1"
    device = "cpu"  # pure NumPy/DSP, no GPU involved

    def embed_file(self, path: str) -> EmbedResult:
        import librosa

        samples, sr = _load_audio(path)
        if samples.size == 0:
            raise ValueError(f"empty or unreadable audio: {path}")

        mfcc = librosa.feature.mfcc(y=samples, sr=sr, n_mfcc=20)
        chroma = librosa.feature.chroma_stft(y=samples, sr=sr)
        contrast = librosa.feature.spectral_contrast(y=samples, sr=sr)

        # Summarise each feature over time with mean + std, then concatenate.
        parts = []
        for feat in (mfcc, chroma, contrast):
            parts.append(np.mean(feat, axis=1))
            parts.append(np.std(feat, axis=1))
        vec = _l2_normalize(np.concatenate(parts).astype(np.float32))

        dyn = _dsp_dynamics(samples, sr)
        return EmbedResult(
            embedding=vec.tolist(),
            dim=int(vec.shape[0]),
            model_version=self.model_version,
            genre=None,
            genre_confidence=None,
            mood=None,
            energy=dyn["energy"],
            danceability=dyn["danceability"],
            bpm_estimate=dyn["bpm_estimate"],
        )


class ClapEmbedder:
    """LAION-CLAP embedder with zero-shot genre / mood classification."""

    model_version = "clap-htsat-unfused-v1"

    def __init__(self) -> None:
        import laion_clap  # noqa: F401  (import errors handled by caller)

        # Auto-select NVIDIA GPU if present, else CPU.
        self.device = select_device()
        logger.info("CLAP using device: %s", self.device)
        self._model = laion_clap.CLAP_Module(
                enable_fusion=False, device=self.device)
        self._model.load_ckpt()  # downloads default checkpoint on first run
        # Pre-compute text embeddings for the zero-shot vocabularies.
        self._genre_emb = self._embed_text(GENRE_PROMPTS)
        self._mood_emb = self._embed_text(MOOD_PROMPTS)

    def _embed_text(self, prompts: list[str]) -> np.ndarray:
        emb = self._model.get_text_embedding(prompts, use_tensor=False)
        return np.asarray([_l2_normalize(np.asarray(e)) for e in emb])

    def _classify(self, audio_emb: np.ndarray, text_emb: np.ndarray,
                  labels: list[str]) -> tuple[str, float]:
        sims = text_emb @ audio_emb
        idx = int(np.argmax(sims))
        # Softmax over similarities for a calibrated-ish confidence.
        exp = np.exp(sims - np.max(sims))
        conf = float(exp[idx] / np.sum(exp))
        return labels[idx], conf

    def embed_file(self, path: str) -> EmbedResult:
        samples, sr = _load_audio(path)
        if samples.size == 0:
            raise ValueError(f"empty or unreadable audio: {path}")

        audio_emb = self._model.get_audio_embedding_from_data(
            x=samples[np.newaxis, :], use_tensor=False
        )[0]
        audio_emb = _l2_normalize(np.asarray(audio_emb, dtype=np.float32))

        genre, genre_conf = self._classify(audio_emb, self._genre_emb, GENRE_PROMPTS)
        mood, _ = self._classify(audio_emb, self._mood_emb, MOOD_PROMPTS)

        dyn = _dsp_dynamics(samples, sr)
        return EmbedResult(
            embedding=audio_emb.tolist(),
            dim=int(audio_emb.shape[0]),
            model_version=self.model_version,
            genre=genre,
            genre_confidence=genre_conf,
            mood=mood,
            energy=dyn["energy"],
            danceability=dyn["danceability"],
            bpm_estimate=dyn["bpm_estimate"],
        )


def build_embedder(prefer: str = "auto"):
    """Pick the best available backend.

    ``prefer`` may be "auto" (default), "clap" or "librosa".
    """
    if prefer in ("auto", "clap"):
        try:
            embedder = ClapEmbedder()
            logger.info("Using CLAP embedder (%s)", embedder.model_version)
            return embedder
        except Exception as exc:  # noqa: BLE001 - any failure -> fallback
            if prefer == "clap":
                raise
            logger.warning("CLAP unavailable (%s); falling back to librosa", exc)

    embedder = LibrosaEmbedder()
    logger.info("Using librosa DSP embedder (%s)", embedder.model_version)
    return embedder
