# raylib 5.5, exposed to the rest of the build as one target, hwd::raylib,
# which also carries what the chosen platform needs on top of raylib.
#
# A distro raylib is used when find_package finds one, but then there is no
# microphone capture: that uses the miniaudio copy inside raylib's sources.
# Otherwise raylib is fetched (or taken from FETCHCONTENT_SOURCE_DIR_RAYLIB,
# as the Yocto recipe does: no network after do_fetch) and linked statically.

add_library(hwd_raylib INTERFACE)
add_library(hwd::raylib ALIAS hwd_raylib)

if(NOT FETCHCONTENT_SOURCE_DIR_RAYLIB)
    find_package(raylib 5.5 QUIET)
endif()

if(raylib_FOUND)
    message(STATUS "hdmi-wave-demo: using system raylib (no microphone capture)")
    if(HWD_PLATFORM STREQUAL "wayland" AND HWD_DESKTOP_BACKEND STREQUAL "SDL")
        find_package(SDL2 REQUIRED)
        target_link_libraries(hwd_raylib INTERFACE SDL2::SDL2)
    endif()
elseif(HWD_FETCH_RAYLIB OR FETCHCONTENT_SOURCE_DIR_RAYLIB)
    # raylib's own build options, set before it is configured.
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

    # Everything is procedural: no models and no file loaders except PNG
    # (headless screenshots); no audio decoders (audio is synthesised).
    foreach(opt
            SUPPORT_MODULE_RMODELS SUPPORT_CAMERA_SYSTEM SUPPORT_GESTURES_SYSTEM
            SUPPORT_MOUSE_GESTURES SUPPORT_SCREEN_CAPTURE SUPPORT_GIF_RECORDING
            SUPPORT_COMPRESSION_API SUPPORT_AUTOMATION_EVENTS SUPPORT_CLIPBOARD_IMAGE
            SUPPORT_BUSY_WAIT_LOOP SUPPORT_PARTIALBUSY_WAIT_LOOP SUPPORT_IMAGE_GENERATION
            SUPPORT_MESH_GENERATION
            SUPPORT_FILEFORMAT_BMP SUPPORT_FILEFORMAT_TGA SUPPORT_FILEFORMAT_JPG
            SUPPORT_FILEFORMAT_GIF SUPPORT_FILEFORMAT_QOI SUPPORT_FILEFORMAT_PSD
            SUPPORT_FILEFORMAT_DDS SUPPORT_FILEFORMAT_HDR SUPPORT_FILEFORMAT_PIC
            SUPPORT_FILEFORMAT_KTX SUPPORT_FILEFORMAT_ASTC SUPPORT_FILEFORMAT_PKM
            SUPPORT_FILEFORMAT_PVR SUPPORT_FILEFORMAT_TTF SUPPORT_FILEFORMAT_FNT
            SUPPORT_FILEFORMAT_BDF SUPPORT_FILEFORMAT_OBJ SUPPORT_FILEFORMAT_MTL
            SUPPORT_FILEFORMAT_IQM SUPPORT_FILEFORMAT_GLTF SUPPORT_FILEFORMAT_VOX
            SUPPORT_FILEFORMAT_M3D
            SUPPORT_FILEFORMAT_WAV SUPPORT_FILEFORMAT_OGG SUPPORT_FILEFORMAT_MP3
            SUPPORT_FILEFORMAT_QOA SUPPORT_FILEFORMAT_FLAC SUPPORT_FILEFORMAT_XM
            SUPPORT_FILEFORMAT_MOD)
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    foreach(opt SUPPORT_MODULE_RAUDIO SUPPORT_FILEFORMAT_PNG SUPPORT_IMAGE_EXPORT SUPPORT_DEFAULT_FONT)
        set(${opt} ON CACHE BOOL "" FORCE)
    endforeach()

    # EXCLUDE_FROM_ALL also drops raylib's install() rules: it is linked
    # statically, and its headers, pkg-config and CMake files carry build
    # paths (Yocto QA buildpaths).
    include(FetchContent)
    FetchContent_Declare(raylib
        URL "${HWD_RAYLIB_URL}"
        URL_HASH SHA256=${HWD_RAYLIB_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(raylib)
    target_compile_options(raylib PRIVATE -ffunction-sections -fdata-sections -w)

    if(HWD_PLATFORM STREQUAL "drm")
        # raylib adds /usr/include/libdrm only when not cross-compiling with
        # a sysroot; take the path from pkg-config so a Yocto sysroot works.
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBDRM REQUIRED libdrm)
        target_include_directories(raylib PRIVATE ${LIBDRM_INCLUDE_DIRS})
    endif()

    # raudio.c compiles miniaudio with external linkage; the microphone code
    # uses its capture API through the same header.
    target_include_directories(hwd_raylib INTERFACE "${raylib_SOURCE_DIR}/src/external")
    target_compile_definitions(hwd_raylib INTERFACE HWD_HAVE_MINIAUDIO=1)
else()
    message(FATAL_ERROR "raylib 5.5 not found and HWD_FETCH_RAYLIB=OFF")
endif()

target_link_libraries(hwd_raylib INTERFACE raylib)

# rlgl calls GLES 3 entry points directly (no loader) when built for ES 3.0,
# and raylib's desktop platforms do not link libGLESv2 themselves.
if(HWD_PLATFORM STREQUAL "wayland" AND HWD_GLES)
    find_library(GLESV2_LIB GLESv2 REQUIRED)
    target_link_libraries(hwd_raylib INTERFACE ${GLESV2_LIB})
endif()
