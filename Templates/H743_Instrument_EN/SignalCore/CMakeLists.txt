add_library(signal_core STATIC
    src/signal_core_port.c
    src/app_trig_lookup.c
    src/signal_preprocess.c
    src/signal_fft.c
    src/signal_dual_sine.c
    src/signal_harmonic.c
    src/signal_joint_model.c
    src/signal_vote.c
    src/app_phase_lock.c
)

target_include_directories(signal_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/APP/Config
)

target_link_libraries(signal_core PUBLIC
    cmsis_dsp_fft
    m
)

target_compile_options(signal_core PRIVATE
    -Wall
)

# Keep numerical hot paths optimized in Debug builds as in the original project.
set_source_files_properties(
    src/app_trig_lookup.c
    src/signal_preprocess.c
    src/signal_dual_sine.c
    src/signal_harmonic.c
    src/signal_joint_model.c
    PROPERTIES COMPILE_OPTIONS "-O3"
)
