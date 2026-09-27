Building AFCE
=============

AFCE is a C++17 / Qt 6 application built with CMake. It builds on GNU/Linux, macOS and Windows.

Requirements
------------
* CMake 3.21 or newer
* A C++17 compiler: GCC 9+, Clang 10+ / Apple Clang, or MSVC 2019+ (MinGW works too)
* Qt 6.2 or newer with the modules **Core, Gui, Widgets, Xml, Svg, PrintSupport** and the
  **Linguist tools** (`lrelease`/`lupdate`, used to compile the translations).
  Qt 6.2 is enough for building and running; installing with `-DAFCE_DEPLOY_QT=ON`
  (see below) needs Qt 6.3 or newer.
* Optional: Qt's own translations (`qtbase_ru.qm`, `qtbase_uk.qm`). If they are found at
  configure time they are embedded, so that standard buttons (Save, Cancel, ...) and dialogs
  are translated as well. They are looked up in the translations directory of Qt; another
  directory can be given with `-DAFCE_QT_TRANSLATIONS_DIR=...`.

Help pages, code generators (`generators/*.json`) and translations are embedded into the
executable, so a build tree can be run directly without copying any data files.

GNU/Linux
---------
Install Qt 6 and the build tools, e.g.

* Debian 12+ / Ubuntu 24.04+ (Qt 6.4): `sudo apt install build-essential cmake libgl-dev qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-translations-l10n`
* Ubuntu 22.04 (Qt 6.2): the same, but `libqt6svg6-dev` instead of `qt6-svg-dev`
* Fedora: `sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel qt6-linguist`
* Arch Linux: `sudo pacman -S base-devel cmake qt6-base qt6-svg qt6-tools`

Then build:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/afce

Install (binaries, desktop file, icon and the MIME type for `.afc` files):

    sudo cmake --install build                           # prefix /usr/local
    cmake --install build --prefix /usr --strip          # or choose a prefix
    DESTDIR=/tmp/pkg cmake --install build --prefix /usr # staged install for packaging

macOS
-----
Install the Xcode command line tools (`xcode-select --install`) and Qt 6, for example with
Homebrew (`brew install cmake qt`; `brew install qttranslations` for Qt's own translations)
or the Qt online installer (then pass `-DCMAKE_PREFIX_PATH=~/Qt/6.x.y/macos`).

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    open build/afce.app

The build produces the application bundle `build/afce.app` (icon, `.afc` document type and bundle
identifier are declared in `cmake/Info.plist.in`) and the console tool `build/afce-cli`.
To get a self-contained bundle that runs on computers without Qt, configure with
`-DAFCE_DEPLOY_QT=ON` and install; this runs `macdeployqt`:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAFCE_DEPLOY_QT=ON
    cmake --build build -j
    cmake --install build --prefix dist

In this configuration the console tool is installed into the bundle as
`dist/afce.app/Contents/MacOS/afce-cli` and uses the bundled Qt frameworks as well
(without `AFCE_DEPLOY_QT` it goes to `<prefix>/bin/afce-cli`).

Windows
-------
Install Qt 6 with the Qt online installer (MSVC or MinGW kit) and CMake. In a
"Developer Command Prompt" (MSVC) or a shell where the MinGW compiler is in `PATH`:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
    cmake --build build --config Release
    cmake --install build --config Release --prefix dist

The icon of `afce.exe` comes from `afce.rc`. With `-DAFCE_DEPLOY_QT=ON` the install step runs
`windeployqt`, which copies the Qt DLLs and plugins next to `afce.exe`.

Qt Creator
----------
Open `CMakeLists.txt` as a project, choose a Qt 6 kit and build (`Ctrl+B`).

Tests
-----
Tests are built by default (`-DBUILD_TESTING=OFF` disables them) and run with CTest:

    cmake --build build -j
    ctest --test-dir build --output-on-failure

* `tst_flowchart` – loading, saving, rendering and exporting flowcharts, undo/redo, editing
  (sample files in `tests/data/samples`, broken and unusual files in `tests/data/robustness`);
* `tst_mainwindow` – language switching, opening files, generator list, mouse wheel;
* `tst_generator` – code generators;
* `tst_codeimport` – building flowcharts from C/C++ code;
* `cli_*`, `gui_*` – smoke tests of `afce-cli` and of the headless modes of `afce`.

GUI tests use the `offscreen` Qt platform, so no display is needed (also on CI servers).

Command line
------------
`afce` (the editor):

    afce [file.afc]                                     open a flowchart
    afce --import prog.c [--function NAME]              build the flowchart of a C/C++ function
    afce file.afc --export out.png [--zoom 2]           export (png, jpg, bmp, svg, pdf, ...) and exit
    afce --import prog.cpp --export out.svg             export the flowchart of a function
    afce file.afc --screenshot window.png               save a screenshot of the main window and exit
    afce --language ru_RU                               user interface language for this run

`--export`, `--screenshot`, `--help` and `--version` do not need a display (the `offscreen`
platform is selected automatically when it is installed). Exports always use black lines on a
white background. PDF pages are limited to 200 x 200 inches (the PDF page size limit): a larger
chart is scaled down to fit.

`afce-cli` (console tool, no GUI libraries needed):

    afce-cli languages                                  list code generator ids (c, cpp, py, pas, ...)
    afce-cli generate file.afc --lang py [--output f]   generate source code from a flowchart
    afce-cli functions prog.c [--json]                  list the functions of a C/C++ file
    afce-cli import prog.c [--function NAME | --index N] [--output file.afc]
             [--lang c|cpp] [--for-style cstyle|while|arithmetic] [--keep-declarations]
             [--no-io] [--no-calls] [--no-expand-assign] [--exact-output] [--keep-main-return]
             [--generate LANG] [--diagnostics]

`afce-cli <command> --help` describes all options. A file name `-` means standard input.
Errors are reported on stderr and give a non-zero exit code.

Translations
------------
The user interface translations are in `locale/*.ts`. After changing strings in the sources run

    cmake --build build --target update_translations

to update the `.ts` files, then edit them with Qt Linguist. They are compiled and embedded
automatically by the normal build. See `TRANSLATIONS.md`.

Data files on disk
------------------
The embedded generators and help pages can be overridden or extended without rebuilding: AFCE
also looks for `generators/*.json` and `help/<locale>/` in its user data directory
(e.g. `~/.local/share/afce`, `~/Library/Application Support/afce`, `%APPDATA%/afce`), next to the
executable, in `afce.app/Contents/Resources` (macOS) and in `<prefix>/share/afce` (Linux).

Continuous integration and releases
-----------------------------------
`.github/workflows/build.yml` builds AFCE with Qt 6.8 on Linux, macOS and Windows, runs the whole test suite and
packages the program: an AppImage (Linux x86_64), a universal dmg (macOS 12+, Apple Silicon and Intel) and a portable
zip (Windows x64, with the Qt and MSVC runtime DLLs). The packages are attached to every run as artifacts. Pushing a
tag `vX.Y.Z` (after updating `version.txt`) additionally publishes them as a GitHub release.
