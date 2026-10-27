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

> **Status:** the first builds on Linux (Arch/CachyOS, Qt6) are in progress; compile errors
> are being fixed as they appear, so expect more.
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

`build.sh` also repairs a partly copied source tree before building (for example partition
files that landed in `src/modules/`), keeps going after a failed file, and writes every compile
error to `build/errors.log` (full output in `build/build.log`).

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
