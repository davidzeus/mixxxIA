"""Frozen-app entry point for the Mixxx AI sidecar.

PyInstaller bundles this into a standalone executable so the target PC does not
need Python installed. It starts the same FastAPI app used in development.

Host/port can be overridden via the MIXXX_AI_HOST / MIXXX_AI_PORT env vars.
"""

import os
import sys

# The embeddable Python distribution runs in isolated mode and does not add the
# script's own directory to sys.path, so do it explicitly before importing the
# sidecar modules (server/embedder/llm live next to this file).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import uvicorn

from server import app

if __name__ == "__main__":
    host = os.environ.get("MIXXX_AI_HOST", "127.0.0.1")
    port = int(os.environ.get("MIXXX_AI_PORT", "8765"))
    uvicorn.run(app, host=host, port=port, log_level="info")
