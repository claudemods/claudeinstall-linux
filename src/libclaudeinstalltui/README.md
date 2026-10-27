# libclaudeinstalltui: the text-mode installer

`claudeinstall-tui` runs the same modules, `settings.conf` and jobs as the
graphical `claudeinstall`, shown in a terminal with [FTXUI](https://github.com/ArthurSonzogni/FTXUI).
Build it with `-DWITH_TUI=ON` (FTXUI >= 5 is found with `find_package(ftxui)`,
or downloaded if `FETCH_FTXUI` is on).

## Building only the text-mode installer

```bash
cmake -S . -B build -DWITH_GUI=OFF -DWITH_TUI=ON
```

With `WITH_GUI=OFF` (which also turns `WITH_QML` off) CMake does not build
`libclaudeinstallui`, the `claudeinstall` program, the QML files or any Widgets / QML view
module. A view module's `claudeinstall_add_plugin()` then writes only its `module.desc`
(without a `load` key); the module's text-mode plugin, if it has one, adds `tuiLoad`. A view
module without a text-mode plugin is known to the loader and shows the placeholder page.
At least one of `WITH_GUI` and `WITH_TUI` must be on.

## Pieces

| Class | What it is |
|---|---|
| `TuiStep` | A `Step` plus the FTXUI page (`createComponent()`). The text-mode `ViewStep`. |
| `TuiWizard` | The `Wizard` for a terminal. Questions are asked in two rounds (see below). |
| `TuiModule` | A view module in this frontend: loads the module's `tuiLoad` plugin, or a `PlaceholderStep`. |
| `TuiModuleLoader` | Headless `ModuleManager`: scans `module.desc`, loads the *sequence*, checks requirements. |
| `TuiExecutionStep` | The *exec* phase: progress bar and message, from the `ExecutionModel`. |
| `TuiApplication` | Owns the loader and the screen; draws the sidebar, page and keys. |

## One thread

Qt's event loop runs on the main thread, and so does FTXUI. A timer calls
`ftxui::Loop::RunOnce()` every 30 ms (keys and drawing) and posts a redraw event a few times
a second, since Qt signals do not make FTXUI redraw. Config objects therefore never
need to be thread-safe on the UI's account. Standard error is pointed at `/dev/null`
while the screen is up (use `-S` to keep it); everything is still in the log file.

## Keys

`F1` back, `F2` next, `F10` cancel (done at the end). The rest goes to the page.

## Questions

A terminal cannot block while asking. When the `Wizard` asks "really cancel?" or
"start the installation?", `TuiWizard` answers *no*, emits `cancelQuestion()` /
`executionQuestion()`, and the application shows a modal; on *yes* it calls
`confirmCancelAndQuit()` / `confirmExecutionAndNext()`, which repeat the action
with the answer *yes*.

## Adding a text-mode page to a module

1. The module has a `<module>_config` library (`claudeinstall_add_config_library()`).
2. Derive a class from `ClaudeInstall::TuiStep`, with a `Config` made from that library,
   and a `CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION`. See `src/modules/welcome/WelcomeTuiStep.*`.
3. In the module's `CMakeLists.txt`, after `claudeinstall_add_plugin()`:

   ```cmake
   claudeinstall_add_tui_plugin(mymodule SOURCES MyTuiStep.cpp LINK_LIBRARIES mymodule_config)
   ```

   This builds `libclaudeinstall_tuimodule_mymodule.so` and adds `tuiLoad: ...` to the
   module's `module.desc`. A module without a text-mode plugin gets a `PlaceholderStep`
   that tells the user so and lets them continue; its jobs are **not** created.

`TuiWidgets.h` has the building blocks the pages share (`Tui::field`, `Tui::check`,
`Tui::scrolled`, `Tui::titled`, `Tui::problem`), so the pages look alike.

## Modules with a text-mode page

welcome, users, summary, finished, license, tracking, oemid, locale, keyboard,
packagechooser, netinstall. All of them use the module's `_config` library unchanged;
only the `<Name>TuiStep` is new. (`RESOURCES` of `claudeinstall_add_tui_plugin()` is for
Configs that read from a Qt resource file, e.g. keyboard.)

partition has a text-mode page for **automatic partitioning only** (erase a whole disk;
swap, filesystem and encryption choices). It works on the module's `partition_core`
library (KPMcore wrappers, models, actions, jobs and the Config), which the GUI plugin
now links too, and applies the choice with `PartitionActions::doAutopartition()` when
the step is left.

## Known limits

* partition: no install alongside another system, no replacing a partition, no manual
  partitioning in text mode. A disk with a mounted partition cannot be erased.
* partition_core needs QtGui (colours in the models) and QtConcurrent, but no Widgets.
* locale: no world map; changing the zone applies it at once, which for a Config with
  *adjustLiveTimezone* changes the live system's time zone on each arrow key.
* keyboard: no preview of the layout.
* packagechooser: no screenshots. netinstall: no descriptions column.
* users: the active-directory options of the GUI are not offered.
* license: the license texts are not shown, only their names and addresses.
* tracking: ticking "do not send any feedback" does not untick it again if a kind is chosen afterwards.
* finished: when the installation fails the application shows the error and exits (the
  finished page is not reached).
* The text-mode installer needs QtCore, QtGui (colours in the partition models; no window
  system is used), QtNetwork, QtDBus and QtConcurrent. It does not need Qt Widgets, Svg or
  Quick -- except that KPMcore, when it is used by the partition module, depends on
  KDE Frameworks' WidgetsAddons, which pulls in QtWidgets at link time.
* Ctrl-C ends the program without cancelling the jobs in a controlled way.
