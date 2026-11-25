Unicode true
!include MUI2.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include Sections.nsh
!include x64.nsh
!ifndef PAYLOAD_DIR
  !error "Provide /DPAYLOAD_DIR for a fresh native payload"
!endif
!ifndef OUTPUT_FILE
  !error "Provide /DOUTPUT_FILE for a new candidate artifact"
!endif
SetCompressor /SOLID lzma
CRCCheck force
Name "${DISPLAY_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\${PACKAGE_NAME}"
RequestExecutionLevel admin
Var VLCROOT
Var WINAMPROOT
Var MODE
Var Components
Var PackageInstalled
Var RegistrationWritten
Var InstallMutex
!macro AcquirePackageMutex
  ; Keep the handle until this process exits, including silent installs.
  System::Call 'kernel32::SetLastError(i 0)'
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "Global\VLCVisualizers.Package.${VIS_ID}") p.r9 ?e'
  Pop $R8
  StrCpy $InstallMutex $R9
  ${If} $R9 == 0
  ${OrIf} $R8 == 183
    SetErrorLevel 1
    Abort "Another installer or uninstaller is using this visualizer. Wait for it to finish."
  ${EndIf}
!macroend
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

Section "Standalone visualizer and audio input selector" SEC_STANDALONE
SectionEnd
Section "VLC visualizer (native addon required)" SEC_VLC
SectionEnd
!ifdef NO_WINAMP
Section /o "-Winamp visualizer bridge" SEC_WINAMP
!else
Section "Winamp visualizer bridge" SEC_WINAMP
!endif
SectionEnd
Section "-Install selected components" SEC_INSTALL
  SectionIn RO
  SetShellVarContext all
  SetRegView 64
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\payload"
  File /r "${PAYLOAD_DIR}\*"
  Call GetSelectedComponents
  IfFileExists "$INSTDIR\Uninstall.exe" 0 +3
    SetErrorLevel 1
    Abort "This installation directory already has an uninstaller. Uninstall its owner before upgrading."
  ${If} ${SectionIsSelected} ${SEC_WINAMP}
    ReadRegStr $R0 HKLM "Software\VLCVisualizers\Renderers\${VIS_ID}" "Host"
    ${If} $R0 != ""
      SetErrorLevel 1
      Abort "A Winamp host is already registered for this visualizer. Uninstall its owner before upgrading."
    ${EndIf}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SEC_STANDALONE}
    ClearErrors
    SetOutPath "$PLUGINSDIR\payload\shortcuts"
    CreateShortcut "$PLUGINSDIR\payload\shortcuts\${DISPLAY_NAME}.lnk" "$INSTDIR\Visualizers.exe" '--visualizer ${VIS_ID}'
    CreateShortcut "$PLUGINSDIR\payload\shortcuts\${DISPLAY_NAME} settings.lnk" "$INSTDIR\Visualizers.exe" '--settings ${VIS_ID}'
    ${If} ${Errors}
      SetErrorLevel 1
      Abort "The standalone shortcuts could not be staged. No installed files were changed."
    ${EndIf}
  ${EndIf}
  ClearErrors
  StrCpy $0 -1
  ExecWait '"$PLUGINSDIR\payload\runtime\VisualizersCtl.exe" package-install --root "$INSTDIR" --payload "$PLUGINSDIR\payload" --vlc-root "$VLCROOT" --winamp-root "$WINAMPROOT" --menu-root "$SMPROGRAMS\VLC Visualizers" --components "$Components"' $0
  ${If} $0 != 0
    SetErrorLevel 1
    Abort "The selected components could not be installed. Existing files are preserved; inspect the package transaction for repair details."
  ${EndIf}
  StrCpy $PackageInstalled 1
  ClearErrors
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ${If} ${Errors}
    SetErrorLevel 1
    Abort "The uninstaller could not be written. The renderer transaction will be rolled back."
  ${EndIf}
  ${IfNot} ${SectionIsSelected} ${SEC_VLC}
    StrCpy $VLCROOT ""
  ${EndIf}
  ${IfNot} ${SectionIsSelected} ${SEC_WINAMP}
    StrCpy $WINAMPROOT ""
  ${EndIf}
  ClearErrors
  StrCpy $RegistrationWritten 1
  WriteRegStr HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "InstallRoot" "$INSTDIR"
  WriteRegStr HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "VLCROOT" "$VLCROOT"
  WriteRegStr HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "WINAMPROOT" "$WINAMPROOT"
  ${If} $WINAMPROOT != ""
    WriteRegStr HKLM "Software\VLCVisualizers\Renderers\${VIS_ID}" "Host" "$INSTDIR\Visualizers.exe"
  ${EndIf}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}" "DisplayName" "${DISPLAY_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  ${If} ${Errors}
    SetErrorLevel 1
    Abort "Registration failed. The renderer transaction will be rolled back."
  ${EndIf}
SectionEnd

!macro AddSelected SECTION_ID COMPONENT_ID
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
  !insertmacro AddSelected ${SEC_STANDALONE} standalone
  !insertmacro AddSelected ${SEC_VLC} vlc
  !insertmacro AddSelected ${SEC_WINAMP} winamp
FunctionEnd
Function .onInit
  !insertmacro AcquirePackageMutex
  ${IfNot} ${RunningX64}
    SetErrorLevel 1
    Abort "The rendering host requires 64-bit Windows."
  ${EndIf}
  StrCpy $PackageInstalled 0
  StrCpy $RegistrationWritten 0
  SetRegView 64
  ReadRegStr $R2 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}" "DisplayVersion"
  ${If} $R2 != ""
    SetErrorLevel 1
    MessageBox MB_ICONSTOP|MB_OK "Uninstall the existing ${DISPLAY_NAME} before installing this candidate. Its settings will be retained." /SD IDOK
    Abort
  ${EndIf}
!if "${VIS_ID}" == "geiss"
  ReadRegStr $R2 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Geiss64" "DisplayVersion"
  ${If} $R2 != ""
    SetErrorLevel 1
    MessageBox MB_ICONSTOP|MB_OK "Uninstall the older Geiss64 package before installing Prism Flow." /SD IDOK
    Abort
  ${EndIf}
!endif
  ReadRegStr $VLCROOT HKLM "Software\VideoLAN\VLC" "InstallDir"
  ${If} $VLCROOT == ""
    StrCpy $VLCROOT "$PROGRAMFILES64\VideoLAN\VLC"
  ${EndIf}
  SetRegView 32
  ReadRegStr $WINAMPROOT HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Winamp" "InstallLocation"
  ${If} $WINAMPROOT == ""
    ReadRegStr $WINAMPROOT HKCU "Software\Winamp" ""
  ${EndIf}
  ${If} $WINAMPROOT == ""
    StrCpy $WINAMPROOT "$PROGRAMFILES32\Winamp"
  ${EndIf}
  SetRegView 64
  ${GetParameters} $R0
  ${GetOptions} $R0 "/VLCROOT=" $R1
  ${If} $R1 != ""
    StrCpy $VLCROOT $R1
  ${EndIf}
  ${GetOptions} $R0 "/WINAMPROOT=" $R1
  ${If} $R1 != ""
    StrCpy $WINAMPROOT $R1
  ${EndIf}
  ${GetOptions} $R0 "/MODE=" $MODE
!ifdef NO_WINAMP
  ${If} $MODE == "WINAMP"
    SetErrorLevel 1
    Abort "This visualizer supports standalone use and VLC."
  ${EndIf}
!endif
  ${If} $MODE == "VLC"
    !insertmacro UnselectSection ${SEC_WINAMP}
    !insertmacro UnselectSection ${SEC_STANDALONE}
  ${ElseIf} $MODE == "WINAMP"
    !insertmacro UnselectSection ${SEC_VLC}
  ${ElseIf} $MODE == "STANDALONE"
    !insertmacro UnselectSection ${SEC_VLC}
    !insertmacro UnselectSection ${SEC_WINAMP}
  ${ElseIf} $MODE != "ALL"
    ${If} $MODE != ""
      SetErrorLevel 1
      Abort "Unknown installer mode."
    ${EndIf}
    ${IfNot} ${FileExists} "$WINAMPROOT\winamp.exe"
      !insertmacro UnselectSection ${SEC_WINAMP}
    ${EndIf}
    ${IfNot} ${FileExists} "$VLCROOT\visualizers\native\VisualizerNative.exe"
      !insertmacro UnselectSection ${SEC_VLC}
    ${EndIf}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTSTANDALONE" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_STANDALONE}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTVLC" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_VLC}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/WITHOUTWINAMP" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SEC_WINAMP}
  ${EndIf}
!ifdef NO_WINAMP
  !insertmacro UnselectSection ${SEC_WINAMP}
  StrCpy $WINAMPROOT ""
!endif
  ClearErrors
FunctionEnd
Function .onInstFailed
  ${If} $PackageInstalled == 1
    ExecWait '"$PLUGINSDIR\payload\runtime\VisualizersCtl.exe" package-remove --root "$INSTDIR" --payload "$PLUGINSDIR\payload" --vlc-root "$VLCROOT" --winamp-root "$WINAMPROOT" --menu-root "$SMPROGRAMS\VLC Visualizers"' $0
    ${If} $0 == 0
      ${If} $RegistrationWritten == 1
        DeleteRegKey HKLM "Software\VLCVisualizers\Renderers\${VIS_ID}"
        DeleteRegKey HKLM "Software\VLCVisualizers\Packages\${VIS_ID}"
        DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}"
      ${EndIf}
      Delete "$INSTDIR\Uninstall.exe"
      Delete "$INSTDIR\.package\transaction.json"
      RMDir "$INSTDIR\.package"
      RMDir "$SMPROGRAMS\VLC Visualizers"
      RMDir "$INSTDIR"
    ${EndIf}
  ${EndIf}
FunctionEnd
Section Uninstall
  !insertmacro AcquirePackageMutex
  SetShellVarContext all
  SetRegView 64
  ReadRegStr $R0 HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "InstallRoot"
  ${If} $R0 != "$INSTDIR"
    SetErrorLevel 1
    Abort "The renderer registration belongs to a different installation; no files were removed."
  ${EndIf}
  ReadRegStr $VLCROOT HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "VLCROOT"
  ReadRegStr $WINAMPROOT HKLM "Software\VLCVisualizers\Packages\${VIS_ID}" "WINAMPROOT"
  ${If} $WINAMPROOT != ""
    ReadRegStr $R0 HKLM "Software\VLCVisualizers\Renderers\${VIS_ID}" "Host"
    ${If} $R0 != "$INSTDIR\Visualizers.exe"
      SetErrorLevel 1
      Abort "The Winamp host registration changed; no files were removed."
    ${EndIf}
  ${EndIf}
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\payload"
  File "${PAYLOAD_DIR}\renderer-payload.json"
  File /oname=VisualizersCtl.exe "${PAYLOAD_DIR}\runtime\VisualizersCtl.exe"
  ClearErrors
  StrCpy $0 -1
  ExecWait '"$PLUGINSDIR\payload\VisualizersCtl.exe" package-remove --root "$INSTDIR" --payload "$PLUGINSDIR\payload" --vlc-root "$VLCROOT" --winamp-root "$WINAMPROOT" --menu-root "$SMPROGRAMS\VLC Visualizers"' $0
  ${If} $0 != 0
    SetErrorLevel 1
    Abort "Renderer removal was refused. Modified or in-use files and transaction evidence were retained; close the players and retry."
  ${EndIf}
  DeleteRegKey HKLM "Software\VLCVisualizers\Renderers\${VIS_ID}"
  DeleteRegKey HKLM "Software\VLCVisualizers\Packages\${VIS_ID}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PACKAGE_NAME}"
  Delete "$INSTDIR\.package\transaction.json"
  RMDir "$INSTDIR\.package"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$SMPROGRAMS\VLC Visualizers"
  RMDir "$INSTDIR"
SectionEnd
