; NSIS installer script
Name "PC Corner"
OutFile "PCCorner-Installer.exe"
InstallDir "$LOCALAPPDATA\\PC Corner"
RequestExecutionLevel user
Page directory
Page instfiles

Section "Install"
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "PCCorner.exe"
  File "present_hook.dll"
  File "injector.exe"
  WriteUninstaller "$INSTDIR\\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\\PC Corner.lnk" "$INSTDIR\\PCCorner.exe"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PC Corner" "DisplayName" "PC Corner"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PC Corner" "UninstallString" "$\"$INSTDIR\\Uninstall.exe$\""
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  Delete "$INSTDIR\\PCCorner.exe"
  Delete "$INSTDIR\\present_hook.dll"
  Delete "$INSTDIR\\injector.exe"
  Delete "$INSTDIR\\Uninstall.exe"
  Delete "$SMPROGRAMS\\PC Corner.lnk"
  DeleteRegKey HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PC Corner"
  RMDir "$INSTDIR"
SectionEnd
