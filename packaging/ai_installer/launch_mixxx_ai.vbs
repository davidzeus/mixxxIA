' Mixxx AI launcher.
' Starts the local AI sidecar (hidden) and then Mixxx. Installed next to the
' "app" and "sidecar" folders by the installer.
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
base = fso.GetParentFolderName(WScript.ScriptFullName)

sidecar = base & "\sidecar\mixxx-ai-sidecar.exe"
mixxx = base & "\app\mixxx.exe"

' Launch the sidecar hidden (window style 0) if present; do not wait.
If fso.FileExists(sidecar) Then
    sh.Run """" & sidecar & """", 0, False
End If

' Launch Mixxx (normal window); do not wait.
sh.Run """" & mixxx & """", 1, False
