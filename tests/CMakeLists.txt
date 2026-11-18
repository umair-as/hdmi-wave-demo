add_executable(test_fft test_fft.c)
target_link_libraries(test_fft PRIVATE hwd_maths hwd_warnings)
add_test(NAME fft COMMAND test_fft)

add_executable(test_svgpath test_svgpath.c)
target_link_libraries(test_svgpath PRIVATE hwd_maths hwd_warnings)
add_test(NAME svgpath COMMAND test_svgpath
    ${PROJECT_SOURCE_DIR}/assets/logo-mark.svg
    ${PROJECT_SOURCE_DIR}/assets/signature.svg)
