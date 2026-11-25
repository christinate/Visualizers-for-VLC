Unicode true
!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include Sections.nsh
!include FileFunc.nsh
!ifndef PAYLOAD_DIR
  !error "Provide /DPAYLOAD_DIR for the fresh native staging directory"
!endif
!ifndef OUTPUT_FILE
  !error "Provide /DOUTPUT_FILE for a new candidate artifact"
!endif
SetCompressor /SOLID lzma
Var Components
Name "VLC Visualizer Addon"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\VideoLAN\VLC"
InstallDirRegKey HKLM "Software\VideoLAN\VLC" "InstallDir"
RequestExecutionLevel admin
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English
Section "VLC Visualizer Addon (required)" SEC_CORE
  SectionIn RO
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\payload"
  File /r "${PAYLOAD_DIR}\*"
  Call GetSelectedComponents
  nsExec::ExecToStack '"$PLUGINSDIR\payload\bootstrap\VisualizersCtl.exe" running --root "$INSTDIR"'
  Pop $0
  Pop $1
  ${If} $0 == "10"
    StrCpy $2 "no"
    ${IfNot} ${Silent}
      MessageBox MB_OKCANCEL|MB_ICONQUESTION "VLC must close before setup can continue. Save any work first. Setup will ask VLC to quit and close it forcibly if it does not respond." IDOK +2
      Abort "Setup cancelled while VLC was open."
      StrCpy $2 "yes"
    ${EndIf}
    nsExec::ExecToStack '"$PLUGINSDIR\payload\bootstrap\VisualizersCtl.exe" close-vlc --root "$INSTDIR" --force-close "$2"'
    Pop $0
    Pop $1
  ${EndIf}
  ${If} $0 != "0"
    DetailPrint "$1"
    MessageBox MB_OK|MB_ICONSTOP "Setup could not close VLC.$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort "Close VLC and retry setup."
  ${EndIf}
  nsExec::ExecToStack '"$PLUGINSDIR\payload\bootstrap\VisualizersCtl.exe" patch --root "$INSTDIR" --payload "$PLUGINSDIR\payload" --components "$Components" --recover yes'
  Pop $0
  Pop $1
  ${If} $0 != "0"
    ${If} $0 == "error"
      StrCpy $1 "Windows could not start the VLC setup helper."
    ${ElseIf} $1 == ""
      StrCpy $1 "The VLC setup helper exited with code $0."
    ${EndIf}
    DetailPrint "$1"
    MessageBox MB_OK|MB_ICONSTOP "VLC could not be patched.$\r$\n$\r$\n$1" /SD IDOK
    SetErrorLevel 1
    Abort "The VLC patch step failed. See the setup details for the reason."
  ${EndIf}
  DetailPrint "$1"
  ; Populate the persistent menu before the first launch. The inline interface
  ; reads this catalog; it does not discover bundled plugins by itself.
  ExecWait '"$PLUGINSDIR\payload\bootstrap\VisualizersCtl.exe" scan --suite "$INSTDIR\visualizers"' $0
  ${If} $0 != 0
    ExecWait '"$PLUGINSDIR\payload\bootstrap\VisualizersCtl.exe" restore --root "$INSTDIR"' $1
    ${If} $1 != 0
      Abort "The visualizer list could not be created and restoration could not finish. Close VLC and retain the verified backups for repair."
    ${EndIf}
    Abort "The visualizer list could not be created. Original VLC files have been restored."
  ${EndIf}
  SetOutPath "$INSTDIR\visualizers"
  File /oname=uninstall-control.exe "${PAYLOAD_DIR}\bootstrap\VisualizersCtl.exe"
  WriteUninstaller "$INSTDIR\visualizers\Uninstall-Addon.exe"
  SetRegView 64
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon" "DisplayName" "VLC Visualizer Addon"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon" "UninstallString" '"$INSTDIR\visualizers\Uninstall-Addon.exe"'
SectionEnd
Section "Prism Flow (includes Geiss Classic)" SEC_GEISS
SectionEnd
Section "Rhythm Terrain" SEC_TERRAIN
SectionEnd
Section "Beach" SEC_BEACH
SectionEnd
Section "Objects" SEC_OBJECTS
SectionEnd
Section "UnderSea" SEC_UNDERSEA
SectionEnd
Section "CoralCanyon" SEC_CORALCANYON
SectionEnd
Section "MilkDrop 2 64-bit" SEC_MILKDROP
SectionEnd
!ifdef INCLUDE_DUALSUBS
Section /o "DualSubs for VLC" SEC_DUALSUBS
SectionEnd
!endif
!macro AddSelectedComponent SECTION_ID COMPONENT_ID
  ${If} ${SectionIsSelected} ${SECTION_ID}
    ${If} $Components == ""
      StrCpy $Components "${COMPONENT_ID}"
    ${Else}
      StrCpy $Components "$Components,${COMPONENT_ID}"
    ${EndIf}
  ${EndIf}
!macroend
Function GetSelectedComponents
  StrCpy $Components ""
  !insertmacro AddSelectedComponent ${SEC_GEISS} geiss
  !insertmacro AddSelectedComponent ${SEC_TERRAIN} terrain
  !insertmacro AddSelectedComponent ${SEC_BEACH} beach
  !insertmacro AddSelectedComponent ${SEC_OBJECTS} objects
  !insertmacro AddSelectedComponent ${SEC_UNDERSEA} undersea
  !insertmacro AddSelectedComponent ${SEC_CORALCANYON} coralcanyon
  !insertmacro AddSelectedComponent ${SEC_MILKDROP} milkdrop
  !ifdef INCLUDE_DUALSUBS
    !insertmacro AddSelectedComponent ${SEC_DUALSUBS} dualsubs
  !endif
FunctionEnd
Function .onInit
  ${IfNot} ${RunningX64}
    Abort "This release requires 64-bit Windows and VLC 3.0.24 AMD64."
  ${EndIf}
  SetRegView 64
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTGEISS" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_GEISS}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTTERRAIN" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_TERRAIN}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTBEACH" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_BEACH}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTOBJECTS" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_OBJECTS}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTUNDERSEA" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_UNDERSEA}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTCORALCANYON" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_CORALCANYON}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTMILKDROP" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_MILKDROP}
  ${EndIf}
  !ifdef INCLUDE_DUALSUBS
    ClearErrors
    ${GetOptions} $R0 "/WITHDUALSUBS" $R1
    ${IfNot} ${Errors}
      !insertmacro SelectSection ${SEC_DUALSUBS}
    ${EndIf}
  !endif
FunctionEnd
Section Uninstall
  ; $INSTDIR is the directory containing this uninstaller (VLC\visualizers).
  GetFullPathName $R0 "$INSTDIR\.."
  nsExec::ExecToStack '"$INSTDIR\uninstall-control.exe" running --root "$R0"'
  Pop $0
  Pop $1
  ${If} $0 == "10"
    StrCpy $2 "no"
    ${IfNot} ${Silent}
      MessageBox MB_OKCANCEL|MB_ICONQUESTION "VLC must close before uninstalling. Save any work first." IDOK +2
      Abort "Uninstall cancelled while VLC was open."
      StrCpy $2 "yes"
    ${EndIf}
    nsExec::ExecToStack '"$INSTDIR\uninstall-control.exe" close-vlc --root "$R0" --force-close "$2"'
    Pop $0
    Pop $1
  ${EndIf}
  ${If} $0 != "0"
    Abort "Close VLC and retry uninstall. $1"
  ${EndIf}
  nsExec::ExecToStack '"$INSTDIR\uninstall-control.exe" restore --root "$R0"'
  Pop $0
  Pop $1
  ${If} $0 != "0"
    Abort "The VLC files could not be restored. $1"
  ${EndIf}
  Delete "$INSTDIR\installed-visualizers.json"
  Delete "$INSTDIR\uninstall-control.exe"
  Delete "$INSTDIR\Uninstall-Addon.exe"
  SetRegView 64
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon"
  RMDir "$INSTDIR"
SectionEnd
