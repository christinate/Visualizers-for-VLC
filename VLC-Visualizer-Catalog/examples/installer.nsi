; Build after replacing the example payload with your own AMD64 renderer DLL.
Unicode true
!include MUI2.nsh
!include LogicLib.nsh
Name "Example VLC Visualizer"
OutFile "ExampleVisualizer-setup.exe"
InstallDir "$PROGRAMFILES64\VideoLAN\VLC"
RequestExecutionLevel admin
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE English
Section
  IfFileExists "$INSTDIR\visualizers\VisualizersCtl.exe" +2
    Abort "Install VLC Visualizer Addon first."
  SetOutPath "$INSTDIR\visualizers\plugins\example_visualizer"
  File "example-visualizer\manifest.json"
  File "example-visualizer\vis_example64.dll"
  ExecWait '"$INSTDIR\vlc.exe" --scan-visualizers' $0
  ${If} $0 != 0
    Abort "Registration failed. Close VLC and run vlc.exe --scan-visualizers."
  ${EndIf}
  WriteUninstaller "$INSTDIR\visualizers\plugins\example_visualizer\Uninstall.exe"
SectionEnd
Section Uninstall
  ; This uninstaller is inside VLC\visualizers\plugins\example_visualizer.
  GetFullPathName $R0 "$INSTDIR\..\..\.."
  Delete "$INSTDIR\manifest.json"
  Delete "$INSTDIR\vis_example64.dll"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  IfFileExists "$R0\visualizers\VisualizersCtl.exe" 0 +2
    ExecWait '"$R0\vlc.exe" --scan-visualizers'
SectionEnd
