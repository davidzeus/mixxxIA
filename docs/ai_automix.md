# Local-AI Automix for Mixxx

This feature adds **local, offline AI** to Mixxx so AutoDJ can chain tracks
that *sound* similar while respecting key and BPM — no cloud, no subscription.

## How it works

1. **Analysis** — when AI is enabled, each track is sent (during the normal
   library analysis) to a local **AI sidecar** which returns an audio
   *embedding* plus genre / mood / energy. This is stored in the
   `track_ai_features` table (schema v40).
2. **Selection** — when **AI Automix** is on, AutoDJ scores the tracks in its
   queue against the one currently playing and moves the best match to the
   top. Score = embedding cosine similarity + Camelot key compatibility +
   BPM closeness + energy continuity. Candidates whose BPM is too far apart
   are rejected. BPM / key / intro-outro come from Mixxx's existing analyzers.

```
Mixxx ──HTTP──> tools/ai_sidecar (FastAPI, localhost)
  AnalyzerAiFeatures            CLAP or librosa -> embedding + tags
  AiFeatureDao  -> SQLite (track_ai_features)
  AiAutomixSelector -> reorders the AutoDJ queue
```

## Setup

1. Start the sidecar (see `tools/ai_sidecar/README.md`):
   ```bash
   cd tools/ai_sidecar
   pip install -r requirements.txt          # + optional: laion-clap torch
   python -m uvicorn server:app --host 127.0.0.1 --port 8765
   ```
2. In Mixxx, enable **AI Automix** in the AutoDJ panel (this also turns on AI
   analysis).
3. **Analyze your library** (right-click → Analyze, or it runs on import).
   Embeddings are computed once per track and cached.
4. Add tracks to the AutoDJ queue and start AutoDJ. The next track is chosen
   by the AI scorer.

## Configuration (`[AI]` group in mixxx.cfg)

| Key              | Default | Meaning                                  |
|------------------|---------|------------------------------------------|
| `Enabled`        | false   | Master switch (analysis + automix)       |
| `AnalyzeEnabled` | true    | Compute embeddings during analysis       |
| `AutomixEnabled` | true    | AI-driven next-track selection           |
| `SidecarUrl`     | http://127.0.0.1:8765 | Sidecar base URL           |
| `MaxBpmFraction` | 0.08    | Max BPM gap (8%) for a valid candidate   |
| `WeightEmbedding`| 0.5     | Scorer weight: sound similarity          |
| `WeightKey`      | 0.2     | Scorer weight: harmonic key compat       |
| `WeightBpm`      | 0.2     | Scorer weight: BPM closeness             |
| `WeightEnergy`   | 0.1     | Scorer weight: energy continuity         |

## Why local (Ollama vs cloud)?

LLMs (Ollama / Claude / Gemini) don't turn audio into music embeddings, so the
embeddings come from a dedicated local audio model (CLAP/Essentia). Next-track
selection is deterministic (cosine + Camelot + BPM), which is faster, free and
private. An optional Ollama layer for natural-language requests
("now something darker") is left as future work (Phase 5).

## Status

- [x] Phase 0 — AI sidecar (CLAP + librosa fallback)
- [x] Phase 1 — `track_ai_features` schema v40 + `AiFeatureDao`
- [x] Phase 2 — `AnalyzerAiFeatures` populates embeddings during analysis
- [x] Phase 3 — `AiAutomixSelector` reorders the AutoDJ queue
- [x] Phase 4 — AI Automix toggle in the AutoDJ panel
- [ ] Phase 5 — optional Ollama re-ranking / natural-language requests
