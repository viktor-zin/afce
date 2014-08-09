SetCompressor /SOLID lzma
Name "Редактор блок-схем алгоритмов"
!define VERSION 0.9.6
!define QTDIR C:\Qt\5.2.1\mingw48_32\bin
!define BUILDDIR ..\build-afce-Desktop_Qt_5_2_1_MinGW_32bit-Debug\release
OutFile "..\afce-${VERSION}-win32.exe"
InstallDir "$PROGRAMFILES\afce"
;BGGradient 0000FF 000000 FFFFFF
;MiscButtonText "< Назад" "Далее >" "Отмена" "Закрыть"
;InstallButtonText "Установить"
;DirText "Укажите папку, куда будет учтаневлен Клиент." "Укажите директорию:" "Обзор..."
AutoCloseWindow true
ShowInstDetails show
;CompletedText "Готово"
XPStyle on

LoadLanguageFile "${NSISDIR}\Contrib\Language files\Russian.nlf"

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Program"
  SetOutPath "$INSTDIR"
  File "${BUILDDIR}\afce.exe"
  File "${BUILDDIR}\afce_ru_RU.qm"
  File "${BUILDDIR}\afce_en_US.qm"
  File "README.RU.txt"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayName" "Редактор блок-схем"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayIcon" "$INSTDIR\afce.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "Publisher" "Виктор Зинкевич"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "URLInfoAbout" "http://viktor-zin.blogspot.ru/2011/09/blog-post_5556.html"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce" "InstallLocation" "$INSTDIR"

  WriteUninstaller "$INSTDIR\uninst.exe"
SectionEnd

Section "Qt"
  SetOutPath "$INSTDIR"
  File "${QTDIR}\libstdc++-6.dll"
  File "${QTDIR}\libgcc_s_dw2-1.dll"
  File "${QTDIR}\Qt5Core.dll"
  File "${QTDIR}\Qt5Gui.dll"
  File "${QTDIR}\Qt5PrintSupport.dll"
  File "${QTDIR}\Qt5Svg.dll"
  File "${QTDIR}\Qt5Xml.dll"
  File "${QTDIR}\Qt5Widgets.dll"
  File "${QTDIR}\libwinpthread-1.dll"
  File "${QTDIR}\icuin51.dll"
  File "${QTDIR}\icuuc51.dll"
  File "${QTDIR}\icudt51.dll"

SectionEnd

Section "Doc"
  SetOutPath "$INSTDIR\doc"
  File /r "doc\*.*"
SectionEnd


Section "Shortcuts"
  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\Редактор блок-схем"
  CreateShortCut "$SMPROGRAMS\Редактор блок-схем\Редактор блок-схем.lnk" "$INSTDIR\afce.exe"
  CreateShortCut "$SMPROGRAMS\Редактор блок-схем\Удалить программу.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\doc\*.*"
  RMDIR "$INSTDIR\doc"

  Delete "$INSTDIR\*.*"
  RMDIR "$INSTDIR"
  
  Delete "$SMPROGRAMS\Редактор блок-схем\Редактор блок-схем.lnk"
  Delete "$SMPROGRAMS\Редактор блок-схем\Удалить программу.lnk"
  RMDIR "$SMPROGRAMS\Редактор блок-схем"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\afce"

SectionEnd
