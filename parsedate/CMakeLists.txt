if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    include(FetchContent)
    FetchContent_Declare(download_bison
        URL "http://user.xmission.com/~legalize/trn/bison-2.4.1.zip"
        URL_MD5 "6d728df45f1cf60f5569da4963322ec3"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    FetchContent_MakeAvailable(download_bison)
    set(BISON_ROOT "${download_bison_SOURCE_DIR}/bin")
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
set_target_properties(parsedate PROPERTIES FOLDER Libraries)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
