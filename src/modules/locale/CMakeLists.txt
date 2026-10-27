# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

# When debugging the timezone widget, add this debugging definition
# to have a debugging-friendly timezone widget, debug logging,
# and no intrusive timezone-setting while clicking around.
option(DEBUG_TIMEZONES "Debug-friendly timezone widget." OFF)

include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

# The headless part: the Config, the locale data and the job, shared by
# all frontends. (A frontend that links it also needs locale.qrc.)
set(_locale_config_definitions)
if(DEBUG_TIMEZONES)
    list(APPEND _locale_config_definitions DEBUG_TIMEZONES)
endif()
claudeinstall_add_config_library(locale_config
    SOURCES
        Config.cpp
        LocaleConfiguration.cpp
        LocaleNames.cpp
        SetTimezoneJob.cpp
    LINK_LIBRARIES
        ${qtname}::Network
        yamlcpp::yamlcpp
    DEFINITIONS
        ${_locale_config_definitions}
)

claudeinstall_add_plugin(locale
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        LCLocaleDialog.cpp
        LocalePage.cpp
        LocaleViewStep.cpp
        timezonewidget/timezonewidget.cpp
        timezonewidget/TimeZoneImage.cpp
    UI
    RESOURCES
        locale.qrc
    LINK_PRIVATE_LIBRARIES
        locale_config
        ${qtname}::Network
        yamlcpp::yamlcpp
    SHARED_LIB
)
if(DEBUG_TIMEZONES AND locale_TARGET)  # without the GUI there is no plugin target
    target_compile_definitions(${locale_TARGET} PRIVATE DEBUG_TIMEZONES)
endif()

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(locale
    SOURCES
        LocaleTuiStep.cpp
    LINK_LIBRARIES
        locale_config
)

claudeinstall_add_test(
    localetest
    SOURCES
        Tests.cpp
        Config.cpp
        LocaleConfiguration.cpp
        LocaleNames.cpp
        SetTimezoneJob.cpp
        timezonewidget/TimeZoneImage.cpp
    DEFINITIONS SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/images" DEBUG_TIMEZONES=1
    LIBRARIES ${qtname}::Gui
)
