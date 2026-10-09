Option Explicit

Dim sh, fso, bat, cmd
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
bat = fso.BuildPath(fso.GetParentFolderName(WScript.ScriptFullName), "setup_sd.bat")
If Not fso.FileExists(bat) Then
  MsgBox "ClockOS Setup could not find setup_sd.bat.", vbCritical, "ClockOS Setup"
  WScript.Quit 1
End If

' Run the batch through its hidden path. WScript itself has no console window.
cmd = Chr(34) & bat & Chr(34) & " --clock-hidden"
sh.Run cmd, 0, False
