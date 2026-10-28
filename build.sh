#!/bin/sh
# SPDX-FileCopyrightText: no
# SPDX-License-Identifier: CC0-1.0
#
# build.sh - install build dependencies for the running distro and build ClaudeInstall.
#
# Usage: ./build.sh [options]
#   --deps-only      only install build dependencies
#   --no-deps        skip installing dependencies
#   --new | --clone | --claudemodsdistrosonline   new-system installer, clone of the running system,
#                    or the claudemods distributions installed online (pacstrap + profile; works with
#                    --gui, --tui or --both; needs arch-install-scripts, curl, unzip; --distro = same)
#   --gui | --tui | --both   which interface (GUI is always Qt6)
#   --test           try the installer on a machine whose only disk holds the running
#                    system (it is normally hidden, which blocks the first page).
#                    Partitioning is NOT carried out in this build.
#   (with none of those options, the script asks)
#   --install        run "install" afterwards (uses sudo if not root)
#   --package        also create a distro package (deb / rpm / pkg.tar via cpack or makepkg-less tarball)
#   --clean          remove the build directory first
#   -h, --help       this text
#
# Environment: BUILDDIR (default ./build), CMAKE_ARGS (extra cmake arguments),
#              JOBS (parallel jobs, default: all cores)
#
# NOTE: package names differ per distro and version; the lists below are
# best-effort. If a package is not found, adjust it for your release.

set -e

SRCDIR=$(cd "$(dirname "$0")" && pwd)
BUILDDIR=${BUILDDIR:-$SRCDIR/build}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

DEPS=1 BUILD=1 TUI= GUI= QT6=ON INSTALL=0 PACKAGE=0 CLEAN=0 MODE=
for a in "$@"; do
  case $a in
    --deps-only) BUILD=0 ;;
    --no-deps)   DEPS=0 ;;
    --tui)       GUI=OFF TUI=ON ;;
    --gui)       GUI=ON TUI=OFF ;;
    --both)      GUI=ON TUI=ON ;;
    --new)       MODE=new ;;
    --clone)     MODE=clone ;;
    --claudemodsdistrosonline|--distro) MODE=distro ;;
    --test)      CMAKE_ARGS="$CMAKE_ARGS -DDEBUG_PARTITION_UNSAFE=ON" ;;
    --install)   INSTALL=1 ;;
    --package)   PACKAGE=1 ;;
    --clean)     CLEAN=1 ;;
    -h|--help)   sed -n '2,27p' "$0"; exit 0 ;;
    *) echo "Unknown option: $a" >&2; exit 1 ;;
  esac
done

SUDO=
[ "$(id -u)" -ne 0 ] && SUDO=sudo

# ---- questions (skipped when the matching option was given) -----------------
ask() { # ask VAR "question" "1) a" "2) b" ...
  _var=$1; _q=$2; shift 2
  echo; echo "$_q"; for _o in "$@"; do echo "  $_o"; done
  printf "Choice [1]: "; read -r _c || _c=1
  case ${_c:-1} in 2) _r=2 ;; 3) _r=3 ;; *) _r=1 ;; esac
  eval "$_var=$_r"
}
if [ -z "$MODE" ]; then
  ask _m "What is this installer for?" "1) New system setup (create users, install packages)" "2) Cloned system (copy this running system; no new users, no package install)" "3) claudemods distributions (pacstrap + profile: Spitfire / Apex ...)"
  case $_m in 2) MODE=clone ;; 3) MODE=distro ;; *) MODE=new ;; esac
fi
if [ -z "$GUI" ]; then
  ask _u "Which interface?" "1) GUI (Qt6)" "2) TUI (terminal)" "3) Both"
  case $_u in 2) GUI=OFF TUI=ON ;; 3) GUI=ON TUI=ON ;; *) GUI=ON TUI=OFF ;; esac
fi
echo ":: Mode: $MODE, GUI=$GUI, TUI=$TUI (Qt6)"

# Install the chosen configuration under $1 (a root, e.g. DESTDIR or empty).
install_config() {
  _d="$1/etc/claudeinstall"
  $SUDO mkdir -p "$_d/modules"
  # the colour command: claudeinstall-colors {default|claudemods|custom ...|show}
  $SUDO install -Dm755 "$SRCDIR/claudeinstall-colors" "$1/usr/bin/claudeinstall-colors"
  # claudeconfig: settings command, plus the claudemods preset it can install
  $SUDO install -Dm755 "$SRCDIR/claudeconfig" "$1/usr/bin/claudeconfig"
  # claudeinstall-pkgfix: refresh/resolve package lists so stale names don't break installs
  $SUDO install -Dm755 "$SRCDIR/claudeinstall-pkgfix" "$1/usr/bin/claudeinstall-pkgfix"
  for _pf in "$SRCDIR"/config/*/settings.conf "$SRCDIR"/config/*/modules/*.conf; do
    [ -f "$_pf" ] || continue
    _rel=${_pf#"$SRCDIR"/config/}
    $SUDO install -Dm644 "$_pf" "$1/usr/share/claudeinstall/presets/$_rel"
  done
  if [ "$MODE" = distro ]; then
    # claudemods distributions: pacstrap + profile (modules/distroprofile), chosen on the "Distribution" page
    $SUDO cp "$SRCDIR/settings-distro.conf" "$_d/settings.conf"
    $SUDO install -Dm644 "$SRCDIR/packagechooser-distro.conf" "$_d/modules/packagechooser-distro.conf"
    $SUDO install -Dm644 "$SRCDIR/src/modules/distroprofile/distroprofile.conf" "$_d/modules/distroprofile.conf"
    $SUDO install -Dm644 "$SRCDIR/src/modules/distroprofile/distroprofile-customize.conf" "$_d/modules/distroprofile-customize.conf"
    for _p in "$SRCDIR"/profiles/*.conf; do
      $SUDO install -Dm644 "$_p" "$1/usr/share/claudeinstall/profiles/$(basename "$_p")"
    done
    $SUDO install -Dm644 "$SRCDIR/packagechooser-regdom.conf" "$_d/modules/packagechooser-regdom.conf"
    # the tile pictures of the Distribution and Wireless region pages
    for _i in "$SRCDIR"/images/*.png; do
      $SUDO install -Dm644 "$_i" "$1/usr/share/claudeinstall/images/$(basename "$_i")"
    done
    # The small local files (pacman.conf, pacman.d.zip, tweak scripts ...) come from needed-files/
    # next to this script; the big theme / tool zips are downloaded by the profiles at install time.
    # Elsewhere: NEEDED_FILES=/path/to/needed-files ./build.sh --claudemodsdistrosonline ...
    _nf="${NEEDED_FILES:-$SRCDIR/needed-files}"
    if [ -d "$_nf" ]; then
      $SUDO mkdir -p "$1/usr/share/claudeinstall/needed-files"
      $SUDO cp -a "$_nf/." "$1/usr/share/claudeinstall/needed-files/"
    else
      echo "! $_nf not found: the profiles' copy steps will fail. Put needed-files/ next to build.sh or set NEEDED_FILES=/path" >&2
    fi
    $SUDO install -Dm755 "$SRCDIR/claudeinstall-pkgfix" "$1/usr/bin/claudeinstall-pkgfix"
  elif [ "$MODE" = clone ]; then
    $SUDO cp "$SRCDIR/settings-clone.conf" "$_d/settings.conf"
    $SUDO cp "$SRCDIR/unpackfs-clone.conf" "$_d/modules/unpackfs-clone.conf"
  else
    $SUDO cp "$SRCDIR/settings.conf" "$_d/settings.conf"
  fi
}

# ---- distro detection -------------------------------------------------------
. /etc/os-release 2>/dev/null || { echo "! no /etc/os-release" >&2; exit 1; }
FAMILY=
for id in $ID $ID_LIKE; do
  case $id in
    debian|ubuntu|linuxmint|neon|pop|raspbian) FAMILY=debian; break ;;
    fedora|rhel|centos|rocky|almalinux|nobara)  FAMILY=fedora; break ;;
    opensuse*|suse|sles)                         FAMILY=suse; break ;;
    arch|manjaro|endeavouros|garuda|cachyos|kaos) FAMILY=arch; break ;;
    alpine|postmarketos)                         FAMILY=alpine; break ;;
    gentoo)                                      FAMILY=gentoo; break ;;
    void)                                        FAMILY=void; break ;;
  esac
done
[ -n "$FAMILY" ] || { echo "! Unsupported distro: $ID ($ID_LIKE)" >&2; exit 1; }
echo ":: Distro: $PRETTY_NAME (family: $FAMILY)"
if [ "$MODE" = distro ] && [ "$FAMILY" != arch ]; then
  echo "! --claudemodsdistrosonline installs with pacstrap and must be built and run on an Arch-family system" >&2
  exit 1
fi

# ---- dependencies -----------------------------------------------------------
install_deps() {
  case $FAMILY in
  debian)
    $SUDO apt-get update
    if [ $QT6 = ON ]; then
      QT="qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-svg-dev qt6-declarative-dev libkf6coreaddons-dev libkf6crash-dev libkf6i18n-dev libkf6widgetsaddons-dev libpolkit-qt6-1-dev"
    else
      QT="qtbase5-dev qtbase5-private-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev qtdeclarative5-dev libkf5coreaddons-dev libkf5crash-dev libkf5i18n-dev libkf5widgetsaddons-dev libpolkit-qt5-1-dev"
    fi
    $SUDO apt-get -y install build-essential cmake ninja-build git pkg-config gettext extra-cmake-modules \
      libyaml-cpp-dev libkpmcore-dev libparted-dev libicu-dev libatasmart-dev libpwquality-dev os-prober \
      rsync squashfs-tools $QT
    ;;
  fedora)
    if [ $QT6 = ON ]; then
      QT="qt6-qtbase-devel qt6-qtbase-private-devel qt6-qttools-devel qt6-linguist qt6-qtsvg-devel qt6-qtdeclarative-devel kf6-kcoreaddons-devel kf6-kcrash-devel kf6-ki18n-devel kf6-kwidgetsaddons-devel kf6-kconfig-devel kf6-kservice-devel polkit-qt6-1-devel"
    else
      QT="qt5-qtbase-devel qt5-qtbase-private-devel qt5-qttools-devel qt5-linguist qt5-qtsvg-devel qt5-qtdeclarative-devel kf5-kcoreaddons-devel kf5-kcrash-devel kf5-ki18n-devel kf5-kwidgetsaddons-devel polkit-qt5-1-devel"
    fi
    $SUDO dnf -y install gcc-c++ make cmake ninja-build git gettext extra-cmake-modules yaml-cpp-devel \
      kpmcore-devel parted-devel libicu-devel libatasmart-devel libpwquality-devel rsync squashfs-tools $QT
    ;;
  suse)
    if [ $QT6 = ON ]; then
      QT="qt6-base-devel qt6-base-private-devel qt6-tools-devel qt6-svg-devel qt6-declarative-devel kf6-extra-cmake-modules kf6-kcoreaddons-devel kf6-kcrash-devel kf6-ki18n-devel kf6-kwidgetsaddons-devel polkit-qt6-1-devel"
    else
      QT="libqt5-qtbase-devel libqt5-qtbase-private-headers-devel libqt5-linguist-devel libqt5-qtsvg-devel libqt5-qtdeclarative-devel extra-cmake-modules kcoreaddons-devel kcrash-devel ki18n-devel kwidgetsaddons-devel polkit-qt5-1-devel"
    fi
    $SUDO zypper --non-interactive in gcc-c++ make cmake ninja git gettext-tools yaml-cpp-devel \
      kpmcore-devel parted-devel libicu-devel libatasmart-devel libpwquality-devel rsync squashfs $QT
    ;;
  arch)
    if [ $QT6 = ON ]; then
      QT="qt6-base qt6-tools qt6-svg qt6-declarative kcoreaddons kcrash ki18n kwidgetsaddons polkit-qt6"
    else
      QT="qt5-base qt5-tools qt5-svg qt5-declarative kcoreaddons5 kcrash5 ki18n5 kwidgetsaddons5 polkit-qt5"
    fi
    # --distro also needs pacstrap (arch-install-scripts), curl and unzip at install time
    $SUDO pacman -Sy --noconfirm --needed base-devel cmake ninja git extra-cmake-modules yaml-cpp kpmcore \
      parted icu libatasmart libpwquality rsync squashfs-tools arch-install-scripts curl unzip $QT
    ;;
  alpine)
    $SUDO apk add build-base cmake samurai git gettext-dev extra-cmake-modules yaml-cpp-dev kpmcore-dev \
      parted-dev icu-dev libatasmart-dev libpwquality-dev rsync squashfs-tools \
      qt6-qtbase-dev qt6-qttools-dev qt6-qtsvg-dev qt6-qtdeclarative-dev kcoreaddons-dev kcrash-dev ki18n-dev kwidgetsaddons-dev
    ;;
  gentoo)
    $SUDO emerge --noreplace dev-build/cmake dev-build/ninja dev-vcs/git kde-frameworks/extra-cmake-modules \
      dev-cpp/yaml-cpp kde-apps/kpmcore dev-qt/qtbase dev-qt/qttools kde-frameworks/kcoreaddons \
      kde-frameworks/ki18n kde-frameworks/kwidgetsaddons net-misc/rsync sys-fs/squashfs-tools
    ;;
  void)
    $SUDO xbps-install -Sy base-devel cmake ninja git gettext extra-cmake-modules yaml-cpp-devel kpmcore-devel \
      parted-devel icu-devel libatasmart-devel libpwquality-devel rsync squashfs-tools \
      qt6-base-devel qt6-tools-devel qt6-svg-devel qt6-declarative-devel kcoreaddons-devel ki18n-devel kwidgetsaddons-devel
    ;;
  esac
}

[ $DEPS = 1 ] && install_deps
[ $BUILD = 0 ] && exit 0

# ---- source tree sanity check ----------------------------------------------
# A partial upload (e.g. GitHub's 100-file limit) or an overlay of an older tree
# gives "Cannot find source file" errors from CMake. Catch that up front.
#
# The partition module's top-level files can end up in src/modules/ when a tree
# is uploaded or copied in pieces (they are then picked up as bogus "modules" and
# the partition module has no config). Move them back where they belong.
_M="$SRCDIR/src/modules"; _P="$_M/partition"
if [ -d "$_P" ]; then
  for _f in Config.cpp Config.h PartitionTuiStep.cpp PartitionTuiStep.h \
            PartitionViewStep.cpp PartitionViewStep.h partition.conf partition.schema.yaml jobs; do
    if [ -e "$_M/$_f" ]; then
      if [ -e "$_P/$_f" ]; then rm -rf "$_M/$_f"; else mv "$_M/$_f" "$_P/$_f"; fi
      echo ":: Repaired tree: src/modules/$_f -> src/modules/partition/$_f"
    fi
  done
  # partition's tests/ directory, misplaced as src/modules/tests/
  if [ -d "$_M/tests" ]; then
    # merge (never overwrite what is already there), then drop the misplaced copy
    mkdir -p "$_P/tests" && cp -an "$_M/tests/." "$_P/tests/" && rm -rf "$_M/tests"
    echo ":: Repaired tree: removed misplaced src/modules/tests"
  fi
  rm -f "$_M/?"
fi
_bad=0
for _cml in "$SRCDIR"/src/modules/*/CMakeLists.txt; do
  _md=$(dirname "$_cml")
  for _s in $(grep -oE '[A-Za-z0-9_./-]+\.(cpp|ui|qrc)' "$_cml" | grep -v '^\.\./' | grep -v '^/'); do
    if [ ! -e "$_md/$_s" ] && [ ! -e "$SRCDIR/$_s" ]; then
      echo "! Missing source: ${_md#$SRCDIR/}/$_s" >&2; _bad=1
    fi
  done
done
[ $_bad = 0 ] || { echo "! Fix the tree (re-copy the missing files from the full project) and rerun." >&2; exit 1; }

# ---- build ------------------------------------------------------------------
[ $CLEAN = 1 ] && rm -rf "$BUILDDIR"
GEN=Ninja
command -v ninja >/dev/null 2>&1 || command -v samu >/dev/null 2>&1 || GEN="Unix Makefiles"

# shellcheck disable=SC2086
cmake -S "$SRCDIR" -B "$BUILDDIR" -G "$GEN" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DINSTALL_CONFIG=ON -DWITH_QT6=$QT6 -DWITH_GUI=$GUI -DWITH_TUI=$TUI $CMAKE_ARGS
# Keep going after a failed file (ninja -k 0) so one run lists every compile error.
if [ "$GEN" = Ninja ]; then
  cmake --build "$BUILDDIR" -j "$JOBS" -- -k 0 2>&1 | tee "$BUILDDIR/build.log"
  [ "${PIPESTATUS:-0}" = 0 ] || true
  grep -E 'error:|FAILED:' "$BUILDDIR/build.log" > "$BUILDDIR/errors.log" || true
  if [ -s "$BUILDDIR/errors.log" ]; then
    echo; echo "! Build failed. All errors are in $BUILDDIR/errors.log (full log: $BUILDDIR/build.log)"; exit 1
  fi
else
  cmake --build "$BUILDDIR" -j "$JOBS"
fi

if [ $INSTALL = 1 ]; then
  $SUDO cmake --install "$BUILDDIR"
  install_config ""
fi

if [ $PACKAGE = 1 ]; then
  # Stage into a DESTDIR and wrap as a distro package when the tool exists.
  STAGE="$BUILDDIR/stage"; rm -rf "$STAGE"
  DESTDIR="$STAGE" cmake --install "$BUILDDIR"
  SUDO= install_config "$STAGE"
  VER=$(sed -n 's/^ *VERSION *\([0-9.]*\).*/\1/p' "$SRCDIR/CMakeLists.txt" | head -1); VER=${VER:-0.1}
  case $FAMILY in
    debian) command -v fpm >/dev/null && fpm -s dir -t deb -n claudeinstall -v "$VER" -C "$STAGE" . \
              || tar -C "$STAGE" -czf "$BUILDDIR/claudeinstall-$VER.tar.gz" . ;;
    fedora|suse) command -v fpm >/dev/null && fpm -s dir -t rpm -n claudeinstall -v "$VER" -C "$STAGE" . \
              || tar -C "$STAGE" -czf "$BUILDDIR/claudeinstall-$VER.tar.gz" . ;;
    arch) command -v fpm >/dev/null && fpm -s dir -t pacman -n claudeinstall -v "$VER" -C "$STAGE" . \
              || tar -C "$STAGE" -czf "$BUILDDIR/claudeinstall-$VER.tar.gz" . ;;
    *) tar -C "$STAGE" -czf "$BUILDDIR/claudeinstall-$VER.tar.gz" . ;;
  esac
  echo ":: Package output is in $PWD and $BUILDDIR (fpm needed for deb/rpm/pacman; tarball otherwise)"
fi

if [ "$MODE" = distro ]; then
  echo ":: claudemods distros online: run  sudo claudeinstall  (GUI)  or  sudo claudeinstall-tui  (TUI)"
fi
echo ":: Done. GUI: $BUILDDIR/src/claudeinstall/claudeinstall  TUI: $BUILDDIR/src/claudeinstall-tui/claudeinstall-tui"
