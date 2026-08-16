cmake_minimum_required(VERSION 3.22)

set(TRN_PROJECT_NAME "trn" CACHE STRING "Project name for IDEs")

project("${TRN_PROJECT_NAME}" VERSION 5.0.0 LANGUAGES C CXX)

find_package(Git QUIET)
set(TRN_HEAD_HASH "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE TRN_GIT_RESULT
        OUTPUT_VARIABLE TRN_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(TRN_GIT_RESULT EQUAL 0 AND NOT TRN_GIT_HASH STREQUAL "")
        set(TRN_HEAD_HASH "${TRN_GIT_HASH}")
    endif()
endif()
set(TRN_PATCHLEVEL "${PROJECT_VERSION} (${TRN_HEAD_HASH})")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(misc_target)
include(set_cxx_standard)
include(target_folder)
include(vs_startup_project)
include(CTest)
include(GNUInstallDirs)

set_cxx_standard(17)

find_package(fmt CONFIG REQUIRED)

if(WIN32)
    find_package(pdcurses REQUIRED)
    add_library(curses ALIAS pdcurses)
else()
    find_package(Curses REQUIRED)
    add_library(curses INTERFACE)
    if(CURSES_INCLUDE_DIRS)
        target_include_directories(curses INTERFACE ${CURSES_INCLUDE_DIRS})
    endif()
    if(CURSES_CFLAGS)
        target_compile_options(curses INTERFACE ${CURSES_CFLAGS})
    endif()
    if(CURSES_LIBRARIES)
        target_link_libraries(curses INTERFACE ${CURSES_LIBRARIES})
    endif()
endif()

set(CMAKE_INSTALL_PREFIX "install")

set(trn_text_files
    HACKERSGUIDE.txt
    INIT.txt
    LICENSE.txt
)
add_executable(trn main.cpp
    "docs/AsyncPlan.md"
    "INSTALL.md"
    "ReadMe.md"
    "docs/StructureFunctions.md"
    vcpkg.json
    ${trn_text_files}
    cmake/Findpdcurses.cmake
    cmake/FindBoostAsio.cmake
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("VcPkg Files" FILES vcpkg.json)
source_group("Text Files" FILES ${trn_text_files})
source_group("Markdown Files" FILES
    "docs/AsyncPlan.md"
    "INSTALL.md"
    "ReadMe.md"
    "docs/StructureFunctions.md"
)
target_link_libraries(trn PUBLIC libtrn)
install(TARGETS trn DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
    man/trn.1
    man/Pnews.1
    man/Rnmail.1
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
install(FILES
    ReadMe.md
    INSTALL.md
    LICENSE.txt
    HACKERSGUIDE.txt
    DESTINATION ".")
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
add_subdirectory(wildmat)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

misc_target(FILES
    "CMakePresets.json"
    "HACKERSGUIDE.txt"
    "INIT.txt"
    "INSTALL.md"
    "ReadMe.md"
)
misc_group_sources("CMake Scripts" FILES
    "cmake/FindBoostAsio.cmake"
    "cmake/Findpdcurses.cmake"
    "cmake/misc_target.cmake"
    "cmake/set_cxx_standard.cmake"
    "cmake/target_folder.cmake"
    "cmake/vs_startup_project.cmake")
misc_group_sources("Editing" FILES
    ".clang-format"
    ".editorconfig")
misc_group_sources("Git" FILES
    ".gitignore"
    ".gitmodules")
misc_group_sources("GitHub" FILES
    ".github/workflows/cmake.yml"
    "LICENSE.txt")
misc_group_sources("VcPkg" FILES
    "vcpkg.json")

set(CPACK_PACKAGE_NAME "trn")
set(CPACK_PACKAGE_VENDOR "Richard Thomson")
set(CPACK_PACKAGE_CONTACT "https://github.com/LegalizeAdulthood/trn")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Threaded newsreader")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "trn ${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_VERBATIM_VARIABLES YES)
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    find_program(TRN_NSIS_MAKENSIS makensis NO_CACHE)
    if(TRN_NSIS_MAKENSIS)
        list(APPEND CPACK_GENERATOR NSIS)
    endif()
    set(CPACK_NSIS_DISPLAY_NAME "trn ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "trn ${PROJECT_VERSION}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
