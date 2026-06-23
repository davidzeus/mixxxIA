# Mixxx AI sidecar

Local HTTP service that converts audio tracks into **embeddings + tags**
(genre, mood, energy, danceability) for Mixxx's AI Automix feature.

It runs entirely on your machine — no cloud, no subscription. Mixxx calls it
over `localhost` and stores the returned embeddings in its library database
(via `sqlite-vec`) so it can later find tracks that *sound* similar.

## Why a separate Python service?

The high-quality audio models (CLAP / Essentia) are part of the
PyTorch/TensorFlow ecosystem and cannot reasonably be linked into the Mixxx
C++ binary. Running them in a small sidecar keeps Mixxx unchanged at build
time and lets the model stack evolve independently. Mixxx reuses its existing
async HTTP machinery (`src/network/jsonwebtask.cpp`) to talk to it.

## Backends

| Backend  | Quality | Deps                | Genre/mood |
|----------|---------|---------------------|------------|
| CLAP     | High    | `laion-clap`,`torch`| yes (zero-shot) |
| librosa  | Basic   | `librosa` only      | no (null)  |

The service auto-selects CLAP if available, otherwise falls back to the
librosa DSP embedder so the pipeline works even without the heavy ML stack.
Force a backend with `MIXXX_AI_BACKEND=clap|librosa|auto`.

## Install & run

```bash
cd tools/ai_sidecar
python -m venv .venv && . .venv/bin/activate     # Windows: .venv\Scripts\activate
pip install -r requirements.txt                  # basic (librosa) backend
pip install laion-clap torch                     # optional: high-quality CLAP

python -m uvicorn server:app --host 127.0.0.1 --port 8765
```

## API

```
GET  /health
  -> {"status":"ok","backend":"clap-htsat-unfused-v1","target_sample_rate":48000}

POST /embed
  body: {"path": "C:/Music/track.flac"}
  -> {"embedding":[...], "dim":512, "model_version":"...",
      "genre":"techno", "genre_confidence":0.82, "mood":"dark",
      "energy":0.74, "danceability":0.66, "bpm_estimate":128.0}

POST /embed_batch
  body: {"items":[{"path":"a.flac"},{"path":"b.mp3"}]}
  -> {"results":[{...},{...}], "errors":[null,null]}
```

Tracks can also be sent as raw mono samples:
`{"samples":[...], "sample_rate":44100}`.
