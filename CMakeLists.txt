cmake_minimum_required(VERSION 3.22)

project(trn VERSION 5.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)

if(WIN32)
    find_package(pdcurses REQUIRED)
    add_library(curses ALIAS pdcurses)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE libtrn)
target_link_libraries(inews PUBLIC nntp tool)
set_property(TARGET inews PROPERTY FOLDER Tools)

add_executable(nntplist nntplist.cpp)
target_include_directories(nntplist PRIVATE libtrn)
target_link_libraries(nntplist PUBLIC nntp tool)
set_property(TARGET nntplist PROPERTY FOLDER Tools)

add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE libtrn)
target_link_libraries(trn-artchk PUBLIC nntp tool)
set_property(TARGET trn-artchk PROPERTY FOLDER Tools)

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

add_subdirectory(config)
add_subdirectory(libtrn)
add_subdirectory(nntp)
add_subdirectory(parsedate)
add_subdirectory(tool)
add_subdirectory(util)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
