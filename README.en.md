AFCE Reimagined
===============

[![Build](https://github.com/Heisenberg4441/afce-reimagined/actions/workflows/build.yml/badge.svg)](https://github.com/Heisenberg4441/afce-reimagined/actions/workflows/build.yml)

**Algorithm flowchart editor** for teaching programming: draws flowcharts following GOST 19.701 / ISO 5807,
generates source code in 13 languages and — the headline feature — **builds flowcharts from C and C++ code**.

A reworked version of [AFCE](https://github.com/viktor-zin/afce) by Victor Zinkevich, moved to a modern stack with a new
user interface, new blocks and source code import. [Русская версия](README.md)

![Main window](docs/screenshots/main.png)

What is new compared with AFCE 0.9.8
------------------------------------

### Code → flowchart (C and C++)
* **File → Import from Source Code** (Ctrl+I, the "Code → Flowchart" toolbar button): paste code or open a file, pick a
  function and see its flowchart in a live preview. Import one function or all of them (one tab each). Dropping a
  `.c` / `.cpp` file onto the window works too.
* **Two-way code panel**: for C and C++ the code on the right is editable; "To Flowchart" (Ctrl+Enter) rebuilds the
  flowchart of the current tab (Ctrl+Z brings the previous one back).
* Understands `if/else`, `switch` (including fall-through), `while`, `do-while`, `for`, range-`for`, `return`, `break`,
  `continue`, assignments (`x += 2` → `x := x + 2`), `scanf`/`printf`/`puts`/`gets`/`cin`/`cout`/`getline` input and
  output and function calls; finds functions in classes, namespaces and templates. Whatever a flowchart cannot show
  (`goto`, exception handlers, preprocessor directives) is listed in the messages with a link to the code line.
* A dependency-free C/C++ parser, tested on a corpus of student programs and with fuzzing.

![Import from source code](docs/screenshots/import.png)

### New blocks
Multiple choice (`switch`), C-style `for`, for-each loop, subroutine call (predefined process), `return`, `break` and
`continue`. Every block has a property dialog; the start terminator holds the function name, parameters and return type.

### Modern user interface
* Tabs instead of a new program instance per document, recent files, save prompts on close.
* **Drag and drop** of blocks — from the tool panel straight onto the chart and within the chart (Ctrl/Alt copies).
* Blocks grow with their text; tasteful colours per block type or the classic black-and-white GOST look.
* Light and **dark themes**, crisp HiDPI icons, zoom with Ctrl+wheel, trackpad pinch and "zoom to fit".
* Assignment symbol of your choice: `:=`, `=` or `←`.
* Syntax highlighting in the code panel, copy and save the code.
* English, Russian and Ukrainian user interface and help.

### Code generation
C, C++, Pascal, Python, JavaScript, PHP, Perl, Ruby, BASIC-256, FreeBASIC, VBScript, AutoIt and the Russian school
algorithmic language (KuMir). All generators support the new blocks; old bugs are fixed (`void main`, the `sdt::cin`
typo, missing `;` after `do … while`, wrong `for` bounds, the inverted post-condition loop in Python, indentation at
deep nesting). The C/C++ generators declare variables and choose `printf`/`scanf` formats. Generators are JSON rule
files (`generators/`), the format is documented in `sourcecodegenerator.h`.

### Export and command line
* PNG, SVG and PDF export, copy as image and printing — always in a light document style, even in dark mode.
* Headless use:

      afce chart.afc --export chart.png --zoom 2           # png / svg / pdf
      afce --import prog.c --function main --export chart.pdf
      afce-cli import prog.cpp --function main --output prog.afc
      afce-cli generate prog.afc --lang py                 # code from a flowchart
      afce-cli functions prog.c                            # list the functions

### Under the hood
* Qt 6 and C++17 instead of Qt 4/5, CMake instead of qmake; generators, help and translations are embedded.
* Old bugs fixed: runtime language switching, memory leaks, crashes on close and on damaged files, silent save errors,
  trackpad scrolling.
* More than 1100 automated tests: model and rendering, UI, generators (generated programs are really compiled and
  run), C/C++ import, command line.
* Builds and tests on Linux, macOS and Windows in GitHub Actions, with ready-to-use packages for all three.

Installation
------------
Packages are on the [Releases](https://github.com/Heisenberg4441/afce-reimagined/releases) page (builds of the latest
changes are available as [Actions](https://github.com/Heisenberg4441/afce-reimagined/actions) artifacts):

| System | File | How to run |
|--------|------|------------|
| Windows 10/11 (x64) | `afce-…-windows-x64.zip` | unpack, run `afce\bin\afce.exe` |
| macOS 12+ (Apple Silicon and Intel) | `afce-…-macos-universal.dmg` | drag AFCE to Applications; the build is not notarized, so open it the first time with right-click → Open (or `xattr -dr com.apple.quarantine /Applications/afce.app`) |
| Linux (x86_64) | `afce-…-linux-x86_64.AppImage` | `chmod +x afce-*.AppImage && ./afce-*.AppImage` |

Building from source
--------------------
Requires Qt 6.5+ (Core, Gui, Widgets, Xml, Svg, PrintSupport, LinguistTools) and CMake 3.21+:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ctest --test-dir build

See [BUILDING.md](BUILDING.md) for details and [TRANSLATIONS.md](TRANSLATIONS.md) for translating.
To publish a release, update `version.txt` and push a `vX.Y.Z` tag: GitHub Actions builds the packages and creates the
release.

Authors and license
-------------------
The original AFCE — © 2008–2014 Victor Zinkevich; contributors: Sergey Ryabenko, Alexey Loginov and others.
Licensed under the GNU General Public License, version 2 or 3 (see [LICENSE](LICENSE)).
