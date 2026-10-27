<!-- SPDX-FileCopyrightText: no
     SPDX-License-Identifier: CC0-1.0
-->

# ClaudeInstall

A distribution-independent Linux system installer written entirely in C++ (Qt6), derived from
Calamares. It has an advanced partitioning module (manual and automated), and distributions
can customise it through branding and configuration without patching the code.

- **Pure C++.** Every module is C++. There is no Python or Python runtime dependency.
- **Two interfaces.** A graphical installer (`claudeinstall`, Qt6 Widgets/QML) and a
  terminal installer (`claudeinstall-tui`, FTXUI).
- **Two modes.**
  - *New system setup:* partition, unpack the system image, create users, install packages,
    configure and install a bootloader.
  - *Cloned system:* copy the running system to a new disk with rsync. No new users are
    created and no packages are installed; existing accounts carry over. A fresh machine-id,
    `fstab` and bootloader are still set up for the copy.

> **Status:** the code has not been compiled yet. Expect compile errors on the first build.
> See [BUILDING.md](BUILDING.md) for where to look. Test only in a virtual machine: the
> partition and bootloader steps change the disk.

## Quick start

```bash
./build.sh                       # asks: new system or clone? GUI, TUI or both?
./build.sh --clone --gui --install
./build.sh --new --tui --package
```

`build.sh` detects your distribution (Debian/Ubuntu, Fedora/RHEL, openSUSE, Arch, Alpine,
Gentoo, Void), installs the build dependencies, builds with CMake/Ninja, and can install the
result or wrap it into a package. Run `./build.sh --help` for all options.

## Configuration

| File | Purpose |
|---|---|
| `settings.conf` | Module sequence for a new-system install |
| `settings-clone.conf` | Module sequence for cloning the running system |
| `unpackfs-clone.conf` | What to copy (and exclude) when cloning |
| `src/modules/<name>/<name>.conf` | Per-module settings |
| `src/branding/` | Branding: logos, slideshow, strings |

`build.sh --install` puts the configuration for the chosen mode at
`/etc/claudeinstall/settings.conf`.

## Repository layout

- `src/libclaudeinstall*` – core and UI libraries
- `src/claudeinstall`, `src/claudeinstall-tui` – the GUI and TUI programs
- `src/modules` – installer modules (partition, users, unpackfs, bootloader, packages, ...)
- `src/branding` – branding components
- `ci/` – dependency scripts per distribution
- `tools/` – maintenance tools (all C++)
- `lang/` – translations

## More

- [BUILDING.md](BUILDING.md) – dependencies, CMake options, troubleshooting
- [CONTRIBUTING.md](CONTRIBUTING.md) – how to contribute
- `LICENSES/` – licence texts (GPL-3.0-or-later for the code, see per-file headers)
