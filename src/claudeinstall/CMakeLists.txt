# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

# The graphical program; the text-mode one lives in ../claudeinstall-tui
if(WITH_GUI)
    set(claudeinstallSources
        main.cpp
        ClaudeInstallApplication.cpp
        ClaudeInstallWindow.cpp
        DebugWindow.cpp
        VariantModel.cpp
        progresstree/ProgressTreeDelegate.cpp
        progresstree/ProgressTreeView.cpp
    )

    include_directories(
        ${CMAKE_SOURCE_DIR}/src/libclaudeinstall
        ${CMAKE_SOURCE_DIR}/src/libclaudeinstallstep
        ${CMAKE_SOURCE_DIR}/src/libclaudeinstallui
        ${CMAKE_BINARY_DIR}/src/libclaudeinstall
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    ### EXECUTABLE
    #
    # "claudeinstall_bin" is the main application, not to be confused with
    # the target "claudeinstall" which is the non-GUI library part.
    #
    # The claudeinstall-i18n.cxx file -- full path in CLAUDEINSTALL_TRANSLATIONS_SOURCE --
    # is created as a target in the lang/ directory. This is compiled to a
    # library (it's just the result of a QRC compile).
    add_executable(claudeinstall_bin ${claudeinstallSources} claudeinstall.qrc)
    target_include_directories(claudeinstall_bin PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(claudeinstall_bin PROPERTIES ENABLE_EXPORTS TRUE RUNTIME_OUTPUT_NAME claudeinstall)
    claudeinstall_automoc( claudeinstall_bin )
    claudeinstall_autouic( claudeinstall_bin )
    claudeinstall_autorcc( claudeinstall_bin )

    target_link_libraries(
        claudeinstall_bin
        PRIVATE claudeinstall claudeinstallui claudeinstall-i18n kdsingleapplication ${qtname}::Core ${qtname}::Widgets
    )
    target_link_libraries(claudeinstall_bin PRIVATE ${kfname}::CoreAddons)
    if(BUILD_CRASH_REPORTING)
        target_link_libraries(claudeinstall_bin PRIVATE ${kfname}::Crash)
        target_compile_definitions(claudeinstall_bin PRIVATE BUILD_CRASH_REPORTING)
    endif()

    install(TARGETS claudeinstall_bin BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(
        FILES ${CMAKE_SOURCE_DIR}/data/images/squid.svg
        RENAME claudeinstall.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
endif()

### TESTS
#
#
if(BUILD_TESTING)
    # Don't install, these are just for enable_testing
    if(WITH_GUI)
        add_executable(loadmodule testmain.cpp)
        target_link_libraries(loadmodule PRIVATE ${qtname}::Core ${qtname}::Widgets claudeinstall claudeinstallui)
    endif()

    add_executable(test_conf test_conf.cpp)
    target_link_libraries(test_conf PUBLIC yamlcpp::yamlcpp ${qtname}::Core)
endif()
