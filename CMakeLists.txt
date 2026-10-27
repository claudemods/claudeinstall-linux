# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
###
#
#   ClaudeInstall is Free Software: see the License-Identifier above.
#
#   Individual files may have different licenses (like the CMake
#   infrastructure, which is BSD-2-Clause licensed). Check the SPDX
#   identifiers in each file.
#
###
#
# Generally, this CMakeLists.txt will find all the dependencies for ClaudeInstall
# and complain appropriately. See below (later in this file) for CMake-level
# options. There are some "secret" options as well:
#
#   SKIP_MODULES    : a space or semicolon-separated list of directory names
#                     under src/modules that should not be built.
#   USE_<foo>       : fills in SKIP_MODULES for modules called <foo>-<something>.
#   WITH_<foo>      : try to enable <foo> (these usually default to ON). For
#                     a list of WITH_<foo> grep CMakeCache.txt after running
#                     CMake once. These affect the ABI offered by ClaudeInstall.
#                       - QML (enable QML UI View modules, default ON)
#                       - QT6 (use Qt6 rather than Qt5, default to OFF)
#                     The WITH_* options affect the ABI of ClaudeInstall: you must
#                     build (C++) modules for ClaudeInstall with the same WITH_*
#                     settings, or they may not load at all.
#   BUILD_<foo>     : choose additional things to build
#                       - APPDATA (use AppData in packagechooser, requires QtXml)
#                       - APPSTREAM (use AppStream in packagechooser, requires libappstream-qt)
#                       - BUILD_CRASH_REPORTING (uses KCrash, rather than ClaudeInstall internal, for crash reporting)
#                       - SCHEMA_TESTING (uses tools/configvalidator)
#                       - TESTING (standard CMake option)
#   DEBUG_<foo>     : special developer flags for debugging.
#
# Example usage:
#
#   cmake . -DSKIP_MODULES="partition luksbootkeycfg"
#
# To obtain the version number of claudeinstall, run CMake in script mode, e.g.
#   cmake -P CMakeLists.txt

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CLAUDEINSTALL_VERSION 3.3.15)
set(CLAUDEINSTALL_RELEASE_MODE OFF) # Set to ON during a release

if(CMAKE_SCRIPT_MODE_FILE)
    include(${CMAKE_CURRENT_LIST_DIR}/CMakeModules/ExtendedVersion.cmake)
    set(CMAKE_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
    extend_version( ${CLAUDEINSTALL_VERSION} ${CLAUDEINSTALL_RELEASE_MODE}  _vshort _vlong )
    message("${_vlong}")
    return()
endif()

# Massage the version for CMake if there is a version-suffix
string(REGEX REPLACE "-.*" "" CLAUDEINSTALL_VERSION_SHORT "${CLAUDEINSTALL_VERSION}")
# And preserve the original version (suffix and all) because project() overwrites
# .. but if we're doing non-release builds, this gets replaced with git versioning.
set(CLAUDEINSTALL_VERSION_LONG "${CLAUDEINSTALL_VERSION}")

project(CLAUDEINSTALL VERSION ${CLAUDEINSTALL_VERSION_SHORT} LANGUAGES C CXX HOMEPAGE_URL "https://claudeinstall.io/")

if(NOT CLAUDEINSTALL_RELEASE_MODE AND CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build development versions in the source-directory.")
endif()
# ClaudeInstall in the 3.3 series promises ABI compatbility, so it sets a
# .so-version equal to the series number. We use ci/abicheck.sh to
# keep track of this. Note that the **alpha** releases also have
# such an .so-version, but are not ABI-stable yet.
set(CLAUDEINSTALL_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

### OPTIONS
#
option(INSTALL_POLKIT "Install Polkit configuration" ON)
option(INSTALL_COMPLETION "Install shell completions" OFF)
option(INSTALL_CONFIG "Install configuration files" OFF)
# When adding WITH_* that affects the ABI offered by libclaudeinstall,
# also update libclaudeinstall/ClaudeInstallConfig.h.in
option(WITH_QML "Enable QML UI options (needs WITH_GUI)." ON)
# The graphical installer: libclaudeinstallui, the claudeinstall program and the
# Widgets / QML view modules. Switch it off (and WITH_TUI on) to build the
# text-mode installer alone; Qt Widgets, Svg and Quick are then not needed.
option(WITH_GUI "Build the graphical (Widgets / QML) installer." ON)
option(WITH_QT6 "Use Qt6 instead of Qt5" OFF)
#
# Additional parts to build that do not affect ABI
option(BUILD_SCHEMA_TESTING "Enable schema-validation-tests" ON)
# The text-mode installer (claudeinstall-tui), built on FTXUI
option(WITH_TUI "Build the text-mode (terminal) installer, claudeinstall-tui." OFF)
option(FETCH_FTXUI "Download FTXUI at configure time if it is not installed." ON)
# Options for the claudeinstall executable
option(BUILD_CRASH_REPORTING "Enable crash reporting with KCrash." ON)

option(DEBUG_SANITIZERS "Enable sanitizers and Debug build type" OFF)

# Possible debugging flags are:
#  - DEBUG_TIMEZONES draws latitude and longitude lines on the timezone
#    widget and enables chatty debug logging, for dealing with the timezone
#    location database.
#  - DEBUG_FILESYSTEMS does extra logging and checking when looking at
#    partition configuration. Lists known KPMCore FS types.
#  - DEBUG_PARTITION_UNSAFE (see partition/CMakeLists.txt)
#  - DEBUG_PARTITION_BAIL_OUT (see partition/CMakeLists.txt)

### USE_*
#
# By convention, when there are multiple modules that implement similar
# functionality, and it only makes sense to have **at most one** of them
# enabled at any time, those modules are named <foo>-<implementation>.
# For example, services-systemd and services-openrc.
#
# Setting up SKIP_MODULES to ignore "the ones you don't want" can be
# annoying and error-prone (e.g. if a new module shows up). The USE_*
# modules provide a way to do automatic selection. To pick exactly
# one of the implementations from group <foo>, set USE_<foo> to the
# name of the implementation. If USE_<foo> is unset, or empty, then
# all the implementations are enabled (this just means they are
# **available** to `settings.conf`, not that they are used).
#
# To explicitly disable a set of modules, set USE_<foo>=none
# (e.g. the literal string none), which won't match any of the
# modules but is handled specially.
#
# The following USE_* functionalities are available:
#  - *services* picks one of the two service-configuration modules,
#    for either systemd or openrc. This defaults to empty so that
#    **both** modules are available.
set(USE_services "" CACHE STRING "Select the services module to use")

### ClaudeInstall application info
#
set(CLAUDEINSTALL_ORGANIZATION_NAME "ClaudeInstall")
set(CLAUDEINSTALL_ORGANIZATION_DOMAIN "github.com/claudeinstall")
set(CLAUDEINSTALL_APPLICATION_NAME "ClaudeInstall")
set(CLAUDEINSTALL_DESCRIPTION_SUMMARY "The distribution-independent installer framework")

### Transifex (languages) info
#
# complete = 100% translated,
# good = nearly complete (use own judgement, right now >= 75%)
# ok = incomplete (more than 25% untranslated, at least 5% translated),
# incomplete = <5% translated, placeholder in tx; these are not included.
#
# Language en (source language) is added later. It isn't listed in
# Transifex either. Get the list of languages and their status
# from https://app.transifex.com/claudeinstall/claudeinstall/ , or (preferably)
# use tools/txstats (cmake --build <dir> --target txstats) to automatically check.
#
# When adding a new language, take care that it is properly loaded
# by the translation framework. Languages with alternate scripts
# (sr@latin in particular) or location (ca@valencia) need special
# handling in libclaudeinstall/locale/Translation.h .
#
# NOTE: move ie (Interlingue) to _ok once Qt supports it.
# NOTE: update these lines by running `txstats`, or for full automation
#       `txstats -e`. See also
#
# Total 80 languages
set( _tx_complete de en es_AR fi_FI hr hu ja lt tr_TR uk zh_TW )
set( _tx_good az az_AZ be bg ca cs_CZ es fr fur he hi is it_IT ko
    pl pt_BR pt_PT ru si sq sv zh_CN )
set( _tx_ok ar as ast bn ca@valencia da el en_GB eo es_MX et eu fa
    gl id ka ml mr nb nl oc ro sk sl sr sr@latin tg th vi )
set( _tx_incomplete bqi es_PR gu ie ja-Hira kk kn lo lv mk ne_NP
    ro_RO ta_IN te ur uz zh zh_HK )
# Total 80 languages

### Required versions
#
# See DEPENDENCIES section below.

# The default build is with Qt5, but that is increasingly not the
# version installed-by-default on Linux systems. Upgrade the default
# if Qt5 isn't available but Qt6 is. This also saves messing around
# with special CMake flags for every script (e.g. ci/RELEASE.sh and
# ci/abicheck.sh).
if(NOT WITH_QT6)
    find_package(Qt5Core QUIET)
    if(NOT TARGET Qt5::Core)
        find_package(Qt6Core QUIET)
        if(TARGET Qt6::Core)
            message(STATUS "Default Qt version (Qt5) not found, upgrading build to Qt6")
            set(WITH_QT6 ON)
        endif()
    endif()
endif()
if(WITH_QT6)
    message(STATUS "Building ClaudeInstall with Qt6")
    set(qtname "Qt6")
    set(kfname "KF6")
    set(QT_VERSION 6.5.0)
    set(ECM_VERSION 5.240)
    set(KF_VERSION 5.240) # KDE Neon weirdness
    # API that was deprecated before Qt 5.15 causes a compile error
    add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060400)
    # Something to add to filenames for this specific Qt version
    set(QT_VERSION_SUFFIX "-qt6")
else()
    message(STATUS "Building ClaudeInstall with Qt5")
    set(qtname "Qt5")
    set(kfname "KF5")
    set(QT_VERSION 5.15.0)
    set(ECM_VERSION 5.78)
    set(KF_VERSION 5.78)
    # API that was deprecated before Qt 5.15 causes a compile error
    add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x050f00)
    # Something to add to filenames for this specific Qt version
    set(QT_VERSION_SUFFIX "")
endif()

set(YAMLCPP_VERSION 0.5.1)

### CMAKE SETUP
#
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

# Enable IN_LIST
cmake_policy(SET CMP0057 NEW)
# Let ``AUTOMOC`` and ``AUTOUIC`` process ``GENERATED`` files.
cmake_policy(SET CMP0071 NEW)
# Recognize more macros to trigger automoc
list(APPEND CMAKE_AUTOMOC_MACRO_NAMES
        "K_PLUGIN_FACTORY_WITH_JSON"
        "K_EXPORT_PLASMA_DATAENGINE_WITH_JSON"
        "K_EXPORT_PLASMA_RUNNER"
)
if(POLICY CMP0171)
    cmake_policy(SET CMP0177 NEW)
endif()

# CMake Modules
include(CMakePackageConfigHelpers)
include(CTest)
include(FeatureSummary)

# ClaudeInstall Modules
include(CMakeColors)

### C++ SETUP
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "-Og -g")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined -Wl,--fatal-warnings ${CMAKE_SHARED_LINKER_FLAGS}")

# If no build type is set, pick a reasonable one
if(DEBUG_SANITIZERS)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()
if(NOT CMAKE_BUILD_TYPE)
    if(CLAUDEINSTALL_RELEASE_MODE)
        set(CMAKE_BUILD_TYPE "RelWithDebInfo")
    else()
        set(CMAKE_BUILD_TYPE "Debug")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Found Clang ${CMAKE_CXX_COMPILER_VERSION}, setting up Clang-specific compiler flags.")

    # Clang warnings: doing *everything* is counter-productive, since it warns
    # about things which we can't fix (e.g. C++98 incompatibilities, but
    # ClaudeInstall is C++17).
    foreach(
        CLANG_WARNINGS
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
        -Wno-undefined-reinterpret-cast
        -Wno-global-constructors
        -Wno-exit-time-destructors
        -Wno-missing-prototypes
        -Wno-documentation-unknown-command
        -Wno-unknown-warning-option
    )
        string(APPEND CMAKE_CXX_FLAGS " ${CLANG_WARNINGS}")
    endforeach()

    # The dwarf-debugging flags are slightly different, too
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -gdwarf")
    string(APPEND CMAKE_C_FLAGS_DEBUG " -gdwarf")

    # Third-party code where we don't care so much about compiler warnings
    # (because it's uncomfortable to patch) get different flags; use
    #       mark_thirdparty_code( <file> [<file>...] )
    # to switch off warnings for those sources.
    set(SUPPRESS_3RDPARTY_WARNINGS "-Wno-everything")

    set(CMAKE_TOOLCHAIN_PREFIX "llvm-")

    # The path prefix is only relevant for CMake 3.16 and later, fixes #1286
    set(CMAKE_AUTOMOC_PATH_PREFIX OFF)
    set(CLAUDEINSTALL_AUTOMOC_OPTIONS "-butils/moc-warnings.h")
    set(CLAUDEINSTALL_AUTOUIC_OPTIONS --include utils/moc-warnings.h)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor -Woverloaded-virtual")

    set(SUPPRESS_3RDPARTY_WARNINGS "")
endif()

# Use mark_thirdparty_code() to reduce warnings from the compiler
# on code that we're not going to fix. Call this with a list of files.
macro(mark_thirdparty_code)
    set_source_files_properties(
        ${ARGV}
        PROPERTIES COMPILE_FLAGS "${SUPPRESS_3RDPARTY_WARNINGS}" COMPILE_DEFINITIONS "THIRDPARTY"
    )
endmacro()

if(CMAKE_COMPILER_IS_GNUCXX)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9 OR CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 4.9)
        message(STATUS "Found GNU g++ ${CMAKE_CXX_COMPILER_VERSION}, enabling colorized error messages.")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")
    endif()
    if(DEBUG_SANITIZERS)
        message(STATUS "Setting up sanitizers")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=address -g -O1")
    endif()
endif()

### DEPENDENCIES
#
if(NOT WITH_GUI)
    if(NOT WITH_TUI)
        message(FATAL_ERROR "WITH_GUI and WITH_TUI are both off: there would be nothing to build.")
    endif()
    # QML is a part of the GUI
    set(WITH_QML OFF)
endif()

# QtGui is needed even by the text-mode installer (colours in the models of the
# partition module); it works without a window system.
find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Concurrent Core DBus Gui LinguistTools Network)
if(WITH_GUI)
    find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Svg Widgets)
endif()
if(WITH_QML)
    find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Quick QuickWidgets)
endif()
# Note that some modules need more Qt modules, optionally.

if(WITH_TUI)
    # The targets are ftxui::screen, ftxui::dom and ftxui::component
    find_package(ftxui 5 CONFIG QUIET)
    if(NOT ftxui_FOUND)
        if(NOT FETCH_FTXUI)
            message(FATAL_ERROR "WITH_TUI needs FTXUI (>= 5); install it or set FETCH_FTXUI=ON")
        endif()
        include(FetchContent)
        set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(ftxui GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git GIT_TAG v5.0.0)
        FetchContent_MakeAvailable(ftxui)
    endif()
endif()

find_package(YAMLCPP ${YAMLCPP_VERSION})
set_package_properties(
    YAMLCPP
    PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "YAML parser for C++"
    PURPOSE "Parsing of configuration files"
)

find_package(Polkit${qtname}-1)
if(INSTALL_POLKIT)
    set_package_properties(
        Polkit${qtname}-1
        PROPERTIES
        TYPE REQUIRED
    )
endif()
set_package_properties(
    Polkit${qtname}-1
    PROPERTIES
    DESCRIPTION "${qtname} support for Polkit"
    URL "https://cgit.kde.org/polkit-qt-1.git"
    PURPOSE "Polkit${qtname}-1 helps with installing Polkit configuration"
)

# Find ECM once, and add it to the module search path; ClaudeInstall
# modules that need ECM can do
#   if(ECM_FOUND)
# no need to mess with the module path after.
find_package(ECM ${ECM_VERSION} NO_MODULE)
if(ECM_FOUND)
    message(STATUS "Found KDE ECM ${ECM_MODULE_PATH}")
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
    if(BUILD_TESTING)
        # ECM implies that we can build the tests, too
        find_package(${qtname} COMPONENTS Test REQUIRED)
        include(ECMAddTests)
    endif()
    include(KDEInstallDirs)
endif()

find_package(${kfname}CoreAddons ${KF_VERSION} QUIET)
set_package_properties(
    ${kfname}CoreAddons
    PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "KDE Framework CoreAddons"
    URL "https://api.kde.org/frameworks/"
    PURPOSE "Essential Framework for AboutData and Macros"
)

# After this point, there should be no REQUIRED find_packages,
# since we want tidy reporting of optional dependencies.
feature_summary(
    WHAT REQUIRED_PACKAGES_NOT_FOUND
    FATAL_ON_MISSING_REQUIRED_PACKAGES
    DESCRIPTION "The following REQUIRED packages were not found:"
    QUIET_ON_EMPTY
)

#
# OPTIONAL DEPENDENCIES
#
# First, set KF back to optional so that any missing components don't trip us up.
find_package(${kfname}Crash ${KF_VERSION} QUIET)
set_package_properties(
    ${kfname}Crash
    PROPERTIES
    TYPE OPTIONAL
    DESCRIPTION "KDE Framework Crash"
    URL "https://api.kde.org/frameworks/"
    PURPOSE "Framework for sending Crash Dumps"
)

if(NOT TARGET ${kfname}::Crash)
    if(BUILD_CRASH_REPORTING)
        message(WARNING "BUILD_CRASH_REPORTING is set, but ${kfname}::Crash is not available.")
    endif()
    set(BUILD_CRASH_REPORTING OFF)
endif()

# The config-file schema tests use tools/configvalidator (C++, needs only yaml-cpp,
# which is found already), so there is nothing to look for here.
add_feature_info(yaml-schema BUILD_SCHEMA_TESTING "Validate YAML (config files) with schema (tools/configvalidator).")
# Now we know the state of the ABI-options, copy them into "ClaudeInstall_"
# prefixed variables, to match how the variables would-be-named
# when building out-of-tree.
set(ClaudeInstall_WITH_QML ${WITH_QML})
set(ClaudeInstall_WITH_QT6 ${WITH_QT6})

### Transifex Translation status
#
# Construct language lists for use. This massages the language lists if
# needed and checks for some obvious errors. The actual work of
# compiling translations is done in the lang/ directory.
#

set(curr_tx ${_tx_complete} ${_tx_good} ${_tx_ok} ${_tx_incomplete})
set(tx_errors OFF)
if(curr_tx)
    # New in list
    foreach(l ${curr_tx})
        set(p_l "lang/claudeinstall_${l}.ts")
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${p_l})
            message(WARNING "Language ${l} has no .ts file yet.")
            set(tx_errors ON)
        endif()
    endforeach()

    unset(p_l)
    unset(l)
endif()
unset(curr_tx)
if(tx_errors)
    message(FATAL_ERROR "Translation warnings, see above.")
endif()

set(CLAUDEINSTALL_TRANSLATION_LANGUAGES en ${_tx_complete} ${_tx_good} ${_tx_ok})
if(NOT CLAUDEINSTALL_RELEASE_MODE)
    # Outside of release mode, enable all the languages
    list(APPEND CLAUDEINSTALL_TRANSLATION_LANGUAGES ${_tx_incomplete})
endif()
list(SORT CLAUDEINSTALL_TRANSLATION_LANGUAGES)
list(REMOVE_DUPLICATES CLAUDEINSTALL_TRANSLATION_LANGUAGES)

add_subdirectory(lang) # i18n tools

### Example Distro
#
# For testing purposes ClaudeInstall includes a very, very, limited sample
# distro called "Generic". The root filesystem of "Generic" lives in
# data/example-root and can be squashed up as part of the build, so
# that a pure-upstream run of ./claudeinstall -d from the build directory
# (with all the default settings and configurations) can actually
# do an complete example run.
#
# Some binaries from the build host (e.g. /bin and /lib) are also
# squashed into the example filesystem.
#
# To build the example distro (for use by the default, example,
# unsquashfs module), build the target *example-distro*, eg.:
#
#   make example-distro
#
find_program(mksquashfs_PROGRAM mksquashfs)
if(mksquashfs_PROGRAM)
    set(mksquashfs_FOUND ON)
    set(src_fs ${CMAKE_SOURCE_DIR}/data/example-root/)
    set(dst_fs ${CMAKE_BINARY_DIR}/example.sqfs)
    if(EXISTS ${src_fs})
        # based on the build host. If /lib64 exists, assume it is needed.
        # Collect directories needed for a minimal binary distro,
        # Note that the last path component is added to the root, so
        # if you add /usr/sbin here, it will be put into /sbin_1.
        # Add such paths to /etc/profile under ${src_fs}.
        set(candidate_fs /sbin /bin /lib /lib64)
        set(host_fs "")
        foreach(c_fs ${candidate_fs})
            if(EXISTS ${c_fs})
                list(APPEND host_fs ${c_fs})
            endif()
        endforeach()
        add_custom_command(
            OUTPUT ${dst_fs}
            COMMAND ${mksquashfs_PROGRAM} ${src_fs} ${dst_fs} -all-root
            COMMAND ${mksquashfs_PROGRAM} ${host_fs} ${dst_fs} -all-root
        )
        add_custom_target(example-distro DEPENDS ${dst_fs})
    endif()
else()
    set(mksquashfs_FOUND OFF)
endif()
# Doesn't list mksquashfs as an optional dep, though, because it
# hasn't been sent through the find_package() scheme.
#
# "http://tldp.org/HOWTO/SquashFS-HOWTO/creatingandusing.html"
add_feature_info(ExampleDistro ${mksquashfs_FOUND} "Create example-distro target.")

### CLAUDEINSTALL PROPER
#

# Additional info for non-release builds. The "extended" version information
# with date and git information (commit, dirty status) is used only
# by ClaudeInstallVersionX.h, which is included by consumers that need a full
# version number with all that information; normal consumers can include
# ClaudeInstallVersion.h with more stable numbers.
if(NOT CLAUDEINSTALL_RELEASE_MODE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/")
    include(ExtendedVersion)
    extend_version( "${CLAUDEINSTALL_VERSION}" OFF CLAUDEINSTALL_VERSION_SHORT CLAUDEINSTALL_VERSION_LONG )
endif()

# Special define for RC (e.g. not-a-release) builds.
# This is consumed via the ClaudeInstallConfig.h header.
if(NOT CLAUDEINSTALL_RELEASE_MODE)
    set(CLAUDEINSTALL_VERSION_RC 1)
endif()

# enforce using constBegin, constEnd for const-iterators
add_definitions(-DQT_STRICT_ITERATORS -DQT_SHARED -DQT_SHAREDPOINTER_TRACK_POINTERS)

# set paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Better default installation paths: GNUInstallDirs defines
# CMAKE_INSTALL_FULL_SYSCONFDIR to be CMAKE_INSTALL_PREFIX/etc by default
# but we really want /etc
if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()

# make predefined install dirs available everywhere
include(GNUInstallDirs)

# This is used by ClaudeInstallAddLibrary; once ClaudeInstall is installed,
# the ClaudeInstallConfig.cmake module sets this variable to the IMPORTED
# libraries for ClaudeInstall.
set(ClaudeInstall_LIBRARIES claudeinstall)

add_subdirectory(3rdparty/kdsingleapplication)
add_subdirectory(tools)
add_subdirectory(src)

add_feature_info(Gui ${WITH_GUI} "Graphical installer (claudeinstall)")
add_feature_info(Qml ${WITH_QML} "QML UI support")
add_feature_info(Tui ${WITH_TUI} "Text-mode installer (claudeinstall-tui)")
add_feature_info(Polkit ${INSTALL_POLKIT} "Install Polkit files")
add_feature_info(KCrash ${BUILD_CRASH_REPORTING} "Crash dumps via KCrash")

### Post-source configuration
#
#
find_package(${kfname} ${KF_VERSION} QUIET COMPONENTS CoreAddons)

### CMake infrastructure installation
#
#
set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/ClaudeInstall" CACHE PATH "Installation directory for CMake files")
set(CMAKE_INSTALL_FULL_CMAKEDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_CMAKEDIR}")

export(PACKAGE ClaudeInstall)
configure_package_config_file(
    "ClaudeInstallConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/ClaudeInstallConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_DATADIR
)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/ClaudeInstallConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(EXPORT ClaudeInstall DESTINATION "${CMAKE_INSTALL_CMAKEDIR}" FILE "ClaudeInstallTargets.cmake" NAMESPACE ClaudeInstall::)

# Install the cmake files
install(
    FILES
        "${PROJECT_BINARY_DIR}/ClaudeInstallConfig.cmake"
        "${PROJECT_BINARY_DIR}/ClaudeInstallConfigVersion.cmake"
        "CMakeModules/ClaudeInstallAddBrandingSubdirectory.cmake"
        "CMakeModules/ClaudeInstallAddConfigLibrary.cmake"
        "CMakeModules/ClaudeInstallAddLibrary.cmake"
        "CMakeModules/ClaudeInstallAddModuleSubdirectory.cmake"
        "CMakeModules/ClaudeInstallAddPlugin.cmake"
        "CMakeModules/ClaudeInstallAddTest.cmake"
        "CMakeModules/ClaudeInstallAddTranslations.cmake"
        "CMakeModules/ClaudeInstallAddTuiPlugin.cmake"
        "CMakeModules/ClaudeInstallAutomoc.cmake"
        "CMakeModules/ClaudeInstallCheckModuleSelection.cmake"
        "CMakeModules/CMakeColors.cmake"
        "CMakeModules/FindYAMLCPP.cmake"
    DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
)

### Miscellaneous installs
#
#
if(INSTALL_POLKIT AND WITH_GUI)
    install(FILES com.github.claudeinstall.claudeinstall.policy DESTINATION "${POLKITQT-1_POLICY_FILES_INSTALL_DIR}")
endif()

if(INSTALL_COMPLETION)
    if(NOT CMAKE_INSTALL_BASHCOMPLETIONDIR)
        set(CMAKE_INSTALL_BASHCOMPLETIONDIR "${CMAKE_INSTALL_DATADIR}/bash-completion/completions")
    endif()

    install(FILES ${CMAKE_SOURCE_DIR}/data/completion/bash/claudeinstall DESTINATION "${CMAKE_INSTALL_BASHCOMPLETIONDIR}")
endif()

if(WITH_GUI)
    install(FILES claudeinstall.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

install(FILES man/claudeinstall.8 DESTINATION ${CMAKE_INSTALL_MANDIR}/man8/)

if(INSTALL_CONFIG)
    install(FILES settings.conf DESTINATION share/claudeinstall)
endif()

### Uninstall
#
#
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE
    @ONLY
)

add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

### Developer convenience
#
# The module support files -- .desc files, .conf files -- are copied into the build
# directory so that it is possible to run `claudeinstall -d` from there. Copy the
# top-level settings.conf as well, into the build directory.
if(settings.conf IS_NEWER_THAN ${CMAKE_BINARY_DIR}/settings.conf)
    configure_file(settings.conf ${CMAKE_BINARY_DIR}/settings.conf COPYONLY)
endif()



### CMAKE SUMMARY REPORT
#
get_directory_property(SKIPPED_MODULES DIRECTORY src/modules DEFINITION LIST_SKIPPED_MODULES)
claudeinstall_explain_skipped_modules( ${SKIPPED_MODULES} )

feature_summary(WHAT ENABLED_FEATURES DESCRIPTION "The following features are enabled" QUIET_ON_EMPTY)
feature_summary(WHAT DISABLED_FEATURES DESCRIPTION "The following features have been disabled:" QUIET_ON_EMPTY)
feature_summary(
    WHAT OPTIONAL_PACKAGES_NOT_FOUND
    DESCRIPTION "The following OPTIONAL packages were not found:"
    QUIET_ON_EMPTY
)
feature_summary(
    WHAT REQUIRED_PACKAGES_NOT_FOUND
    FATAL_ON_MISSING_REQUIRED_PACKAGES
    DESCRIPTION "The following REQUIRED packages were not found:"
    QUIET_ON_EMPTY
)

### PACKAGING
#
# Note: most distro's will do distro-specific packaging rather than
#       using CPack, and this duplicates information in the AppStream, too.
set(CPACK_PACKAGE_VENDOR claudeinstall)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Linux system installer")
set(CPACK_PACKAGE_DESCRIPTION
    "ClaudeInstall is a Linux system installer, intended for Linux distributions to use on their ISOs and other bootable media to install the distribution to the end-user's computer. ClaudeInstall can also be used as an OEM configuration tool. It is modular, extensible and highly-configurable for Linux distributions from all five major Linux families."
)
set(CPACK_PACKAGE_ICON "data/images/squid.png")

include(CPack)
