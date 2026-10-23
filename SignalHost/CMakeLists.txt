cmake_minimum_required(VERSION 3.24)

project(SignalHost LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SIGNALHOST_CORE_DIR
    "F:/STM32/stm32project/Function/Algorithm/SignalCore"
    CACHE PATH "SignalCore source directory used by the host simulator")

file(TO_CMAKE_PATH "${SIGNALHOST_CORE_DIR}" SIGNALHOST_CORE_DIR)

set(SIGNALHOST_REQUIRED_CORE_FILES
    include/signal_core_port.h
    include/signal_preprocess.h
    include/signal_fft.h
    include/signal_joint_model.h
    include/signal_vote.h
    src/signal_core_port.c
    src/signal_preprocess.c
    src/signal_fft.c
    src/signal_dual_sine.c
    src/signal_harmonic.c
    src/signal_joint_model.c
    src/signal_vote.c
    src/app_phase_lock.c)

foreach(required_file IN LISTS SIGNALHOST_REQUIRED_CORE_FILES)
    if(NOT EXISTS "${SIGNALHOST_CORE_DIR}/${required_file}")
        message(FATAL_ERROR
            "SIGNALHOST_CORE_DIR is incomplete: missing ${required_file}\n"
            "Current directory: ${SIGNALHOST_CORE_DIR}")
    endif()
endforeach()

message(STATUS "SignalHost external SignalCore: ${SIGNALHOST_CORE_DIR}")

add_library(signalhost_cmsis_compat STATIC
    src/compat/arm_math_compat.c
    src/compat/app_trig_lookup_host.c)

target_include_directories(signalhost_cmsis_compat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
    ${SIGNALHOST_CORE_DIR}/include)

add_library(signal_core_host STATIC
    ${SIGNALHOST_CORE_DIR}/src/signal_core_port.c
    ${SIGNALHOST_CORE_DIR}/src/signal_preprocess.c
    ${SIGNALHOST_CORE_DIR}/src/signal_fft.c
    ${SIGNALHOST_CORE_DIR}/src/signal_dual_sine.c
    ${SIGNALHOST_CORE_DIR}/src/signal_harmonic.c
    ${SIGNALHOST_CORE_DIR}/src/signal_joint_model.c
    ${SIGNALHOST_CORE_DIR}/src/signal_vote.c
    ${SIGNALHOST_CORE_DIR}/src/app_phase_lock.c)

target_include_directories(signal_core_host PUBLIC
    ${SIGNALHOST_CORE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compat)

target_link_libraries(signal_core_host PUBLIC signalhost_cmsis_compat)

add_executable(signalhost
    src/host_main.c)

target_link_libraries(signalhost PRIVATE signal_core_host)
target_compile_definitions(signalhost PRIVATE
    SIGNALHOST_CORE_DIR_TEXT="${SIGNALHOST_CORE_DIR}")

if(MSVC)
    target_compile_options(signalhost_cmsis_compat PRIVATE /W4 /O2 /utf-8)
    target_compile_options(signal_core_host PRIVATE
        /W4 /O2 /utf-8
        /FI${CMAKE_CURRENT_SOURCE_DIR}/src/compat/signalhost_compiler_compat.h)
    target_compile_options(signalhost PRIVATE /W4 /O2 /utf-8)
    target_compile_definitions(signalhost PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(signalhost_cmsis_compat PRIVATE -Wall -Wextra -O3)
    target_compile_options(signal_core_host PRIVATE
        -Wall -Wextra -O3
        -include ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/signalhost_compiler_compat.h)
    target_compile_options(signalhost PRIVATE -Wall -Wextra -O2)
    target_link_libraries(signalhost PRIVATE m)
endif()

set_target_properties(signalhost PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
