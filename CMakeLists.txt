cmake_minimum_required(VERSION 3.20)
project(hdmi_wave_demo VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Display path, chosen at build time.
#   wayland: windowed/fullscreen client (Weston on the Pi, any desktop session on a PC)
#   drm:     raylib PLATFORM_DRM, straight to KMS through GBM/EGL, no compositor
set(HWD_PLATFORM "wayland" CACHE STRING "Display path: wayland or drm")
set_property(CACHE HWD_PLATFORM PROPERTY STRINGS wayland drm)
# Windowing library behind the wayland path.
#   GLFW: raylib PLATFORM_DESKTOP, bundled GLFW with Wayland (+X11 when
#         HWD_GLFW_X11=ON). Needs wayland-client/-scanner/-protocols and
#         xkbcommon headers. This is what the Yocto recipe builds.
#   SDL:  raylib PLATFORM_DESKTOP_SDL. SDL2 picks Wayland or X11 at run time,
#         so one binary runs on either session. Host builds use it when the
#         Wayland/X11 development headers GLFW needs are not installed.
set(HWD_DESKTOP_BACKEND "GLFW" CACHE STRING "wayland path backend: GLFW or SDL")
set_property(CACHE HWD_DESKTOP_BACKEND PROPERTY STRINGS GLFW SDL)
option(HWD_GLFW_X11 "GLFW backend: also build the X11 backend" ON)
# GL flavour for the wayland path. OFF = desktop GL 3.3 (PC). ON = GLES 3.0 (Pi 5, Mesa v3d).
# The drm path is always GLES 2.0 (raylib PLATFORM_DRM).
option(HWD_GLES "wayland path: use OpenGL ES 3.0 instead of desktop GL 3.3" OFF)

option(HWD_FETCH_RAYLIB "Fetch raylib 5.5 if find_package(raylib) fails" ON)
set(HWD_RAYLIB_URL "https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz"
    CACHE STRING "raylib 5.5 source tarball (URL or local path)")
set(HWD_RAYLIB_SHA256 "aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e"
    CACHE STRING "SHA256 of the raylib 5.5 tarball")

if(NOT HWD_PLATFORM MATCHES "^(wayland|drm)$")
    message(FATAL_ERROR "HWD_PLATFORM must be wayland or drm, got '${HWD_PLATFORM}'")
endif()
if(NOT HWD_DESKTOP_BACKEND MATCHES "^(GLFW|SDL)$")
    message(FATAL_ERROR "HWD_DESKTOP_BACKEND must be GLFW or SDL, got '${HWD_DESKTOP_BACKEND}'")
endif()

# A distro may provide raylib; FETCHCONTENT_SOURCE_DIR_RAYLIB points the
# fetch at an already unpacked tree (the Yocto recipe does this: no network
# after do_fetch). The bundled build is preferred when FETCHCONTENT_SOURCE_DIR_RAYLIB
# is set, because only it gives us the miniaudio capture API.
if(NOT FETCHCONTENT_SOURCE_DIR_RAYLIB)
    find_package(raylib 5.5 QUIET)
endif()

set(HWD_HAVE_MINIAUDIO OFF)
if(raylib_FOUND)
    message(STATUS "hdmi-wave-demo: using system raylib (no microphone capture)")
elseif(HWD_FETCH_RAYLIB OR FETCHCONTENT_SOURCE_DIR_RAYLIB)
    include(FetchContent)
    if(HWD_PLATFORM STREQUAL "drm")
        set(PLATFORM "DRM" CACHE STRING "" FORCE)
        set(OPENGL_VERSION "ES 2.0" CACHE STRING "" FORCE)
    else()
        if(HWD_DESKTOP_BACKEND STREQUAL "SDL")
            set(PLATFORM "SDL" CACHE STRING "" FORCE)
        else()
            set(PLATFORM "Desktop" CACHE STRING "" FORCE)
            set(USE_EXTERNAL_GLFW "OFF" CACHE STRING "" FORCE)
            set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
            set(GLFW_BUILD_X11 ${HWD_GLFW_X11} CACHE BOOL "" FORCE)
        endif()
        if(HWD_GLES)
            set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
        else()
            set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
        endif()
    endif()
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(USE_AUDIO ON CACHE BOOL "" FORCE)
    # Everything is procedural: no models, no file loaders except PNG (the
    # headless screenshots) and no audio file decoders (audio is synthesised).
    foreach(opt
            SUPPORT_MODULE_RMODELS SUPPORT_CAMERA_SYSTEM SUPPORT_GESTURES_SYSTEM
            SUPPORT_MOUSE_GESTURES SUPPORT_SCREEN_CAPTURE SUPPORT_GIF_RECORDING
            SUPPORT_COMPRESSION_API SUPPORT_AUTOMATION_EVENTS SUPPORT_CLIPBOARD_IMAGE
            SUPPORT_BUSY_WAIT_LOOP SUPPORT_PARTIALBUSY_WAIT_LOOP SUPPORT_IMAGE_GENERATION
            SUPPORT_FILEFORMAT_BMP SUPPORT_FILEFORMAT_TGA SUPPORT_FILEFORMAT_JPG
            SUPPORT_FILEFORMAT_GIF SUPPORT_FILEFORMAT_QOI SUPPORT_FILEFORMAT_PSD
            SUPPORT_FILEFORMAT_DDS SUPPORT_FILEFORMAT_HDR SUPPORT_FILEFORMAT_PIC
            SUPPORT_FILEFORMAT_KTX SUPPORT_FILEFORMAT_ASTC SUPPORT_FILEFORMAT_PKM
            SUPPORT_FILEFORMAT_PVR SUPPORT_FILEFORMAT_TTF SUPPORT_FILEFORMAT_FNT
            SUPPORT_FILEFORMAT_BDF SUPPORT_FILEFORMAT_OBJ SUPPORT_FILEFORMAT_MTL
            SUPPORT_FILEFORMAT_IQM SUPPORT_FILEFORMAT_GLTF SUPPORT_FILEFORMAT_VOX
            SUPPORT_FILEFORMAT_M3D SUPPORT_MESH_GENERATION
            SUPPORT_FILEFORMAT_WAV SUPPORT_FILEFORMAT_OGG SUPPORT_FILEFORMAT_MP3
            SUPPORT_FILEFORMAT_QOA SUPPORT_FILEFORMAT_FLAC SUPPORT_FILEFORMAT_XM
            SUPPORT_FILEFORMAT_MOD)
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    foreach(opt SUPPORT_MODULE_RAUDIO SUPPORT_FILEFORMAT_PNG SUPPORT_IMAGE_EXPORT SUPPORT_DEFAULT_FONT)
        set(${opt} ON CACHE BOOL "" FORCE)
    endforeach()

    # EXCLUDE_FROM_ALL also drops raylib's install() rules: raylib is linked
    # statically, so its library, headers, pkg-config and CMake files must not
    # be installed (they carry build paths; Yocto QA rejects them).
    FetchContent_Declare(raylib
        URL "${HWD_RAYLIB_URL}"
        URL_HASH SHA256=${HWD_RAYLIB_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(raylib)
    target_compile_options(raylib PRIVATE -ffunction-sections -fdata-sections -w)
    if(HWD_PLATFORM STREQUAL "drm")
        # raylib adds /usr/include/libdrm only when not cross-compiling with a
        # sysroot; take the path from pkg-config so a Yocto sysroot works too.
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBDRM REQUIRED libdrm)
        target_include_directories(raylib PRIVATE ${LIBDRM_INCLUDE_DIRS})
    endif()
    # raudio.c compiles miniaudio with external linkage; the capture code uses
    # its device API through the same header.
    set(HWD_MINIAUDIO_INCLUDE "${raylib_SOURCE_DIR}/src/external")
    set(HWD_HAVE_MINIAUDIO ON)
else()
    message(FATAL_ERROR "raylib 5.5 not found and HWD_FETCH_RAYLIB=OFF")
endif()

set(HWD_SCENES
    src/scene_spring.c
    src/scene_square.c
    src/scene_epicycles.c
    src/scene_standing.c
    src/scene_spectrogram.c
    src/scene_pendulum.c)

add_executable(hdmi-wave-demo
    src/main.c src/scenes.c src/app.c src/audio.c src/mic.c src/thermal.c
    src/fft.c src/svgpath.c src/draw.c
    ${HWD_SCENES})
target_compile_options(hdmi-wave-demo PRIVATE -Wall -Wextra -Wshadow -Wno-missing-field-initializers)
target_link_options(hdmi-wave-demo PRIVATE -Wl,--gc-sections)
target_compile_definitions(hdmi-wave-demo PRIVATE
    HWD_VERSION="${PROJECT_VERSION}" HWD_PLATFORM_NAME="${HWD_PLATFORM}")
# Scene 3 reads its default shapes from the installed asset directory, or
# from the source tree when the binary runs from this build tree. Distro
# builds turn the latter off so no build-host path ends up in the binary.
option(HWD_DEV_ASSETS "Let a build-tree binary read assets/ from the source tree" ON)
include(GNUInstallDirs)
target_compile_definitions(hdmi-wave-demo PRIVATE
    HWD_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/hdmi-wave-demo/assets")
if(HWD_DEV_ASSETS)
    target_compile_definitions(hdmi-wave-demo PRIVATE
        HWD_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        HWD_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(HWD_PLATFORM STREQUAL "drm")
    target_compile_definitions(hdmi-wave-demo PRIVATE HWD_DRM=1)
endif()
if(HWD_HAVE_MINIAUDIO)
    target_compile_definitions(hdmi-wave-demo PRIVATE HWD_HAVE_MINIAUDIO=1)
    target_include_directories(hdmi-wave-demo PRIVATE ${HWD_MINIAUDIO_INCLUDE})
endif()
target_link_libraries(hdmi-wave-demo PRIVATE raylib m pthread dl)
# rlgl calls GLES 3 entry points directly (no loader) when built for ES 3.0,
# and raylib's desktop platforms do not link libGLESv2 themselves.
if(HWD_GLES AND HWD_PLATFORM STREQUAL "wayland")
    find_library(GLESV2_LIB GLESv2 REQUIRED)
    target_link_libraries(hdmi-wave-demo PRIVATE ${GLESV2_LIB})
endif()
if(raylib_FOUND AND HWD_DESKTOP_BACKEND STREQUAL "SDL" AND HWD_PLATFORM STREQUAL "wayland")
    find_package(SDL2 REQUIRED)
    target_link_libraries(hdmi-wave-demo PRIVATE SDL2::SDL2)
endif()

enable_testing()
add_executable(test_fft tests/test_fft.c src/fft.c)
target_compile_options(test_fft PRIVATE -Wall -Wextra)
target_link_libraries(test_fft PRIVATE m)
add_test(NAME fft COMMAND test_fft)

add_executable(test_svgpath tests/test_svgpath.c src/svgpath.c)
target_compile_options(test_svgpath PRIVATE -Wall -Wextra)
target_link_libraries(test_svgpath PRIVATE m)
add_test(NAME svgpath COMMAND test_svgpath
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo-mark.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/signature.svg)

include(GNUInstallDirs)
install(TARGETS hdmi-wave-demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/hdmi-wave-demo/assets FILES_MATCHING PATTERN "*.svg")
