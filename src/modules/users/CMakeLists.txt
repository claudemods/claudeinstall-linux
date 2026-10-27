# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Core DBus Network)
find_package(Crypt REQUIRED)

# Provide modern alias for -lcrypt
add_library(crypt_crypt INTERFACE)
add_library(crypt::crypt ALIAS crypt_crypt)
if(Crypt_FOUND)
    target_link_libraries(crypt_crypt INTERFACE ${CRYPT_LIBRARIES})
endif()

# Check for crypt_gensalt
if(Crypt_FOUND)
    set(_old_CRL "${CMAKE_REQUIRED_LIBRARIES}")
    list(APPEND CMAKE_REQUIRED_LIBRARIES crypt)
    include(CheckSymbolExists)
    check_symbol_exists(crypt_gensalt crypt.h HAS_CRYPT_GENSALT)
    set(CMAKE_REQUIRED_LIBRARIES "${_old_CRL}")

    if(HAS_CRYPT_GENSALT)
        target_compile_definitions(crypt_crypt INTERFACE HAVE_CRYPT_GENSALT)
    endif()
endif()

# Add optional libraries here
set(USER_EXTRA_LIB
    ${kfname}::CoreAddons
    ${qtname}::DBus
    crypt::crypt
)

find_package(LibPWQuality)
set_package_properties(LibPWQuality PROPERTIES PURPOSE "Extra checks of password quality")

if(LibPWQuality_FOUND)
    list(APPEND USER_EXTRA_LIB ${LibPWQuality_LIBRARIES})
    include_directories(${LibPWQuality_INCLUDE_DIRS})
    add_definitions(-DCHECK_PWQUALITY -DHAVE_LIBPWQUALITY)
endif()

find_package(ICU COMPONENTS uc i18n)
set_package_properties(ICU PROPERTIES PURPOSE "Transliteration support for full name to username conversion")

if(ICU_FOUND)
    list(APPEND USER_EXTRA_LIB ICU::uc ICU::i18n)
    include_directories(${ICU_INCLUDE_DIRS})
    add_definitions(-DHAVE_ICU)
endif()

include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

set(_users_src
    # Jobs
    ActiveDirectoryJob.cpp
    CreateUserJob.cpp
    MiscJobs.cpp
    SetPasswordJob.cpp
    SetHostNameJob.cpp
    # Configuration
    CheckPWQuality.cpp
    Config.cpp
)

# The headless part: the Config and the jobs, shared by all frontends
# (the usersq module uses it too)
claudeinstall_add_config_library(users_config
    SOURCES
        ${_users_src}
    LINK_LIBRARIES
        ${USER_EXTRA_LIB}
)

claudeinstall_add_plugin(users
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        UsersViewStep.cpp
        UsersPage.cpp
    UI
        page_usersetup.ui
    RESOURCES
        users.qrc
    LINK_PRIVATE_LIBRARIES
        users_config
        crypt::crypt
        ${USER_EXTRA_LIB}
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(users
    SOURCES
        UsersTuiStep.cpp
    LINK_LIBRARIES
        users_config
)

if(NOT HAS_CRYPT_GENSALT)
    # Test checks characteristics of the generated hash, but
    # when HAVE_CRYPT_GENSALT is used, the chosen hash is the "best"
    # one -- difficult to set expectations in the tests, so skip it.
    claudeinstall_add_test(userspasswordtest SOURCES TestPasswordJob.cpp SetPasswordJob.cpp LIBRARIES crypt::crypt)
endif()

claudeinstall_add_test(
    usersgroupstest
    SOURCES
        TestGroupInformation.cpp
        ${_users_src} # Build again with test-visibility
    LIBRARIES
        ${kfname}::CoreAddons
        ${qtname}::DBus # HostName job can use DBus to systemd
        crypt::crypt # SetPassword job uses crypt()
        ${USER_EXTRA_LIB}
)

claudeinstall_add_test(
    usershostnametest
    SOURCES
        TestSetHostNameJob.cpp
        SetHostNameJob.cpp
        ${_users_src} # Build again with test-visibility
    LIBRARIES
        ${qtname}::DBus # HostName job can use DBus to systemd
        ${USER_EXTRA_LIB}
)

claudeinstall_add_test(
    userstest
    SOURCES
        Tests.cpp
        ${_users_src} # Build again with test-visibility
    LIBRARIES
        ${kfname}::CoreAddons
        ${qtname}::DBus # HostName job can use DBus to systemd
        crypt::crypt # SetPassword job uses crypt()
        ${USER_EXTRA_LIB}
)
