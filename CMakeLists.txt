cmake_minimum_required(VERSION 3.20)
project(hdmi_wave_demo
    VERSION 0.2.0
    DESCRIPTION "Maths and physics attract-mode scenes for an HDMI screen"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(cmake/options.cmake)
include(cmake/warnings.cmake)
include(cmake/raylib.cmake)

add_subdirectory(src)

option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hdmi-wave-demo/assets
    FILES_MATCHING PATTERN "*.svg")
