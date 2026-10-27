# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
###
#
# This CMakeList handles the following i18n / language targets:
#
# - creating a translation test-tool
# - compiling all the Qt translations into a C++ file claudeinstall-i18n.cxx
# - defines an OBJECT LIBRARY claudeinstall-i18n for linking the compiled
#   translations into an executable.

include(ClaudeInstallAddTranslations)

find_package(${qtname} COMPONENTS Xml)
if(TARGET ${qtname}::Xml)
    add_executable(txload txload.cpp)
    target_link_libraries(txload ${qtname}::Xml)
    # Special-case, needs compatibility-header for XML-handling,
    # but doesn't want all of libclaudeinstall.
    target_include_directories(txload PRIVATE ${CMAKE_SOURCE_DIR}/src/libclaudeinstall)
endif()

# The gettext-based translations for the (former) Python modules are no
# longer built or installed: the modules are C++ now, and their strings
# are Qt translations. The old catalogs remain in lang/python/ as a source
# for translations to move into the Qt .ts files.

### TRANSLATIONS
#
#
claudeinstall_qrc_translations(claudeinstall-i18n
    OUTPUT_VARIABLE translation_outfile
    PREFIXES claudeinstall tz kb
)

add_library(claudeinstall-i18n OBJECT ${translation_outfile})
set_property(TARGET claudeinstall-i18n PROPERTY POSITION_INDEPENDENT_CODE ON)
