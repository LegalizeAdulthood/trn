cmake_minimum_required(VERSION 3.22)

set(TRN_PROJECT_NAME "trn" CACHE STRING "Project name for IDEs")

project("${TRN_PROJECT_NAME}" VERSION 5.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(GnuInstallDirs)

if(WIN32)
    find_package(pdcurses REQUIRED)
    add_library(curses ALIAS pdcurses)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_INSTALL_PREFIX "install")

set(trn_text_files
    HACKERSGUIDE.txt
    INIT.txt
    INSTALL.txt
    LICENSE.txt
    MANIFEST.txt
    README.txt
)
add_executable(trn main.cpp
    vcpkg.json
    ${trn_text_files}
    cmake/Findpdcurses.cmake
    cmake/FindBoostAsio.cmake
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("VcPkg Files" FILES vcpkg.json)
source_group("Text Files" FILES ${trn_text_files})
target_link_libraries(trn PUBLIC libtrn)
install(TARGETS trn DESTINATION ${CMAKE_INSTALL_BINDIR})

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
