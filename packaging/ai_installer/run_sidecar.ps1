# Mixxx AI sidecar launcher + first-run dependency bootstrap.
#
# Lives next to the bundled embeddable Python ("python\") and the sidecar
# sources. On first run it installs the AI dependencies, picking the CUDA
# build of torch when an NVIDIA GPU is present and the CPU build otherwise,
# so the installer itself stays small and only downloads what the machine
# needs. Subsequent runs skip straight to starting the server.

$ErrorActionPreference = "Stop"
$base = $PSScriptRoot
$py = Join-Path $base "python\python.exe"
$marker = Join-Path $base ".deps_installed"

if (-not (Test-Path $marker)) {
    Write-Host "==================================================================="
    Write-Host " Mixxx AI: first-time setup (one time only, may take a few minutes)"
    Write-Host "==================================================================="

    & $py (Join-Path $base "get-pip.py") --no-warn-script-location
    & $py -m pip install --no-warn-script-location -r (Join-Path $base "requirements.txt")

    $hasGpu = $null -ne (Get-Command nvidia-smi -ErrorAction SilentlyContinue)
    if ($hasGpu) {
        Write-Host "NVIDIA GPU detected -> installing CUDA build of torch (GPU)"
        & $py -m pip install --no-warn-script-location torch torchvision --index-url https://download.pytorch.org/whl/cu124
    } else {
        Write-Host "No NVIDIA GPU detected -> installing CPU build of torch"
        & $py -m pip install --no-warn-script-location torch torchvision
    }
    & $py -m pip install --no-warn-script-location laion-clap

    New-Item -ItemType File -Path $marker | Out-Null
    Write-Host "=== AI setup complete ==="
}

Set-Location $base
& $py (Join-Path $base "sidecar_entry.py")
