# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

#
# libclaudeinstall is the non-GUI part of ClaudeInstall, which includes handling
# translations, configurations, logging, utilities, global storage, and
# (non-GUI) jobs.
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ClaudeInstallConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallConfig.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ClaudeInstallVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallVersion.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ClaudeInstallVersionX.h.in ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallVersionX.h)

# Map the available translations names into a suitable constexpr list
# of names in C++. This gets us ClaudeInstall::Locale::availableLanguages,
# a QStringList of names.
set(_names_tu
    "
#ifndef CLAUDEINSTALL_TRANSLATIONS_H
#define CLAUDEINSTALL_TRANSLATIONS_H
#include <QStringList>
namespace {
static const QStringList availableLanguageList{
"
)
foreach(l ${CLAUDEINSTALL_TRANSLATION_LANGUAGES})
    string(APPEND _names_tu "\"${l}\",\n")
endforeach()
string(APPEND _names_tu "};\n} // namespace\n#endif\n\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallTranslations.cc "${_names_tu}")

# Some implementation details are compiled twice, because
# we want them in the library with visibility("hidden")
# but also need them in tests.
set(_geoip_src
    geoip/GeoIPFixed.cpp
    geoip/GeoIPJSON.cpp
)

add_library(
    claudeinstall
    SHARED
    ClaudeInstallAbout.cpp
    CppJob.cpp
    GlobalStorage.cpp
    Job.cpp
    JobExample.cpp
    JobQueue.cpp
    ProcessJob.cpp
    Settings.cpp
    # GeoIP services
    geoip/Interface.cpp
    ${_geoip_src}
    geoip/Handler.cpp
    # Locale-data service
    locale/Global.cpp
    locale/Lookup.cpp
    locale/TimeZone.cpp
    locale/TranslatableConfiguration.cpp
    locale/TranslatableString.cpp
    locale/Translation.cpp
    locale/TranslationsModel.cpp
    # Modules
    modulesystem/Config.cpp
    modulesystem/Descriptor.cpp
    modulesystem/InstanceKey.cpp
    modulesystem/Module.cpp
    modulesystem/Preset.cpp
    modulesystem/RequirementsChecker.cpp
    modulesystem/RequirementsModel.cpp
    # Network service
    network/Manager.cpp
    # Packages service
    packages/Globals.cpp
    # Partition service
    partition/Global.cpp
    partition/Mount.cpp
    partition/PartitionSize.cpp
    partition/Sync.cpp
    # Utility service
    utils/CommandList.cpp
    utils/Dirs.cpp
    utils/Entropy.cpp
    utils/Logger.cpp
    utils/Permissions.cpp
    utils/PluginFactory.cpp
    utils/Retranslator.cpp
    utils/Runner.cpp
    utils/String.cpp
    utils/StringExpander.cpp
    utils/System.cpp
    utils/UMask.cpp
    utils/Variant.cpp
    utils/Yaml.cpp
)

set_target_properties(
    claudeinstall
    PROPERTIES
        VERSION ${CLAUDEINSTALL_VERSION_SHORT}
        SOVERSION ${CLAUDEINSTALL_SOVERSION}
        CXX_VISIBILITY_PRESET hidden
)
target_link_libraries(claudeinstall LINK_PUBLIC yamlcpp::yamlcpp ${qtname}::Core ${qtname}::Network)
target_link_libraries(claudeinstall LINK_PUBLIC ${kfname}::CoreAddons)

target_compile_definitions(claudeinstall PRIVATE DLLEXPORT_PRO)
target_include_directories(claudeinstall PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(claudeinstall PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/libclaudeinstall>
)
target_include_directories(claudeinstall PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

### OPTIONAL Automount support (requires dbus)
#
#
if(TARGET ${qtname}::DBus)
    target_sources(claudeinstall PRIVATE partition/AutoMount.cpp)
    target_link_libraries(claudeinstall PRIVATE ${qtname}::DBus)
endif()

### OPTIONAL GeoIP XML support
#
#
find_package(${qtname} ${QT_VERSION} COMPONENTS Xml)
if(TARGET ${qtname}::Xml)
    target_sources(claudeinstall PRIVATE geoip/GeoIPXML.cpp)
    target_link_libraries(claudeinstall PRIVATE ${qtname}::Network ${qtname}::Xml)
endif()

### OPTIONAL KPMcore support
#
#
include(KPMcoreHelper)

if(KPMcore_FOUND)
    target_sources(
        claudeinstall
        PRIVATE
            partition/FileSystem.cpp
            partition/KPMManager.cpp
            partition/PartitionIterator.cpp
            partition/PartitionQuery.cpp
    )
endif()
# Always, since this also handles the no-KPMcore case; we don't
# call it claudeinstall::kpmcore because that name exists only
# when KPMcore is actually found.
target_link_libraries(claudeinstall PRIVATE calapmcore)

### LIBRARY
#
#
claudeinstall_automoc( claudeinstall )
add_library(ClaudeInstall::claudeinstall ALIAS claudeinstall)

### Installation
#
#
install(
    TARGETS claudeinstall
    EXPORT ClaudeInstall
    RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install header files
file(GLOB rootHeaders "*.h")
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallConfig.h ${CMAKE_CURRENT_BINARY_DIR}/ClaudeInstallVersion.h ${rootHeaders}
    DESTINATION include/libclaudeinstall
)
# Install each subdir-worth of header files
foreach(subdir geoip locale modulesystem network partition utils compat packages)
    file(GLOB subdir_headers "${subdir}/*.h")
    install(FILES ${subdir_headers} DESTINATION include/libclaudeinstall/${subdir})
endforeach()

### TRANSLATION TESTING
#

claudeinstall_qrc_translations( localetest OUTPUT_VARIABLE localetest_qrc SUBDIRECTORY testdata LANGUAGES nl )

### TESTING
#
#
claudeinstall_add_test(libclaudeinstalltest SOURCES Tests.cpp)

claudeinstall_add_test(libclaudeinstallgeoiptest SOURCES geoip/GeoIPTests.cpp ${_geoip_src})

claudeinstall_add_test(libclaudeinstalllocaletest SOURCES locale/Tests.cpp ${localetest_qrc})

claudeinstall_add_test(libclaudeinstallmodulesystemtest SOURCES modulesystem/Tests.cpp)

claudeinstall_add_test(libclaudeinstallnetworktest SOURCES network/Tests.cpp)

claudeinstall_add_test(libclaudeinstallpackagestest SOURCES packages/Tests.cpp)

if(KPMcore_FOUND)
    claudeinstall_add_test(
        libclaudeinstallpartitiontest
        SOURCES partition/Global.cpp partition/Tests.cpp
        LIBRARIES claudeinstall::kpmcore
    )
    claudeinstall_add_test(libclaudeinstallpartitionkpmtest SOURCES partition/KPMTests.cpp LIBRARIES claudeinstall::kpmcore)
endif()

claudeinstall_add_test(libclaudeinstallutilstest SOURCES utils/Tests.cpp utils/Permissions.cpp utils/Runner.cpp)

claudeinstall_add_test(libclaudeinstallutilspathstest SOURCES utils/TestPaths.cpp)

# This is not an actual test, it's a test / demo application
# for experimenting with GeoIP.
add_executable(test_geoip geoip/test_geoip.cpp ${_geoip_src})
target_link_libraries(test_geoip ClaudeInstall::claudeinstall ${qtname}::Network yamlcpp::yamlcpp)
claudeinstall_automoc( test_geoip )

if(TARGET ${qtname}::DBus)
    add_executable(test_automount partition/calautomount.cpp)
    target_link_libraries(test_automount ClaudeInstall::claudeinstall ${qtname}::DBus)
endif()
