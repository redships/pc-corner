; NSIS installer script
Name "PC Corner"
OutFile "PCCorner-Installer.exe"
InstallDir "$PROGRAMFILES\\PC Corner"
RequestExecutionLevel user
Page directory
Page instfiles

Section "Install"
  SetOutPath "$INSTDIR"
  File "PCCorner.exe"
  File "present_hook.dll"
  File "injector.exe"
  CreateShortCut "$SMPROGRAMS\\PC Corner.lnk" "$INSTDIR\\PCCorner.exe"
  ; optional autostart
  ; WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Run" "PCCorner" "$INSTDIR\\PCCorner.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\\PCCorner.exe"
  Delete "$INSTDIR\\present_hook.dll"
  Delete "$INSTDIR\\injector.exe"
  Delete "$SMPROGRAMS\\PC Corner.lnk"
  RMDir "$INSTDIR"
  ; DeleteRegValue HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Run" "PCCorner"
SectionEnd
