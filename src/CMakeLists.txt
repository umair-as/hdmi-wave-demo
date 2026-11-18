# Pure maths, no raylib: FFT and the SVG loader. The unit tests link it too.
add_library(hwd_maths STATIC
    maths/fft.c
    maths/svgpath.c)
target_include_directories(hwd_maths PUBLIC maths)
target_link_libraries(hwd_maths PUBLIC m PRIVATE hwd_warnings)

find_package(Threads REQUIRED)

add_executable(hdmi-wave-demo
    main.c
    core/app.c
    core/draw.c
    devices/audio.c
    devices/mic.c
    devices/thermal.c
    scenes/scenes.c
    scenes/spring.c
    scenes/square.c
    scenes/epicycles.c
    scenes/standing.c
    scenes/spectrogram.c
    scenes/pendulum.c
    scenes/lissajous.c
    scenes/interference.c
    scenes/lorenz.c)
target_include_directories(hdmi-wave-demo PRIVATE core devices scenes)
target_link_libraries(hdmi-wave-demo PRIVATE
    hwd_maths hwd::raylib Threads::Threads ${CMAKE_DL_LIBS} hwd_warnings)
target_link_options(hdmi-wave-demo PRIVATE -Wl,--gc-sections)
# build/hdmi-wave-demo, not build/src/hdmi-wave-demo
set_target_properties(hdmi-wave-demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

target_compile_definitions(hdmi-wave-demo PRIVATE
    HWD_VERSION="${PROJECT_VERSION}"
    HWD_PLATFORM_NAME="${HWD_PLATFORM}"
    HWD_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/hdmi-wave-demo/assets"
    $<$<STREQUAL:${HWD_PLATFORM},drm>:HWD_DRM=1>)
if(HWD_DEV_ASSETS)
    target_compile_definitions(hdmi-wave-demo PRIVATE
        HWD_SOURCE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
        HWD_BUILD_DIR="${PROJECT_BINARY_DIR}")
endif()

install(TARGETS hdmi-wave-demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
