add_library(calibration_core STATIC
    src/calibration_core.c
    src/calibration_self_test.c
)

target_include_directories(calibration_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(calibration_core PUBLIC
    m
)

target_compile_options(calibration_core PRIVATE
    -Wall
)
