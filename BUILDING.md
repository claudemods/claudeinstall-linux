# Building ClaudeInstall

**Status:** the code has never been compiled (it was written on a machine without a compiler,
CMake or Qt). Expect compile errors on the first build; the checklist at the end says where to look.

Linux only. There are three ways to build:

| Build | Gives you | CMake options |
|---|---|---|
| GUI | `claudeinstall` (Widgets / QML) | default |
| GUI + TUI | both programs | `-DWITH_TUI=ON` |
| TUI only | `claudeinstall-tui` (terminal) | `-DWITH_GUI=OFF -DWITH_TUI=ON` |

## Dependencies

Always:

- CMake >= 3.16, a C++17 compiler, Ninja or Make
- Qt 5.15 (or Qt 6.5 with `-DWITH_QT6=ON`): Core, Gui, Network, DBus, Concurrent, LinguistTools
  (Test for the tests)
- KDE Frameworks (5.78 / 6): CoreAddons; ECM for the tests
- yaml-cpp >= 0.5.1
- For the `partition` module: KPMcore >= 20.04 (24.01.75 for Qt6), with KDE I18n and WidgetsAddons.
  Without KPMcore the partition module is skipped.

Only for the GUI build (`WITH_GUI=ON`): Qt Svg and Widgets; with `WITH_QML=ON` also Qt Quick and QuickWidgets;
KDE Crash (`-DBUILD_CRASH_REPORTING=OFF` to drop it).

Only for the TUI (`WITH_TUI=ON`): [FTXUI](https://github.com/ArthurSonzogni/FTXUI) >= 5. If it is
not installed, CMake downloads v5.0.0 (needs git and network; `-DFETCH_FTXUI=OFF` forbids that).

Some modules have their own optional dependencies (libcrypt for `users`, libpwquality, ICU, AppStream, ...);
CMake lists what it found and what it skipped at the end of configuration.

Debian / Ubuntu, for example:

```bash
sudo apt install build-essential cmake ninja-build git qtbase5-dev qtbase5-private-dev qttools5-dev \
  qttools5-dev-tools libqt5svg5-dev qtdeclarative5-dev libkf5coreaddons-dev extra-cmake-modules \
  libyaml-cpp-dev libkpmcore-dev libkf5i18n-dev libkf5widgetsaddons-dev libkf5crash-dev
```

(Package names differ per distribution. For TUI only, drop the Svg, Declarative and Crash packages
and add `libftxui-dev` if your distribution has version 5.)

## Commands

GUI only:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

GUI and TUI:

```bash
cmake -S . -B build -G Ninja -DWITH_TUI=ON
cmake --build build
```

TUI only (no Widgets, Svg or QML of your own; see the note on KPMcore below):

```bash
cmake -S . -B build-tui -G Ninja -DWITH_GUI=OFF -DWITH_TUI=ON
cmake --build build-tui
```

Maintenance tools (all C++; the schema validator is built automatically for the tests):

```bash
cmake --build build --target tools   # txstats, txreduce, cldr-/zone-/layout-extractor, configvalidator
```n
Tests (needs ECM and Qt Test; the config-file schema tests use 	ools/configvalidator, no Python needed):

```bash
cmake -S . -B build -G Ninja -DWITH_TUI=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Trying it without installing (from the build directory, with `-d` so the configuration is
looked for in the current directory):

```bash
./build/src/claudeinstall-tui/claudeinstall-tui -d -S 2> tui.log   # -S keeps stderr; send it to a file
./build/src/claudeinstall/claudeinstall -d                         # the GUI
```

Do not run the installer for real on a machine you care about: the partition and bootloader
jobs change the disk. Use a virtual machine, or build with `-DDEBUG_PARTITION_SKIP=ON`.

Notes:

- With `WITH_GUI=OFF`, KPMcore still pulls in KDE's WidgetsAddons, hence QtWidgets, at link time.
- The text-mode installer needs a terminal of reasonable size (the sidebar takes 26 columns).
  Keys: F1 back, F2 next, F10 cancel / done.

## When the build fails: look here first

Work in this order; each step is where the most errors are likely to be.

1. **Configuration errors (CMake).**
   - `ClaudeInstallAddPlugin.cmake` was edited by script: check the `_descriptor_only` block
     (lines ~130-200) for stray or missing `endif()`.
   - `src/modules/partition/CMakeLists.txt` was restructured (new `partition_core` library); check the
     source lists and the `UI` keyword after `gui/VolumeGroupBaseDialog.cpp`.
   - FTXUI target names: `ftxui::screen`, `ftxui::dom`, `ftxui::component`.
2. **The headless libraries**, because everything else builds on them:
   `libclaudeinstallstep` (Step, Wizard, ExecutionModel, BrandingInfo, ModuleFactory), then `libclaudeinstallui`.
   If the GUI build breaks here, the Phase 2a refactor (ViewStep deriving from Step, ViewManager from Wizard) is the cause.
3. **The `*_config` libraries** (`claudeinstall_add_config_library`). They link QtCore only, so an
   include of a Widgets/GUI header in a `Config.cpp` is a compile error by design: move that code to the view step.
4. **Ported job modules** (the former Python modules; there is no Python left anywhere in the tree, the old translation catalogs in `lang/python/` are kept only as a source for translators): `packages`, `displaymanager`, `bootloader`,
   `unpackfs`, ... Their tests are `packagestest`, `displaymanagertest`, `bootloadertest`. Compare behaviour with the
   deviations noted in `claudeinstall-port-deviations.md` of the project memory, if you have it.
5. **`libclaudeinstalltui`**, then the `*TuiStep.cpp` of each module. Typical problems: FTXUI API details
   (`Maybe`, `Menu` options, `InputOption::password`, `Event::F10`, decorators on `Component`) and Qt
   string conversions (`QT_NO_CAST_FROM_ASCII`).
6. **Plugins that do not load at run time** (message "No factory" or "create() failed" in the log): a
   text-mode plugin must have `CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION` and be named in `module.desc` by
   `tuiLoad`; check `build/src/modules/<name>/module.desc`.
7. **Tests**: run `libclaudeinstallmodulesystemtest`, `test_libclaudeinstallstep` and `test_libclaudeinstalltui`
   first; they need no hardware.

Known things that were guessed, not checked, and are worth a look if behaviour is wrong:

- the model deletion in `PartitionTuiStep::prettyStatus()` (possible double delete of the "before" device);
- the translation context of strings that moved (`ClaudeInstall::ViewManager`, `PartitionViewStep`,
  `NetInstallViewStep`, `OEMViewStep`), which keep their old context names on purpose;
- the placeholder URLs (`claudeinstall.io`, `com.github.claudeinstall...`) -- replace them with real ones;
- new translatable strings need `lupdate` before they are translated.
