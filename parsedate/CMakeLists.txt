if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    include(FetchContent)
    FetchContent_Declare(download_bison
        URL "https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip"
        URL_MD5 "720226b1befe7033fb3ecc98f5ffd425"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    FetchContent_MakeAvailable(download_bison)
    set(BISON_ROOT "${download_bison_SOURCE_DIR}")
    message(STATUS "BISON_ROOT=${BISON_ROOT}")
endif()
find_package(BISON REQUIRED)

bison_target(parsedate parsedate.y ${CMAKE_CURRENT_BINARY_DIR}/parsedate.c)
add_library(parsedate
    ${BISON_parsedate_OUTPUTS}
    include/parsedate/parsedate.h
    parsedate.y
)
source_group("Yacc Files" REGULAR_EXPRESSION "^.*\\.y")
target_include_directories(parsedate PUBLIC include)
target_folder(parsedate "Libraries")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
