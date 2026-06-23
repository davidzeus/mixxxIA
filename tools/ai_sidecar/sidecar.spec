# PyInstaller spec for the Mixxx AI sidecar.
# Produces a standalone folder (onedir) so the target PC needs no Python.
#
# Build:  pyinstaller sidecar.spec --noconfirm
# Output: dist/mixxx-ai-sidecar/mixxx-ai-sidecar.exe

from PyInstaller.utils.hooks import collect_all, collect_submodules

datas = []
binaries = []
hiddenimports = []

# Packages that ship data files / dynamic submodules and need full collection.
_collect = [
    "laion_clap",
    "clap_module",
    "torch",
    "torchvision",
    "torchaudio",
    "transformers",
    "tokenizers",
    "huggingface_hub",
    "librosa",
    "soundfile",
    "soxr",
    "audioread",
    "pooch",
    "sklearn",
    "scipy",
    "numba",
    "llvmlite",
    "uvicorn",
    "fastapi",
    "starlette",
    "pydantic",
    "pydantic_core",
    "anyio",
    "regex",
]

for pkg in _collect:
    try:
        d, b, h = collect_all(pkg)
        datas += d
        binaries += b
        hiddenimports += h
    except Exception as exc:  # package may be absent; keep going
        print(f"[spec] skip {pkg}: {exc}")

# uvicorn loads its protocol/loop implementations dynamically.
hiddenimports += collect_submodules("uvicorn")

# Our own modules.
hiddenimports += ["server", "embedder", "llm"]

a = Analysis(
    ["sidecar_entry.py"],
    pathex=["."],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    runtime_hooks=[],
    excludes=["tkinter", "matplotlib", "PyQt5", "PyQt6", "PySide6"],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="mixxx-ai-sidecar",
    debug=False,
    strip=False,
    upx=False,
    console=True,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    name="mixxx-ai-sidecar",
)
