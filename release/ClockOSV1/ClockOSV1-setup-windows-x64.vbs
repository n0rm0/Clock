Option Explicit

Const TEMPORARY_FOLDER = 2
Const FOR_BINARY = 1
Const AD_SAVE_CREATE_OVERWRITE = 2

Dim sh, fso, localBat, bat, tempDir, remoteUrl
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
localBat = fso.BuildPath(fso.GetParentFolderName(WScript.ScriptFullName), "setup_sd.bat")

If fso.FileExists(localBat) Then
  bat = localBat
Else
  On Error Resume Next
  remoteUrl = "https://raw.githubusercontent.com/n0rm0/Clock/main/.source/install/setup_sd.bat"
  tempDir = fso.GetSpecialFolder(TEMPORARY_FOLDER).Path
  bat = fso.BuildPath(tempDir, "ClockOS_setup_" & CStr(Int(Timer * 1000)) & ".bat")

  Dim http, stream
  Set http = CreateObject("MSXML2.XMLHTTP")
  http.Open "GET", remoteUrl & "?t=" & CStr(Timer), False
  http.Send
  If Err.Number = 0 And http.Status = 200 Then
    Set stream = CreateObject("ADODB.Stream")
    stream.Type = FOR_BINARY
    stream.Open
    stream.Write http.ResponseBody
    stream.SaveToFile bat, AD_SAVE_CREATE_OVERWRITE
    stream.Close
  Else
    bat = ""
  End If
  On Error GoTo 0
End If

If Len(bat) = 0 Or Not fso.FileExists(bat) Then
  MsgBox "ClockOS Setup could not download its launcher. Check your internet connection and try again.", vbCritical, "ClockOS Setup"
  WScript.Quit 1
End If

' Run the batch through its hidden path. WScript itself has no console window.
sh.Run Chr(34) & bat & Chr(34) & " --clock-hidden", 0, False
