configure_file(access_def.in access_def @ONLY)

add_library(data INTERFACE
    access_def.in
    "${CMAKE_CURRENT_BINARY_DIR}/access_def"
)
source_group("CMake Templates" FILES access_def.in)
source_group("Data Files" FILES "${CMAKE_CURRENT_BINARY_DIR}/access_def")
target_folder(data "Libraries")
