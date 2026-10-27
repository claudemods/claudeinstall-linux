# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED DBus Network)

find_package(LIBPARTED)
if(LIBPARTED_FOUND)
    set(PARTMAN_SRC checker/partman_devices.c)
    set(PARTMAN_LIB ${LIBPARTED_LIBRARY})
else()
    set(PARTMAN_SRC)
    set(PARTMAN_LIB)
    add_definitions(-DWITHOUT_LIBPARTED)
endif()

# The headless part: the Config and the requirements checker, shared by
# all frontends. (The check that the screen is large enough is not part of
# it: a frontend with a screen supplies that, see checker/ScreenCheck.h.)
claudeinstall_add_config_library(welcome_config
    SOURCES
        checker/GeneralRequirements.cpp
        ${PARTMAN_SRC}
        Config.cpp
    LINK_LIBRARIES
        ${PARTMAN_LIB}
        ${qtname}::DBus
        ${qtname}::Network
)

claudeinstall_add_plugin(welcome
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        checker/CheckerContainer.cpp
        checker/ResultDelegate.cpp
        checker/ResultsListWidget.cpp
        WelcomeViewStep.cpp
        WelcomePage.cpp
    UI
        WelcomePage.ui
    RESOURCES
        welcome.qrc
    LINK_PRIVATE_LIBRARIES
        welcome_config
        ${PARTMAN_LIB}
        ${qtname}::DBus
        ${qtname}::Network
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(welcome
    SOURCES
        WelcomeTuiStep.cpp
    LINK_LIBRARIES
        welcome_config
)

if(WITH_GUI)
    claudeinstall_add_test(
        welcometest
        SOURCES checker/GeneralRequirements.cpp ${PARTMAN_SRC} Config.cpp Tests.cpp
        LIBRARIES ${PARTMAN_LIB} ${qtname}::DBus ${qtname}::Network ${qtname}::Widgets ClaudeInstall::claudeinstallui
    )
endif()
