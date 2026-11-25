; Plugin-only example. Never ships or overwrites the central native runtime.
Unicode true
!include MUI2.nsh
!include LogicLib.nsh
!ifndef PACKAGE_DIR
  !error "Build the example first and define PACKAGE_DIR to build/package/audio_color"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "AudioColorExample-setup.exe"
!endif
Name "Audio Color Example"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\VideoLAN\VLC"
RequestExecutionLevel admin
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

Section
  IfFileExists "$INSTDIR\visualizers\native\VisualizerNative.exe" +2
    Abort "Install the native Dawn version of VLC Visualizer Addon first."
  IfFileExists "$INSTDIR\visualizers\VisualizersCtl.exe" +2
    Abort "The visualizer scanner is missing."
  IfFileExists "$INSTDIR\visualizers\plugins\audio_color\*.*" 0 +2
    Abort "The audio_color package directory is already in use. Uninstall its owner before upgrading."
  SetOutPath "$INSTDIR\visualizers\plugins\audio_color"
  File "${PACKAGE_DIR}\manifest.json"
  File "${PACKAGE_DIR}\audio_color.dll"
  File "${PACKAGE_DIR}\LICENSE.txt"
  SetOutPath "$INSTDIR\visualizers\plugins\audio_color\assets"
  File "${PACKAGE_DIR}\assets\README.txt"
  ExecWait '"$INSTDIR\visualizers\VisualizersCtl.exe" check --suite "$INSTDIR\visualizers" --package "$INSTDIR\visualizers\plugins\audio_color"' $0
  ${If} $0 != 0
    Delete "$INSTDIR\visualizers\plugins\audio_color\assets\README.txt"
    Delete "$INSTDIR\visualizers\plugins\audio_color\manifest.json"
    Delete "$INSTDIR\visualizers\plugins\audio_color\audio_color.dll"
    Delete "$INSTDIR\visualizers\plugins\audio_color\LICENSE.txt"
    RMDir "$INSTDIR\visualizers\plugins\audio_color\assets"
    RMDir "$INSTDIR\visualizers\plugins\audio_color"
    Abort "This package is incompatible with the installed native runtime."
  ${EndIf}
  WriteUninstaller "$INSTDIR\visualizers\plugins\audio_color\Uninstall.exe"
  ExecWait '"$INSTDIR\vlc.exe" --scan-visualizers' $0
  ${If} $0 != 0
    MessageBox MB_ICONEXCLAMATION "The package was installed, but scanning failed. Close VLC and run vlc.exe --scan-visualizers."
  ${EndIf}
SectionEnd

; Compare every owned file against bytes embedded in this exact uninstaller
; before deleting anything. This small example uses NSIS byte reads; a larger
; product should use its install journal and a native streaming hash helper.
Function un.CheckOwnedFile
  Pop $0
  IfFileExists "$INSTDIR\$0" 0 done
  ClearErrors
  FileOpen $1 "$INSTDIR\$0" r
  IfErrors open_error
  FileOpen $2 "$PLUGINSDIR\original\$0" r
  IfErrors expected_error
  loop:
    ClearErrors
    FileReadByte $1 $3
    IfErrors actual_end
    FileReadByte $2 $4
    IfErrors different
    IntCmp $3 $4 loop different different
  actual_end:
    ClearErrors
    FileReadByte $2 $4
    IfErrors equal different
  equal:
    FileClose $1
    FileClose $2
    Goto done
  different:
    FileClose $1
    FileClose $2
    MessageBox MB_ICONSTOP "Modified installed file preserved: $INSTDIR\$0. No package files were removed."
    Abort
  expected_error:
    FileClose $1
  open_error:
    MessageBox MB_ICONSTOP "Could not compare $INSTDIR\$0. No package files were removed."
    Abort
  done:
FunctionEnd

Section Uninstall
  ; NSIS sets $INSTDIR to this uninstaller's package directory.
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\original"
  File "${PACKAGE_DIR}\manifest.json"
  File "${PACKAGE_DIR}\audio_color.dll"
  File "${PACKAGE_DIR}\LICENSE.txt"
  SetOutPath "$PLUGINSDIR\original\assets"
  File "${PACKAGE_DIR}\assets\README.txt"
  Push "manifest.json"
  Call un.CheckOwnedFile
  Push "audio_color.dll"
  Call un.CheckOwnedFile
  Push "LICENSE.txt"
  Call un.CheckOwnedFile
  Push "assets\README.txt"
  Call un.CheckOwnedFile
  GetFullPathName $R0 "$INSTDIR\..\..\.."
  SetOutPath "$TEMP"
  Delete "$INSTDIR\assets\README.txt"
  Delete "$INSTDIR\manifest.json"
  Delete "$INSTDIR\audio_color.dll"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR\assets"
  RMDir "$INSTDIR"
  IfFileExists "$R0\visualizers\VisualizersCtl.exe" 0 +2
    ExecWait '"$R0\vlc.exe" --scan-visualizers'
SectionEnd
