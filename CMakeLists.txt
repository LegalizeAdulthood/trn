cmake_minimum_required(VERSION 3.22)

set(TRN_PROJECT_NAME "trn" CACHE STRING "Project name for IDEs")

project("${TRN_PROJECT_NAME}" VERSION 5.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(set_cxx_standard)
include(target_folder)
include(vs_startup_project)
include(CTest)
include(GNUInstallDirs)

set_cxx_standard(17)

if(WIN32)
    find_package(pdcurses REQUIRED)
    add_library(curses ALIAS pdcurses)
endif()

set(CMAKE_INSTALL_PREFIX "install")

set(trn_text_files
    HACKERSGUIDE.txt
    INIT.txt
    INSTALL.txt
    LICENSE.txt
    README.txt
)
add_executable(trn main.cpp
    ReadMe.md
    vcpkg.json
    ${trn_text_files}
    cmake/Findpdcurses.cmake
    cmake/FindBoostAsio.cmake
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("VcPkg Files" FILES vcpkg.json)
source_group("Text Files" FILES ${trn_text_files})
source_group("Markdown Files" FILES ReadMe.md)
target_link_libraries(trn PUBLIC libtrn)
install(TARGETS trn DESTINATION ${CMAKE_INSTALL_BINDIR})
vs_startup_project(trn)

add_subdirectory(config)
add_subdirectory(inews)
add_subdirectory(libtrn)
add_subdirectory(nntp)
add_subdirectory(nntplist)
add_subdirectory(parsedate)
add_subdirectory(tool)
add_subdirectory(trn-artchk)
add_subdirectory(util)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
