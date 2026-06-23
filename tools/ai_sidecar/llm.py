"""Natural-language genre resolution for Mixxx AI Automix (Phase 5).

Given a free-text request ("switch to reggaeton", "something darker and
slower") plus the genres actually present in the user's library, an LLM maps
it to ONE target genre from that list. The result is always constrained to the
available genres so Mixxx can never be asked for a genre it doesn't have.

Providers:
* local      -> Ollama (http://localhost:11434), model auto-pulled if missing
* google     -> Gemini generateContent REST API
* openai     -> Chat Completions API
* anthropic  -> Messages API

Only the network call differs per provider; the prompt is shared.
"""

from __future__ import annotations

import json
import logging
import os
from typing import Optional

import requests

logger = logging.getLogger("ai_sidecar.llm")

OLLAMA_URL = os.environ.get("OLLAMA_URL", "http://localhost:11434")

DEFAULT_MODELS = {
    "local": "llama3.2",
    "google": "gemini-1.5-flash",
    "openai": "gpt-4o-mini",
    "anthropic": "claude-haiku-4-5",
}

_PROMPT = """You are a DJ assistant. The user is mixing music and wants to \
steer the next track.

Currently playing genre: {current}
Genres available in the library (you MUST pick exactly one of these): {genres}

User request: "{request}"

Choose the single best target genre from the available list that satisfies the \
request. If the request doesn't clearly map to any, pick the closest available \
genre. Respond ONLY with compact JSON:
{{"target_genre": "<one of the available genres, verbatim>", "confidence": <0..1>, "reasoning": "<short>"}}"""


def _build_prompt(request: str, current: str, genres: list[str]) -> str:
    return _PROMPT.format(
        current=current or "unknown",
        genres=", ".join(genres) if genres else "(none)",
        request=request,
    )


def _match_genre(value: str, genres: list[str]) -> Optional[str]:
    """Snap an LLM answer to one of the available genres (case-insensitive)."""
    if not value:
        return None
    lowered = value.strip().lower()
    for g in genres:
        if g.lower() == lowered:
            return g
    # Loose containment fallback.
    for g in genres:
        if g.lower() in lowered or lowered in g.lower():
            return g
    return None


def _extract_json(text: str) -> dict:
    text = text.strip()
    start = text.find("{")
    end = text.rfind("}")
    if start >= 0 and end > start:
        try:
            return json.loads(text[start : end + 1])
        except json.JSONDecodeError:
            pass
    return {}


# --- Provider calls: each returns the raw model text ---------------------

def _call_ollama(prompt: str, model: str, timeout: int) -> str:
    def _generate() -> requests.Response:
        return requests.post(
            f"{OLLAMA_URL}/api/generate",
            json={"model": model, "prompt": prompt, "stream": False,
                  "format": "json"},
            timeout=timeout,
        )

    resp = _generate()
    if resp.status_code == 404:
        # Model not present locally -> pull it automatically, then retry.
        logger.info("Ollama model '%s' missing; pulling...", model)
        requests.post(f"{OLLAMA_URL}/api/pull", json={"name": model},
                      timeout=max(timeout, 1800))
        resp = _generate()
    resp.raise_for_status()
    return resp.json().get("response", "")


def _call_google(prompt: str, model: str, api_key: str, timeout: int) -> str:
    url = (f"https://generativelanguage.googleapis.com/v1beta/models/"
           f"{model}:generateContent?key={api_key}")
    resp = requests.post(
        url,
        json={"contents": [{"parts": [{"text": prompt}]}],
              "generationConfig": {"responseMimeType": "application/json"}},
        timeout=timeout,
    )
    resp.raise_for_status()
    data = resp.json()
    return data["candidates"][0]["content"]["parts"][0]["text"]


def _call_openai(prompt: str, model: str, api_key: str, timeout: int) -> str:
    resp = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={"Authorization": f"Bearer {api_key}"},
        json={"model": model,
              "messages": [{"role": "user", "content": prompt}],
              "response_format": {"type": "json_object"}},
        timeout=timeout,
    )
    resp.raise_for_status()
    return resp.json()["choices"][0]["message"]["content"]


def _call_anthropic(prompt: str, model: str, api_key: str, timeout: int) -> str:
    resp = requests.post(
        "https://api.anthropic.com/v1/messages",
        headers={"x-api-key": api_key,
                 "anthropic-version": "2023-06-01"},
        json={"model": model, "max_tokens": 256,
              "messages": [{"role": "user", "content": prompt}]},
        timeout=timeout,
    )
    resp.raise_for_status()
    return resp.json()["content"][0]["text"]


def resolve_genre(request: str,
        current_genre: str,
        available_genres: list[str],
        provider: str = "local",
        api_key: str = "",
        model: str = "",
        timeout: int = 60) -> dict:
    """Map a free-text request to one of available_genres.

    Returns {"target_genre": str|None, "confidence": float, "reasoning": str}.
    Raises on transport/provider errors so the caller can report them.
    """
    provider = (provider or "local").lower()
    model = model or DEFAULT_MODELS.get(provider, DEFAULT_MODELS["local"])
    prompt = _build_prompt(request, current_genre, available_genres)

    if provider == "local":
        raw = _call_ollama(prompt, model, timeout)
    elif provider == "google":
        raw = _call_google(prompt, model, api_key, timeout)
    elif provider == "openai":
        raw = _call_openai(prompt, model, api_key, timeout)
    elif provider == "anthropic":
        raw = _call_anthropic(prompt, model, api_key, timeout)
    else:
        raise ValueError(f"unknown LLM provider: {provider}")

    parsed = _extract_json(raw)
    target = _match_genre(parsed.get("target_genre", ""), available_genres)
    return {
        "target_genre": target,
        "confidence": float(parsed.get("confidence", 0.0) or 0.0),
        "reasoning": str(parsed.get("reasoning", "")),
    }
