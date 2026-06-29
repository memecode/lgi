

Outfile "lvc-win64.exe"
InstallDir $PROGRAMFILES64\Memecode\Lvc

Section

    SetOutPath "$INSTDIR"

    # binaries:
    File "x64Release22\lvc.exe"
    File "..\..\lib\Lgi22.dll"
    File "..\..\..\deps\build-x64\lib\z.dll"
    File "..\..\..\deps\build-x64\lib\libpng16.lib"

    # resources:
    File "..\resources\Lvc.lr8"
    File "..\resources\image-list.png"

    # create uninstall:
    WriteUninstaller "$INSTDIR\uninstall.exe"
 
SectionEnd

Section "uninstall"
 
    Delete $INSTDIR\uninstaller.exe 
    RMDir $INSTDIR

SectionEnd