# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

# libclaudeinstallui is the GUI part of ClaudeInstall, which includes handling
# view modules, view steps, widgets, and branding.

# The UI libs use the non-UI library
include_directories(
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstall
    ${CMAKE_BINARY_DIR}/src/libclaudeinstall
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstallstep
    ${CMAKE_SOURCE_DIR}
)

set(claudeinstallui_SOURCES
    modulesystem/ModuleManager.cpp
    modulesystem/ViewModule.cpp
    utils/Gui.cpp
    utils/ImageRegistry.cpp
    utils/Paste.cpp
    viewpages/BlankViewStep.cpp
    viewpages/ExecutionViewStep.cpp
    viewpages/Slideshow.cpp
    viewpages/ViewStep.cpp
    widgets/ClickableLabel.cpp
    widgets/ErrorDialog.cpp
    widgets/FixedAspectRatioLabel.cpp
    widgets/PrettyRadioButton.cpp
    widgets/LogWidget.cpp
    widgets/TranslationFix.cpp
    widgets/WaitingWidget.cpp
    widgets/waitingspinnerwidget.cpp
    Branding.cpp
    ViewManager.cpp
)

if(WITH_QML)
    list(APPEND claudeinstallui_SOURCES utils/Qml.cpp viewpages/QmlViewStep.cpp)
endif()

claudeinstall_add_library(claudeinstallui
    SOURCES ${claudeinstallui_SOURCES}
    TARGET_TYPE SHARED
    EXPORT_MACRO UIDLLEXPORT_PRO
    LINK_LIBRARIES
        ${qtname}::Svg
    RESOURCES libclaudeinstallui.qrc
    EXPORT ClaudeInstall
    UI
        utils/ErrorDialog/ErrorDialog.ui
    VERSION ${CLAUDEINSTALL_VERSION_SHORT}
    SOVERSION ${CLAUDEINSTALL_SOVERSION}
)
# The frontend-neutral wizard (steps, navigation); ViewStep and ViewManager build on it
target_link_libraries(claudeinstallui PUBLIC claudeinstallstep)
target_link_libraries(claudeinstallui PRIVATE yamlcpp::yamlcpp)
target_link_libraries(claudeinstallui PRIVATE ${kfname}::CoreAddons)

if(WITH_QML)
    target_link_libraries(claudeinstallui PUBLIC ${qtname}::QuickWidgets)
endif()

set_target_properties(
    claudeinstallui
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
)

add_library(ClaudeInstall::claudeinstallui ALIAS claudeinstallui)

### Installation
#
#
# The library is already installed through claudeinstall_add_library(),
# so we only need to do headers. Unlike the ClaudeInstall source tree,
# where libclaudeinstall and libclaudeinstallui live in different branches,
# we're going to glom it all together in the installed headers location.

install(FILES Branding.h ViewManager.h DESTINATION include/libclaudeinstall)

# Install each subdir-worth of header files
foreach(subdir modulesystem utils viewpages widgets)
    file(GLOB subdir_headers "${subdir}/*.h")
    install(FILES ${subdir_headers} DESTINATION include/libclaudeinstall/${subdir})
endforeach()

claudeinstall_add_test(test_libclaudeinstalluipaste SOURCES utils/TestPaste.cpp utils/Paste.cpp LIBRARIES claudeinstallui)
