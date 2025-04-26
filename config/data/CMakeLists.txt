configure_file(access_def.in access_def @ONLY)
configure_file(environment.in environment @ONLY)

add_library(data INTERFACE
    access_def.in
    environment.in
    "${CMAKE_CURRENT_BINARY_DIR}/access_def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
)
source_group("CMake Templates" FILES
    access_def.in
    environment.in)
source_group("Data Files" FILES
    "${CMAKE_CURRENT_BINARY_DIR}/access_def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
)
target_folder(data "Libraries")
