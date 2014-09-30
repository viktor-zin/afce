RequestExecutionLevel admin
SetCompressor /SOLID lzma
Name "Редактор блок-схем алгоритмов"
!define VERSION 0.9.7
!define QT_INSTALL_PREFIX C:\Qt\5.2.1\mingw48_32
!define QT_INSTALL_BINS ${QT_INSTALL_PREFIX}\bin
!define QT_INSTALL_PLUGINS ${QT_INSTALL_PREFIX}\plugins
!define QT_INSTALL_TRANSLATIONS ${QT_INSTALL_PREFIX}\translations
!define BUILDDIR ..\build-afce-Desktop_Qt_5_2_1_MinGW_32bit-Release
OutFile "..\afce-${VERSION}-win32.exe"
InstallDir "$PROGRAMFILES\afce"

AutoCloseWindow true
ShowInstDetails show

XPStyle on

LoadLanguageFile "${NSISDIR}\Contrib\Language files\Russian.nlf"

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Program"
  SetOutPath "$INSTDIR"
  File "${BUILDDIR}\release\afce.exe"
  File "README.RU.txt"
  File "afc.ico"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayName" "Редактор блок-схем"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayIcon" "$INSTDIR\afce.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "Publisher" "Виктор Зинкевич"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "URLInfoAbout" "http://viktor-zin.blogspot.ru/2011/09/blog-post_5556.html"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "InstallLocation" "$INSTDIR"

  WriteRegStr HKCR ".afc" "" "afcfile"
  WriteRegStr HKCR ".afc" "Content Type" "application/x-afce"
  WriteRegStr HKCR "afcfile" "" "Блок-схема"
  WriteRegStr HKCR "afcfile\DefaultIcon" "" '$INSTDIR\afc.ico'
  WriteRegStr HKCR "afcfile\shell\open\command" "" '"$INSTDIR\afce.exe" "%1"'

  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'

  WriteUninstaller "$INSTDIR\uninst.exe"
SectionEnd

Section "Translations"
  SetOutPath "$INSTDIR\locale"
  File "locale\*.qm"
  File "${BUILDDIR}\locale\*.qm"
SectionEnd

Section "Qt"
  SetOutPath "$INSTDIR"
  File "${QT_INSTALL_BINS}\libstdc++-6.dll"
  File "${QT_INSTALL_BINS}\libgcc_s_dw2-1.dll"
  File "${QT_INSTALL_BINS}\Qt5Core.dll"
  File "${QT_INSTALL_BINS}\Qt5Gui.dll"
  File "${QT_INSTALL_BINS}\Qt5PrintSupport.dll"
  File "${QT_INSTALL_BINS}\Qt5Svg.dll"
  File "${QT_INSTALL_BINS}\Qt5Xml.dll"
  File "${QT_INSTALL_BINS}\Qt5Widgets.dll"
  File "${QT_INSTALL_BINS}\libwinpthread-1.dll"
  File "${QT_INSTALL_BINS}\icuin51.dll"
  File "${QT_INSTALL_BINS}\icuuc51.dll"
  File "${QT_INSTALL_BINS}\icudt51.dll"
  
  SetOutPath "$INSTDIR\platforms"
  File "${QT_INSTALL_PLUGINS}\platforms\qwindows.dll"

  SetOutPath "$INSTDIR\imageformats"
  File "${QT_INSTALL_PLUGINS}\imageformats\qwbmpd.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qtiff.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qmng.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qjpeg.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qgif.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qsvg.dll"
  File "${QT_INSTALL_PLUGINS}\imageformats\qwbmp.dll"

  SetOutPath "$INSTDIR\printsupport"
  File "${QT_INSTALL_PLUGINS}\printsupport\windowsprintersupport.dll"


SectionEnd


Section "Help"
  SetOutPath "$INSTDIR\help"
  File /r "help\*.*"
SectionEnd

Section "Generators"
  SetOutPath "$INSTDIR\generators"
  File /r "generators\*.json"
SectionEnd


Section "Shortcuts"
  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\Редактор блок-схем"
  CreateShortCut "$SMPROGRAMS\Редактор блок-схем\Редактор блок-схем.lnk" "$INSTDIR\afce.exe"
  CreateShortCut "$SMPROGRAMS\Редактор блок-схем\Удалить программу.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section "Uninstall"
  RMDIR /r "$INSTDIR"
  RMDIR /r "$SMPROGRAMS\Редактор блок-схем"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce"

  DeleteRegKey HKCR ".afc"
  DeleteRegKey HKCR "afcfile"
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'
SectionEnd
