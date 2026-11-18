# Warning flags for this project's own targets (not raylib's).
add_library(hwd_warnings INTERFACE)
target_compile_options(hwd_warnings INTERFACE
    -Wall -Wextra -Wshadow -Wno-missing-field-initializers)
