<!-- SPDX-FileCopyrightText: no
     SPDX-License-Identifier: CC0-1.0
-->

<p align="center">
<img src="https://i.postimg.cc/JhMRf2RZ/claudemods-03-17-2025.gif">

<div align="center">

  <a href="https://www.linux.org" target="_blank"><img src="https://img.shields.io/badge/OS-Linux-e06c75?style=for-the-badge&logo=linux" /></a>

  <a href="https://archlinux.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Arch-56b6c2?style=for-the-badge&logo=arch-linux" /></a>
  <a href="https://ubuntu.com/" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Ubuntu-E95420?style=for-the-badge&logo=Ubuntu" /></a>
  <a href="https://www.debian.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Debian-CE0058?style=for-the-badge&logo=Debian" /></a>
  <a href="https://getfedora.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Fedora-51A2DA?style=for-the-badge&logo=fedora" /></a>
  <a href="https://www.opensuse.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-openSUSE-73BA25?style=for-the-badge&logo=opensuse" /></a>
  <a href="https://alpinelinux.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Alpine-0D597F?style=for-the-badge&logo=alpine-linux" /></a>
  <a href="https://www.gentoo.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Gentoo-54487A?style=for-the-badge&logo=gentoo" /></a>
  <a href="https://voidlinux.org" target="_blank"><img src="https://img.shields.io/badge/DISTRO-Void-478061?style=for-the-badge&logo=void-linux" /></a>

</div>

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

> **Status:** the first builds on Linux (Arch/CachyOS, Qt6) are in progress; compile errors
> are being fixed as they appear, so expect more.
> See [BUILDING.md](BUILDING.md) for where to look. Test only in a virtual machine: the
> partition and bootloader steps change the disk.

## Quick start

```bash
./build.sh                       # asks: new system or clone? GUI, TUI or both?
./build.sh --clone --gui --install
./build.sh --new --tui --package
./build.sh --claudemodsdistrosonline --gui     # claudemods distributions (see below); or --tui / --both
```

`build.sh` detects your distribution, installs the build dependencies, builds with
CMake/Ninja, and can install the result or wrap it into a package. Run `./build.sh --help`
for all options.

### Supported distributions

`build.sh` reads `/etc/os-release` (`ID` and `ID_LIKE`) and picks the package manager and
package names for the family:

| Family | Distributions | Package manager |
|---|---|---|
| Arch | Arch Linux, CachyOS, Manjaro, EndeavourOS, Garuda, KaOS | `pacman` |
| Debian | Debian, Ubuntu, Linux Mint, KDE neon, Pop!_OS, Raspberry Pi OS | `apt` |
| Fedora | Fedora, RHEL, CentOS, Rocky Linux, AlmaLinux, Nobara | `dnf` |
| openSUSE | openSUSE Tumbleweed and Leap, SLES | `zypper` |
| Alpine | Alpine Linux, postmarketOS | `apk` |
| Gentoo | Gentoo | `emerge` |
| Void | Void Linux | `xbps` |

Only the Arch family (CachyOS) has been built so far; the other families' package lists are
untested and may need adjusting for your release. The installer's own `packages` module
supports `pacman`, `apt`, `dnf`, `dnf5`, `yum`, `zypp`, `apk`, `portage`, `entropy`, `luet`,
`packagekit` and `pamac` as backends for the installed system.

`build.sh` also repairs a partly copied source tree before building (for example partition
files that landed in `src/modules/`), keeps going after a failed file, and writes every compile
error to `build/errors.log` (full output in `build/build.log`).

## claudemods distributions (online install)

`./build.sh --claudemodsdistrosonline` (alias `--distro`) builds an installer that sets systems
up the way the old claudemods scripts did: pick a distribution on the **Distribution** page
(GUI and TUI), and the system is bootstrapped with `pacstrap` and customised. Locale, keyboard,
partitioning, users, fstab, initramfs and the bootloader use the regular ClaudeInstall modules.

- `src/modules/distroprofile` – the job module, used twice in `settings-distro.conf`:
  `distroprofile@install` (replaces `unpackfs`) and `distroprofile@customize` (after `users`).
- `profiles/*.conf` – one small file per variant (Spitfire / Apex, minimal / full / black, dev).
  They `extends:` a family file and `claudemods-common.conf`, which holds the shared steps. Step
  types: `copy`, `mkdir`, `remove`, `enable`, `run`, `runAsUser`, `hostRun`, `download`, `regdom`.
- `needed-files/` – your grub / plymouth / SDDM themes, `pacman.conf`, `pacman.d.zip`, tweak
  scripts. **Not included**: add the folder next to `build.sh`; it is installed to
  `/usr/share/claudeinstall/needed-files`.
- `download` steps must give a `sha256` (or `insecure: true`); the claudemods zips are currently
  unpinned.
- Wireless regulatory domain: set `wirelessRegdom: GB` in `distroprofile.conf`, or leave it empty
  to use the country of the chosen time zone.

Runtime needs: `arch-install-scripts`, `curl`, `unzip`. Not yet ported: the CachyOS, Arch and
handheld installers.

## Package lists that change over time

Package lists go stale (renamed, split or dropped packages, 404s). Three layers handle it:

1. The `packages` module refreshes the lists and retries when an install fails, for apt, pacman,
   dnf, dnf5, yum, zypp, apk, xbps and entropy.
2. Packages that the manager reports as no longer existing are skipped and logged; the rest
   still install. `distroprofile` does the same for `pacstrap`.
3. `claudeinstall-pkgfix` (installed to `/usr/bin`) works on the command line: `refresh`,
   `extract` a list from a meta-package `.pkg.tar.zst`, `resolve` names against the live
   repositories (following renames), and `pacstrap` / `install` with retries. Skipped names go to
   `/tmp/claudeinstall-missing.txt`.

## Filesystems

All filesystems the backend can create are offered by default in the GUI and TUI (default first).
`availableFileSystemTypes` in `partition.conf` is `["all"]`; give a list to restrict it, or use
`claudeconfig fs available all|btrfs,ext4,...`.

## C++ standard

CMake uses the newest standard the compiler supports (C++23 with GCC 13+ / Clang 17+, else 20,
else 17). Force one with `-DCLAUDEINSTALL_CXX_STANDARD=17|20|23`. The code needs only C++17.

## Look and colours

The default branding (`src/branding/default`) uses a 1100×520 window with a horizontal
progress bar along the bottom, the `mascot.png` logo, and the stock ClaudeInstall colours and
images.

Change the sidebar colours at any time with the `claudeinstall-colors` command (installed to
`/usr/bin` by `./build.sh --install`; run it with sudo for the system-wide install):

```bash
sudo claudeinstall-colors default       # stock colours
sudo claudeinstall-colors claudemods    # blue bar (#00568f), cyan text (#24ffff)
sudo claudeinstall-colors custom "#00568f" "#24ffff" "#00568f" "#24ffff"   # bg, text, current bg, current text
claudeinstall-colors show               # print the colours in use
```

It edits the `style:` section of the installed `branding.desc`.

## claudeconfig: configure the installer for your distro

`claudeconfig` (installed to `/usr/bin` by `./build.sh --install`) edits
`/etc/claudeinstall/settings.conf` and `/etc/claudeinstall/modules/*.conf` for you. Each
changed file is kept as `FILE.bak`. Run it as root; `claudeconfig --help` lists everything.

```bash
sudo claudeconfig preset claudemods          # install the claudemods set (config/claudemods/)
sudo claudeconfig mode clone                 # clone the live system, no users (or: new)
sudo claudeconfig image erofs /run/arch/bootmnt/LiveOS/rootfs.img   # or: squashfs
sudo claudeconfig image tools                # are unsquashfs / erofs kernel support there?
sudo claudeconfig fs default btrfs           # default filesystem; fs available btrfs,ext4,f2fs,xfs
sudo claudeconfig btrfs compress zstd:22     # or off; also: btrfs ssd/hdd/options/swap-subvol
sudo claudeconfig btrfs subvols default      # or: minimal | none | list | add MP SUBVOL | remove MP
sudo claudeconfig kernel linux-cachyos       # initramfs kernel
sudo claudeconfig service enable sddm.service mandatory
sudo claudeconfig service set-default graphical.target
```

It also covers swap and partitioning defaults, bootloader, users (shell, autologin, groups),
the welcome-page requirements, the finish page and the display manager order. squashfs images
are unpacked with `unsquashfs`; erofs images are mounted (loop) and copied with rsync, so they
need erofs support in the kernel.

The `claudemods` preset is your Calamares configuration adapted to ClaudeInstall's module
names and keys: `services` is now `services-systemd`, and the Manjaro-only `mhwdcfg` and
`postcfg` modules, plus the `kernel`/`img`/`timeout` bootloader keys and a few `users` keys,
do not exist here and are left out.

## Configuration

| File | Purpose |
|---|---|
| `settings.conf` | Module sequence for a new-system install |
| `settings-clone.conf` | Module sequence for cloning the running system |
| `unpackfs-clone.conf` | What to copy (and exclude) when cloning |
| `src/modules/<name>/<name>.conf` | Per-module settings |
| `src/branding/` | Branding: logos, slideshow, strings, sidebar layout |
| `claudeinstall-colors` | Command to switch the sidebar colours |

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
