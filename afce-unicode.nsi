RequestExecutionLevel admin
SetCompressor /SOLID lzma
!define VERSION 0.9.8-beta
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
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Ukrainian.nlf"
LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"

LangString ^Name ${LANG_ENGLISH} "Algorithm flowchart editor"
LangString ^Name ${LANG_RUSSIAN} "Редактор блок-схем алгоритмов"
LangString ^Name ${LANG_UKRAINIAN} "Редактор блок-схем алгоритмів"

LangString AFCE ${LANG_ENGLISH} "Flowchart editor"
LangString AFCE ${LANG_RUSSIAN} "Редактор блок-схем"
LangString AFCE ${LANG_UKRAINIAN} "Редактор блок-схем"

LangString FLOWCHART ${LANG_ENGLISH} "Flowchart"
LangString FLOWCHART ${LANG_RUSSIAN} "Блок-схема"
LangString FLOWCHART ${LANG_UKRAINIAN} "Блок-схема"

LangString REMOVEPROGRAM ${LANG_ENGLISH} "Remove the program"
LangString REMOVEPROGRAM ${LANG_RUSSIAN} "Удалить программу"
LangString REMOVEPROGRAM ${LANG_UKRAINIAN} "Видалити програму"

LangString LOCALE ${LANG_ENGLISH} "en_US"
LangString LOCALE ${LANG_RUSSIAN} "ru_RU"
LangString LOCALE ${LANG_UKRAINIAN} "uk_UA"


Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Program"
  SetOutPath "$INSTDIR"
  File "${BUILDDIR}\release\afce.exe"
  File "README.RU.txt"
  File "afc.ico"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayName" $(AFCE)
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayIcon" "$INSTDIR\afce.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "Publisher" "Виктор Зинкевич"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "URLInfoAbout" "http://viktor-zin.github.io/afce/"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "InstallLocation" "$INSTDIR"

  WriteRegStr HKCR ".afc" "" "afcfile"
  WriteRegStr HKCR ".afc" "Content Type" "application/x-afce"
  WriteRegStr HKCR "afcfile" "" $(FLOWCHART)
  WriteRegStr HKCR "afcfile\DefaultIcon" "" '$INSTDIR\afc.ico'
  WriteRegStr HKCR "afcfile\shell\open\command" "" '"$INSTDIR\afce.exe" "%1"'

  WriteRegDWORD HKLM "Software\afce" "InstalledLang" $LANGUAGE
  WriteRegStr HKCU "Software\afce\application" "locale" $(LOCALE)

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
  CreateDirectory "$SMPROGRAMS\$(AFCE)"
  CreateShortCut "$SMPROGRAMS\$(AFCE)\$(AFCE).lnk" "$INSTDIR\afce.exe"
  CreateShortCut "$SMPROGRAMS\$(AFCE)\$(REMOVEPROGRAM).lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section "Uninstall"
  RMDIR /r "$INSTDIR"
  RMDIR /r "$SMPROGRAMS\$(AFCE)"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce"

  DeleteRegKey HKCR ".afc"
  DeleteRegKey HKCR "afcfile"
  DeleteRegKey HKLM "Software\afce"
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'
SectionEnd

Function .onInit

  ;Language selection dialog

  Push ""
  Push ${LANG_ENGLISH}
  Push English
  Push ${LANG_RUSSIAN}
  Push Russian
  Push ${LANG_UKRAINIAN}
  Push Ukrainian
  Push A ; A means auto count languages
         ; for the auto count to work the first empty push (Push "") must remain
  LangDLL::LangDialog "Installer Language" "Please select the language of the installer"

  Pop $LANGUAGE
  StrCmp $LANGUAGE "cancel" 0 +2
    Abort
FunctionEnd

Function un.onInit
  ReadRegDWORD $0 HKLM "Software\afce" "InstalledLang"
  IfErrors +3 0
    Push $0
    Pop $LANGUAGE
FunctionEnd