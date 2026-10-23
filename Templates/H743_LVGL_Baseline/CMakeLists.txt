cmake_minimum_required(VERSION 3.22)

#
# H743_LVGL_Baseline —— 母板工程(基线)
# 底座:STM32H743 + FreeRTOS(CMSIS-RTOS2)+ LVGL 9.5 + LTDC RGB 屏 + GT9xx 触摸 + SDRAM
# 用法:整目录复制改名后,只需改下面 CMAKE_PROJECT_NAME 与 .ioc 文件名,即可编译点亮。
# 扩展:从 F:\STM32\stm32project\Function 复制模块目录进来,取消对应注释段即可,
#       各段注释已按模块写好(搜索 "Function 模块")。
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name(新项目改这里,并把根目录 .ioc 同步改名+改内部 ProjectName)
set(CMAKE_PROJECT_NAME H743_LVGL_Baseline)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)


# ============================ LVGL ============================
set(LVGL_DIR ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/lvgl_9.5.0)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${LVGL_DIR}/src/*.c"
)

add_library(lvgl STATIC ${LVGL_SOURCES})

# lv_conf.h 位于 Core/Inc(LV_CONF_INCLUDE_SIMPLE 方式)
target_include_directories(lvgl PUBLIC
    "${LVGL_DIR}"
    "${CMAKE_SOURCE_DIR}/APP/Config"
    "${CMAKE_SOURCE_DIR}/Core/Inc"
    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include"
    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include"
)

target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
    LV_KCONFIG_IGNORE
)


# ==================== CMSIS-DSP(可选,默认关) ====================
# 需要 FFT/DSP 时:
# 1) 从任一旧工程把 Drivers/CMSIS/DSP 的 Include 与下列 10 个源文件拷入本工程
#    (清单说明见 Function\Algorithm\README_算法模块索引.md)
# 2) 取消本段注释,并在最下方 target_link_libraries 里加 cmsis_dsp_fft
#
# set(CMSIS_DSP_DIR ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/DSP)
# add_library(cmsis_dsp_fft STATIC
#     ${CMSIS_DSP_DIR}/Source/CommonTables/arm_common_tables.c
#     ${CMSIS_DSP_DIR}/Source/CommonTables/arm_const_structs.c
#     ${CMSIS_DSP_DIR}/Source/ControllerFunctions/arm_sin_cos_f32.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_bitreversal2.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_cfft_f32.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_cfft_init_f32.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_cfft_radix8_f32.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_rfft_fast_f32.c
#     ${CMSIS_DSP_DIR}/Source/TransformFunctions/arm_rfft_fast_init_f32.c
#     ${CMSIS_DSP_DIR}/Source/ComplexMathFunctions/arm_cmplx_mag_f32.c
# )
# target_include_directories(cmsis_dsp_fft PUBLIC
#     ${CMSIS_DSP_DIR}/Include
#     ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
# )
# target_compile_definitions(cmsis_dsp_fft PUBLIC ARM_MATH_CM7)
# target_compile_options(cmsis_dsp_fft PRIVATE -O3)


# ============== Function 算法库(可选,默认关) ==============
# 从 F:\STM32\stm32project\Function\Algorithm 复制整目录进来后取消注释,
# 并在最下方 target_link_libraries 里加 signal_core / calibration_core / stm32_signal_adapters。
# 注意:SignalCore 依赖 cmsis_dsp_fft;Stm32SignalAdapters 与 SignalCore 成对使用。
#
# add_subdirectory(SignalCore)
# add_subdirectory(CalibrationCore)
# add_subdirectory(Stm32SignalAdapters)


# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# ======================== 用户源文件 ========================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # --- 预装:显示/触摸/LVGL 桥接(不用屏幕时注释掉本组,并删对应目录) ---
    BSP/LTDC/app_display.c
    BSP/SDRAM/app_sdram_test.c
    BSP/Touch/app_touch_gt9xx.c
    UI/app_lvgl.c
    UI/Port/app_lvgl_port_disp.c
    UI/Port/app_lvgl_port_indev.c
    UI/pages/app_lvgl_ui.c

    # --- 预装:调试串口(vcp_printf,独立 DMA 环形缓冲) ---
    APP/Debug/debug_io.c

    # --- 预装:RTOS 显示任务(覆盖 freertos.c 的 __weak 任务) ---
    APP/RTOS/app_display_task.c

    # ============ Function 硬件模块接入模板(复制目录后取消注释) ============
    # 模块来源:F:\STM32\stm32project\Function\Hardware\<模块名>
    #
    # --- ADC_AD9226(并行 ADC,TIM+DMA 采集;拷到 BSP/ADC/) ---
    # BSP/ADC/ad9226.c
    #
    # --- DDS_AD9959(四通道 DDS;拷到 APP/DDS_9959/) ---
    # APP/DDS_9959/Hardware/ad9959.c
    # APP/DDS_9959/Hardware/ad9959_port.c
    # APP/DDS_9959/App_9959/app_ad9959.c
    # APP/DDS_9959/App_9959/app_dds_ctrl.c
    #
    # --- DDS_AD9910(单通道 DDS;拷到 APP/DDS_9910/;与 AD9959 的 app_dds_ctrl 二选一) ---
    # APP/DDS_9910/Hardware/ad9910.c
    # APP/DDS_9910/Hardware/ad9910_port_h743.c
    # APP/DDS_9910/App_9910/app_ad9910.c
    # APP/DDS_9910/App_9910/app_dds_ctrl.c
    #
    # --- CLK_SI5351(I2C 时钟发生器;拷到 APP/SI5351/) ---
    # APP/SI5351/Wrapper/app_si5351_drv.c
    # APP/SI5351/Port/si5351_port.c
    # APP/SI5351/Legacy/common/si5351_legacy_common.c
    # APP/SI5351/Legacy/promax/SI5351PROMAX.c   # basic/pro 变体按需追加
    #
    # --- Flash_W25Q64_QSPI(QSPI Flash;拷到 APP/BoardFlash/) ---
    # APP/BoardFlash/app_board_flash.c
    # APP/BoardFlash/board_flash_qspi_port.c
    # APP/BoardFlash/board_flash_w25q64.c
    #
    # --- DAC_WaveGen(片内 DAC 波形;拷到 APP/DAC/) ---
    # APP/DAC/app_dac_wavegen.c
    #
    # --- Buzzer(拷到 APP/Buzzer/) ---
    # APP/Buzzer/app_buzzer.c
    #
    # --- OCXO_Cal(OCXO 压控校准;依赖 BoardFlash 与 DDS 控制头;拷到 APP/OCXO/) ---
    # APP/OCXO/app_ocxo_cal.c
)

# ======================== include 路径 ========================
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    APP/Config
    APP/Debug
    APP/RTOS
    BSP/LTDC
    BSP/SDRAM
    BSP/Touch
    UI
    UI/Port
    UI/pages

    # ---- Function 模块接入后取消对应注释 ----
    # BSP/ADC
    # APP/DDS_9959/Hardware
    # APP/DDS_9959/App_9959
    # APP/DDS_9910/Hardware
    # APP/DDS_9910/App_9910
    # APP/SI5351/Config
    # APP/SI5351/Port
    # APP/SI5351/Wrapper
    # APP/SI5351/Legacy/common
    # APP/SI5351/Legacy/basic
    # APP/SI5351/Legacy/pro
    # APP/SI5351/Legacy/promax
    # APP/BoardFlash
    # APP/DAC
    # APP/Buzzer
    # APP/OCXO
    # SignalCore/include
    # CalibrationCore/include
    # Stm32SignalAdapters/include
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    m
    # cmsis_dsp_fft
    # signal_core
    # calibration_core
    # stm32_signal_adapters
)

# 每次重新链接后同步生成烧录文件,避免 HEX/BIN 落后于 ELF。
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating synchronized HEX and BIN images"
)
