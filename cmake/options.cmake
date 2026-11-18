# Build options. See the README for how they combine.

# Display path:
#   wayland  a normal window or fullscreen client (Weston, any desktop session)
#   drm      raylib PLATFORM_DRM, straight to KMS through GBM/EGL, no compositor
set(HWD_PLATFORM "wayland" CACHE STRING "Display path: wayland or drm")
set_property(CACHE HWD_PLATFORM PROPERTY STRINGS wayland drm)

# Windowing library behind the wayland path:
#   GLFW  raylib PLATFORM_DESKTOP with its bundled GLFW (Wayland, + X11 if
#         HWD_GLFW_X11); needs the wayland and xkbcommon dev headers
#   SDL   raylib PLATFORM_DESKTOP_SDL; SDL2 picks Wayland or X11 at run time
set(HWD_DESKTOP_BACKEND "GLFW" CACHE STRING "wayland path backend: GLFW or SDL")
set_property(CACHE HWD_DESKTOP_BACKEND PROPERTY STRINGS GLFW SDL)
option(HWD_GLFW_X11 "GLFW backend: also build the X11 backend" ON)

# GL flavour of the wayland path; the drm path is always GLES 2.0.
option(HWD_GLES "wayland path: OpenGL ES 3.0 instead of desktop GL 3.3" OFF)

option(HWD_FETCH_RAYLIB "Fetch raylib 5.5 if find_package(raylib) fails" ON)
set(HWD_RAYLIB_URL "https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz"
    CACHE STRING "raylib 5.5 source tarball (URL or local path)")
set(HWD_RAYLIB_SHA256 "aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e"
    CACHE STRING "SHA256 of the raylib 5.5 tarball")

# Scene 3 reads its default shapes from the installed asset directory, or
# from the source tree when the binary runs from this build tree. Distro
# builds turn the latter off so no build-host path ends up in the binary.
option(HWD_DEV_ASSETS "Let a build-tree binary read assets/ from the source tree" ON)

if(NOT HWD_PLATFORM MATCHES "^(wayland|drm)$")
    message(FATAL_ERROR "HWD_PLATFORM must be wayland or drm, got '${HWD_PLATFORM}'")
endif()
if(NOT HWD_DESKTOP_BACKEND MATCHES "^(GLFW|SDL)$")
    message(FATAL_ERROR "HWD_DESKTOP_BACKEND must be GLFW or SDL, got '${HWD_DESKTOP_BACKEND}'")
endif()
