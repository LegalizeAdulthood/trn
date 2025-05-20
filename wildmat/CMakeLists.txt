add_library(wildmat
    wildmat.cpp include/wildmat/wildmat.h
)
if(WIN32)
    target_compile_definitions(wildmat PUBLIC _CRT_DECLARE_NONSTDC_NAMES=1 _CRT_INTERNAL_NONSTDC_NAMES=1)
endif()
target_include_directories(wildmat PUBLIC include)
target_folder(wildmat "Libraries")
