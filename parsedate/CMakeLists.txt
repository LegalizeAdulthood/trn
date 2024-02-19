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
