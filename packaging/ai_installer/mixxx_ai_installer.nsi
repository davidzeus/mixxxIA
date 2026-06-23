; NSIS installer for Mixxx AI (Mixxx + local-AI Automix sidecar).
; Build with:
;   makensis /DMIXXX_APP=<path> /DSIDECAR=<path> mixxx_ai_installer.nsi
; or rely on the defaults below.

Unicode true
SetCompressor /SOLID lzma

!ifndef MIXXX_APP
  !define MIXXX_APP "E:\Proyectos\IADJ\mixxx\dist\mixxx_app"
!endif
!ifndef SIDECAR
  !define SIDECAR "E:\Proyectos\IADJ\mixxx\tools\ai_sidecar\dist_frozen\mixxx-ai-sidecar"
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
!define MUI_FINISHPAGE_RUN "$WINDIR\System32\wscript.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS '"$INSTDIR\launch_mixxx_ai.vbs"'
!define MUI_FINISHPAGE_RUN_TEXT "Launch Mixxx AI now"
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
  File "${EXTRA}\launch_mixxx_ai.vbs"

  CreateDirectory "$SMPROGRAMS\Mixxx AI"
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Mixxx AI.lnk" "$WINDIR\System32\wscript.exe" '"$INSTDIR\launch_mixxx_ai.vbs"' "$INSTDIR\app\mixxx.exe" 0
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Mixxx (no sidecar).lnk" "$INSTDIR\app\mixxx.exe"
  CreateShortcut "$SMPROGRAMS\Mixxx AI\AI Sidecar.lnk" "$INSTDIR\sidecar\mixxx-ai-sidecar.exe"
  CreateShortcut "$DESKTOP\Mixxx AI.lnk" "$WINDIR\System32\wscript.exe" '"$INSTDIR\launch_mixxx_ai.vbs"' "$INSTDIR\app\mixxx.exe" 0

  WriteRegStr HKLM "Software\MixxxAI" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Mixxx AI\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "DisplayName" "Mixxx AI"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI" "DisplayIcon" "$INSTDIR\app\mixxx.exe"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Mixxx AI.lnk"
  RMDir /r "$SMPROGRAMS\Mixxx AI"
  RMDir /r "$INSTDIR\app"
  RMDir /r "$INSTDIR\sidecar"
  Delete "$INSTDIR\launch_mixxx_ai.vbs"
  Delete "$INSTDIR\Uninstall.exe"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MixxxAI"
  DeleteRegKey HKLM "Software\MixxxAI"
  RMDir "$INSTDIR"
SectionEnd
