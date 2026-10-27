# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#

# Developer and maintenance tools. They replace the Python scripts that used
# to live in ci/ and next to the data they produce.
#
#   configvalidator   validates config files against their JSON-schema (used by ctest)
#   txstats           queries Transifex and writes the language lists for CMakeLists.txt
#   txreduce          drops untranslated strings from a translation (.ts) file
#   cldr-extractor    writes CountryData_p.cpp from the CLDR likelySubtags.xml
#   zone-extractor    writes ZoneData_p.cpp from zone.tab
#   layout-extractor  writes KeyboardData_p.cxxtr from base.lst
#
# Only configvalidator is built by default (and only when it is needed, for the tests);
# build the rest with the "tools" target:  cmake --build <dir> --target tools

if(BUILD_TESTING AND BUILD_SCHEMA_TESTING)
    add_executable(configvalidator configvalidator.cpp)
    target_link_libraries(configvalidator PRIVATE yamlcpp::yamlcpp)
endif()

add_executable(txstats EXCLUDE_FROM_ALL txstats.cpp)
target_link_libraries(txstats PRIVATE ${qtname}::Core ${qtname}::Network)

add_executable(txreduce EXCLUDE_FROM_ALL txreduce.cpp)
target_link_libraries(txreduce PRIVATE ${qtname}::Core)

add_executable(cldr-extractor EXCLUDE_FROM_ALL cldr-extractor.cpp)
add_executable(zone-extractor EXCLUDE_FROM_ALL zone-extractor.cpp)
add_executable(layout-extractor EXCLUDE_FROM_ALL layout-extractor.cpp)

add_custom_target(tools DEPENDS txstats txreduce cldr-extractor zone-extractor layout-extractor)
if(TARGET configvalidator)
    add_dependencies(tools configvalidator)
endif()
