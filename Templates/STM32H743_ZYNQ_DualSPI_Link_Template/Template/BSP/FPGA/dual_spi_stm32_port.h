#ifndef DUAL_SPI_STM32_PORT_H
#define DUAL_SPI_STM32_PORT_H

#include "dual_spi_transport.h"

#include "cmsis_os2.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DUAL_SPI_PORT_NOTIFY_A_READY UINT32_C(0x00000001)
#define DUAL_SPI_PORT_NOTIFY_A_RX    UINT32_C(0x00000002)
#define DUAL_SPI_PORT_NOTIFY_A_FAULT UINT32_C(0x00000004)
#define DUAL_SPI_PORT_NOTIFY_B_RX    UINT32_C(0x00000008)
#define DUAL_SPI_PORT_NOTIFY_B_FAULT UINT32_C(0x00000010)
#define DUAL_SPI_PORT_NOTIFY_B_START UINT32_C(0x00000020)
#define DUAL_SPI_PORT_NOTIFY_ALL     UINT32_C(0x0000003F)

typedef enum
{
    DUAL_SPI_PORT_RESULT_OK = 0,
    DUAL_SPI_PORT_RESULT_INVALID_ARGUMENT,
    DUAL_SPI_PORT_RESULT_BUSY,
    DUAL_SPI_PORT_RESULT_PEER_NOT_READY,
    DUAL_SPI_PORT_RESULT_NSS_ACTIVE,
    DUAL_SPI_PORT_RESULT_HAL_ERROR
} dual_spi_port_result_t;

dual_spi_port_result_t dual_spi_stm32_port_init(osThreadId_t task_id);
dual_spi_transport_link_t *dual_spi_stm32_port_link(dual_spi_link_id_t link_id);

dual_spi_port_result_t dual_spi_stm32_port_start_master_a(void);
dual_spi_port_result_t dual_spi_stm32_port_arm_slave_b(void);
dual_spi_port_result_t dual_spi_stm32_port_recover(dual_spi_link_id_t link_id);

/* Called from task context; returns task notification bits for timed-out links. */
uint32_t dual_spi_stm32_port_poll_timeouts(uint32_t timeout_ms);

/* Invalidates the completed RX page when D-cache is enabled, then returns it. */
const uint8_t *dual_spi_stm32_port_rx_page(dual_spi_link_id_t link_id);

/* SPI6 BDMA counters captured when the current slave-B transfer faults. */
uint32_t dual_spi_stm32_port_slave_b_last_error_dma_remaining(void);

/* SPI-A READY sampling counters used during board bring-up diagnostics. */
uint32_t dual_spi_stm32_port_a_ready_high_attempts(void);
uint32_t dual_spi_stm32_port_a_ready_low_rejects(void);

/* 由工程唯一的 HAL_GPIO_EXTI_Callback() 统一分发到这里。 */
void DualSpiStm32Port_OnGpioExti(uint16_t gpio_pin);

#ifdef __cplusplus
}
#endif

#endif



