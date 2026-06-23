; NSIS installer for Mixxx AI (Mixxx + local-AI Automix sidecar).
;
; The sidecar ships as a small embeddable-Python payload (no torch/CLAP bundled).
; On first launch it installs the right dependencies for the machine: the CUDA
; build of torch when an NVIDIA GPU is present, the CPU build otherwise. This
; keeps the installer small and avoids shipping CUDA to GPU-less PCs.
;
; Build via packaging/ai_installer/build_installer.ps1 (recommended), or:
;   makensis /DMIXXX_APP=<path> /DSIDECAR=<path> /DEXTRA=<dir> mixxx_ai_installer.nsi

Unicode true
SetCompressor /SOLID lzma

!ifndef MIXXX_APP
  !define MIXXX_APP "E:\Proyectos\IADJ\mixxx\dist\mixxx_app"
!endif
!ifndef SIDECAR
  !define SIDECAR "E:\Proyectos\IADJ\mixxx\dist\sidecar_payload"
!endif
!ifndef EXTRA
  !define EXTRA "E:\Proyectos\IADJ\mixxx\packaging\ai_installer"
!endif

Name "Mixxx AI"
OutFile "E:\Proyectos\IADJ\mixxx\dist\MixxxAI-Setup.exe"
InstallDir "$PROGRAMFILES64\Mixxx AI"
InstallDirRegKey HKLM "Software\MixxxAI" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\start_mixxx_ai.bat"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Mixxx AI now (first launch sets up the AI engine)"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Mixxx application" SecApp
  SectionIn RO
  SetOutPath "$INSTDIR\app"
  File /r "${MIXXX_APP}\*.*"
SectionEnd

Section "AI sidecar (local genre/embeddings + natural language)" SecSidecar
  SetOutPath "$INSTDIR\sidecar"
  File /r "${SIDECAR}\*.*"
SectionEnd

Section "-Launcher & shortcuts"
  SetOutPath "$INSTDIR"
  File "${EXTRA}\start_mixxx_ai.bat"

  CreateDirectory "$SMPROGRAMS\Mixxx AI"
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Mixxx AI.lnk" "$INSTDIR\start_mixxx_ai.bat" "" "$INSTDIR\app\mixxx.exe" 0
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Mixxx (no AI).lnk" "$INSTDIR\app\mixxx.exe"
  CreateShortcut "$DESKTOP\Mixxx AI.lnk" "$INSTDIR\start_mixxx_ai.bat" "" "$INSTDIR\app\mixxx.exe" 0

  WriteRegStr HKLM "Software\MixxxAI" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "DisplayName" "Mixxx AI"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "DisplayIcon" "$INSTDIR\app\mixxx.exe"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Mixxx AI.lnk"
  RMDir /r "$SMPROGRAMS\Mixxx AI"
  RMDir /r "$INSTDIR\app"
  RMDir /r "$INSTDIR\sidecar"
  Delete "$INSTDIR\start_mixxx_ai.bat"
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI"
  DeleteRegKey HKLM "Software\MixxxAI"
  RMDir "$INSTDIR"
SectionEnd
