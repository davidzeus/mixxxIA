@ECHO OFF
REM Launch the Mixxx AI sidecar (in its own window so first-run setup progress
REM is visible) and then Mixxx.
START "Mixxx AI Sidecar" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sidecar\run_sidecar.ps1"
START "" "%~dp0app\mixxx.exe"
