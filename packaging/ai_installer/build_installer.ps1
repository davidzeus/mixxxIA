# Build the single Mixxx AI installer (Mixxx app + lightweight AI sidecar) with NSIS.
#
# The sidecar payload bundles an embeddable Python but NOT torch/CLAP: those are
# installed on the target's first launch, GPU-aware (CUDA if an NVIDIA GPU is
# present, else CPU). This keeps the installer small.
#
# Prerequisites:
#   * Mixxx already built in <repo>\build  (run build_ai_windows.bat first)
#   * NSIS installed (makensis.exe)
#   * Internet access (to fetch the embeddable Python + get-pip)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File packaging\ai_installer\build_installer.ps1
#
# Result: dist\MixxxAI-Setup.exe

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$build = Join-Path $repo "build"
$dist = Join-Path $repo "dist"
$stage = Join-Path $dist "mixxx_app"
$payload = Join-Path $dist "sidecar_payload"
$sidecarSrc = Join-Path $repo "tools\ai_sidecar"
$pyEmbedUrl = "https://www.python.org/ftp/python/3.13.2/python-3.13.2-embed-amd64.zip"

# --- locate cmake (VS-bundled) ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }

# --- locate makensis ---
$makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
if (-not (Test-Path $makensis)) { $makensis = "${env:ProgramFiles}\NSIS\makensis.exe" }
if (-not (Test-Path $makensis)) { throw "makensis.exe not found - install NSIS" }

Write-Host "== 1/4 Staging Mixxx app =="
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
& $cmake --install $build --prefix $stage
# cmake install rules miss the vcpkg DLLs that are deployed next to mixxx.exe
# in the build tree (chromaprint, sqlite3, djinterop, EBUR128, FLAC, ...).
Copy-Item "$build\*.dll" $stage -Force

Write-Host "== 2/4 Building lightweight sidecar payload (embeddable Python, no torch) =="
if (Test-Path $payload) { Remove-Item -Recurse -Force $payload }
$pydir = Join-Path $payload "python"
New-Item -ItemType Directory -Force -Path $pydir | Out-Null
$zip = Join-Path $env:TEMP "py-embed-3.13.2.zip"
if (-not (Test-Path $zip)) { Invoke-WebRequest -Uri $pyEmbedUrl -OutFile $zip }
Expand-Archive -Path $zip -DestinationPath $pydir -Force
# Enable site so pip works inside the embeddable distribution.
$pth = Get-ChildItem "$pydir\python*._pth" | Select-Object -First 1
(Get-Content $pth.FullName) -replace '^#\s*import site', 'import site' | Set-Content $pth.FullName
Invoke-WebRequest -Uri "https://bootstrap.pypa.io/get-pip.py" -OutFile (Join-Path $payload "get-pip.py")
foreach ($f in "server.py", "embedder.py", "llm.py", "sidecar_entry.py", "requirements.txt") {
    Copy-Item (Join-Path $sidecarSrc $f) $payload -Force
}
Copy-Item (Join-Path $PSScriptRoot "run_sidecar.ps1") $payload -Force

Write-Host "== 3/4 Checking NSIS launcher =="
if (-not (Test-Path (Join-Path $PSScriptRoot "start_mixxx_ai.bat"))) { throw "start_mixxx_ai.bat missing" }

Write-Host "== 4/4 Building installer with NSIS =="
& $makensis "/DMIXXX_APP=$stage" "/DSIDECAR=$payload" "/DEXTRA=$PSScriptRoot" (Join-Path $PSScriptRoot "mixxx_ai_installer.nsi")

Write-Host ""
Write-Host "Done -> $dist\MixxxAI-Setup.exe"
