add_library(util
    env.cpp env.h env-internal.h
    util2.cpp util2.h
    wildmat.cpp wildmat.h
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("Template Files" REGULAR_EXPRESSION "^.*\\.in$")
if(WIN32)
    target_compile_definitions(util PUBLIC _CRT_DECLARE_NONSTDC_NAMES=1 _CRT_INTERNAL_NONSTDC_NAMES=1)
endif()
target_include_directories(util PUBLIC .)
target_include_directories(util PRIVATE ../libtrn ../tool)
target_link_libraries(util PUBLIC config)
target_folder(util "Libraries")
