add_library(stm32_signal_adapters STATIC
    src/signal_core_stm32_adapter.c
    src/ad9226_hal_adapter.c
    src/dual_dac_dma_adapter.c
    src/signal_separation_dac_port.c
    src/cmsis_rtos_tracking_task.c
    src/uart_dma_output_service.c
)

target_include_directories(stm32_signal_adapters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/APP/Config
    ${CMAKE_SOURCE_DIR}/APP/RTOS
    ${CMAKE_SOURCE_DIR}/APP/Debug
    ${CMAKE_SOURCE_DIR}/BSP/ADC
    ${CMAKE_SOURCE_DIR}/Core/Inc
)

target_link_libraries(stm32_signal_adapters PUBLIC
    signal_core
    stm32cubemx
    m
)

target_compile_definitions(stm32_signal_adapters PRIVATE
    DEBUG_IO_OWN_HAL_UART_CALLBACKS=0
)

set_source_files_properties(
    src/dual_dac_dma_adapter.c
    PROPERTIES COMPILE_OPTIONS "-O3"
)
