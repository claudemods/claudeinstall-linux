# SPDX-FileCopyrightText: no
# SPDX-License-Identifier: CC0-1.0
#
# Stand-alone project: the ClaudeInstall setup helper (Qt6 Widgets).
# It is built by "./build.sh" (no command) and writes build.config for "./build.sh build".
cmake_minimum_required(VERSION 3.16)
project(claudeinstall-helper VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(claudeinstall-helper
    main.cpp
    MainWindow.cpp
    MainWindow.h
)
target_link_libraries(claudeinstall-helper PRIVATE Qt6::Widgets)
