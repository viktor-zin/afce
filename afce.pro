TEMPLATE = app
TARGET = afce
VERSION = 0.9.8-alpha

  system(echo $$VERSION > version.txt)

INCLUDEPATH += .
QT += gui
QT += xml
QT += printsupport
QT += svg
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}
CONFIG += exceptions \
    rtti \
    stl
OBJECTS_DIR = build
UI_DIR = build
MOC_DIR = build
RCC_DIR = build


win32 {


    RC_FILE = afce.rc

    # Enable console in Debug mode on Windows, with useful logging messages
    Debug:CONFIG += console

    Release:DEFINES += NO_CONSOLE

    gcc48:QMAKE_CXXFLAGS += -Wno-unused-local-typedefs

}

QT_QM = $$[QT_INSTALL_TRANSLATIONS]/qt_ru.qm \
	$$[QT_INSTALL_TRANSLATIONS]/qt_uk.qm

greaterThan(QT_MAJOR_VERSION, 4) {
    QT_QM += $$[QT_INSTALL_TRANSLATIONS]/qtbase_ru.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtscript_ru.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtquick1_ru.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtmultimedia_ru.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtxmlpatterns_ru.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtbase_uk.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtscript_uk.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtquick1_uk.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtmultimedia_uk.qm \
        $$[QT_INSTALL_TRANSLATIONS]/qtxmlpatterns_uk.qm
}

win32 {
    for(qm, QT_QM) {
        system(copy /Y $$replace(qm, /, \\) $$replace(_PRO_FILE_PWD_, /, \\)\\locale\\)
    }

}
!win32 {
    system(cp -f $$QT_QM locale/)
}

macx {
    QMAKE_MAC_SDK = macosx10.9
    ICON = afce.icns
    QMAKE_INFO_PLIST = myInfo.plist
    QMAKE_POST_LINK = mkdir -p afce.app/Contents/MacOS/locale & \
                      cp -R locale/*.qm afce.app/Contents/MacOS/locale/ & \
                      cp -R $$QT_QM afce.app/Contents/MacOS/locale/ & \
                      mkdir -p afce.app/Contents/MacOS/help & \
                      cp -R $${PWD}/help afce.app/Contents/MacOS \
                      mkdir -p afce.app/Contents/MacOS/images & \
                      cp -R $${PWD}/images afce.app/Contents/MacOS \
                      mkdir -p afce.app/Contents/MacOS/generators & \
                      cp -R $${PWD}/generators afce.app/Contents/MacOS
}


unix:!mac {
  # This is to keep symbols for backtraces
  QMAKE_CXXFLAGS += -rdynamic
  QMAKE_LFLAGS += -rdynamic

  ## uncomment to force debug mode
  # QMAKE_CXXFLAGS += -g


    # Install prefix: first try to use qmake's PREFIX variable,
    # then $PREFIX from system environment, and if both fails,
    # use the hardcoded /usr.
    PREFIX = $${PREFIX}
    isEmpty( PREFIX ):PREFIX = $$(PREFIX)
    isEmpty( PREFIX ):PREFIX = /usr
    message(Install Prefix is: $$PREFIX)

    DEFINES += PROGRAM_DATA_DIR=\\\"$$PREFIX/share/afce/\\\"
    target.path = $$PREFIX/bin/
    locale.path = $$PREFIX/share/afce/locale/
    locale.files = locale/*.qm
    INSTALLS += target \
        locale
    pixmaps.path = $$PREFIX/share/pixmaps
    pixmaps.files = afce.png
    INSTALLS += pixmaps
    icons.path = $$PREFIX/share/icons
    icons.files = afc.ico
    INSTALLS += icons
    desktops.path = $$PREFIX/share/applications
    desktops.files = afce.desktop
    INSTALLS += desktops
    helps.path = $$PREFIX/share/afce/help/
    helps.files = help/*
    INSTALLS += helps
    generators.path = $$PREFIX/share/afce/generators
    generators.files = generators/*
    INSTALLS += generators
    mime.path = $$PREFIX/share/mime/packages
    mime.files = afce.xml
    INSTALLS += mime
}
DEFINES += PROGRAM_VERSION=\\\"$$VERSION\\\"

SOURCES += main.cpp \
    mainwindow.cpp \
    thelpwindow.cpp \
    zvflowchart.cpp \
    qflowchartstyle.cpp \
    sourcecodegenerator.cpp \
    qjson4/QJsonArray.cpp \
    qjson4/QJsonDocument.cpp \
    qjson4/QJsonObject.cpp \
    qjson4/QJsonParseError.cpp \
    qjson4/QJsonParser.cpp \
    qjson4/QJsonValue.cpp \
    qjson4/QJsonValueRef.cpp

HEADERS += mainwindow.h \
    thelpwindow.h \
    zvflowchart.h \
    qflowchartstyle.h \
    sourcecodegenerator.h \
    qjson4/QJsonArray.h \
    qjson4/QJsonDocument.h \
    qjson4/QJsonObject.h \
    qjson4/QJsonParseError.h \
    qjson4/QJsonParser.h \
    qjson4/QJsonRoot.h \
    qjson4/QJsonValue.h \
    qjson4/QJsonValueRef.h
RESOURCES += afce.qrc
CONFIG += release
TRANSLATIONS += locale/afce_en_US.ts \
    locale/afce_ru_RU.ts \
    locale/afce_uk_UA.ts


# This makes qmake generate translations

win32:# Windows doesn't seem to have *-qt4 symlinks
isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease-qt4

# The *.qm files might not exist when qmake is run for the first time,
# causing the standard install rule to be ignored, and no translations
# will be installed. With this, we create the qm files during qmake run.
!win32 {
  system($${QMAKE_LRELEASE} -silent $${_PRO_FILE_} 2> /dev/null)
}
win32 {
  system($$[QT_INSTALL_BINS]\\lrelease.exe $${_PRO_FILE_})
}


updateqm.input = TRANSLATIONS
updateqm.output = locale/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE \
    ${QMAKE_FILE_IN} \
    -qm \
    ${QMAKE_FILE_OUT}
updateqm.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += updateqm
TS_OUT = $$TRANSLATIONS
TS_OUT ~= s/.ts/.qm/g
PRE_TARGETDEPS += $$TS_OUT
