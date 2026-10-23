/**
  ******************************************************************************
  * @file    fpga_link_service.c
  * @brief   FPGA 双 SPI 会话服务与波形识别结果封装（UTF-8）。
  ******************************************************************************
  */

#include "fpga_link_service.h"

#include "app_output_task.h"
#include "cmsis_os2.h"
#include "dual_spi_protocol.h"
#include "dual_spi_session.h"
#include "dual_spi_stm32_port.h"
#include "dual_spi_transport.h"
#include "main.h"

#include <string.h>

#define FPGA_LINK_NOTIFY_APP_TX       UINT32_C(0x00000040)
#define FPGA_LINK_WAIT_FLAGS          (DUAL_SPI_PORT_NOTIFY_ALL | FPGA_LINK_NOTIFY_APP_TX)
#define FPGA_LINK_POLL_MS             5U
#define FPGA_LINK_TRANSFER_TIMEOUT_MS 100U
#define FPGA_LINK_RESULT_BODY_BYTES   12U

typedef struct
{
    uint8_t body[FPGA_LINK_RESULT_BODY_BYTES];
    uint16_t tag;
    volatile bool pending;
} FpgaLinkMailbox;

volatile FpgaLinkDiagnostics g_fpga_link_diagnostics;

static dual_spi_session_t s_sessions[2];
static FpgaLinkMailbox s_mailbox;
static volatile osThreadId_t s_task_id;
static uint16_t s_next_tag = 1U;

static bool fpga_link_valid_id(dual_spi_link_id_t link_id)
{
    return (link_id == DUAL_SPI_LINK_A) || (link_id == DUAL_SPI_LINK_B);
}

static uint32_t fpga_link_time_us(void)
{
    return HAL_GetTick() * UINT32_C(1000);
}

static uint32_t fpga_link_timeout_ticks(uint32_t timeout_ms)
{
    uint32_t tick_hz = osKernelGetTickFreq();
    uint64_t ticks = ((uint64_t)timeout_ms * tick_hz + 999ULL) / 1000ULL;
    return (ticks == 0U) ? 1U : (uint32_t)ticks;
}

static uint8_t fpga_link_wave_type(SignalWaveType type)
{
    if (type == SIGNAL_WAVE_SINE)
    {
        return 1U;
    }
    if (type == SIGNAL_WAVE_TRIANGLE)
    {
        return 2U;
    }
    return 0U;
}

static uint32_t fpga_link_frequency_mhz(float frequency_hz)
{
    if (frequency_hz <= 0.0f)
    {
        return 0U;
    }
    if (frequency_hz >= 4294967.0f)
    {
        return UINT32_MAX;
    }
    return (uint32_t)(frequency_hz * 1000.0f + 0.5f);
}

static void fpga_link_pack_result(const SignalVoteResult *result,
                                  uint8_t body[FPGA_LINK_RESULT_BODY_BYTES])
{
    uint32_t frequency_a_mhz = 0U;
    uint32_t frequency_b_mhz = 0U;
    uint8_t type_a = 0U;
    uint8_t type_b = 0U;

    if (result->valid)
    {
        frequency_a_mhz =
            fpga_link_frequency_mhz(result->components[0].frequency_hz);
        frequency_b_mhz =
            fpga_link_frequency_mhz(result->components[1].frequency_hz);
        type_a = fpga_link_wave_type(result->components[0].type);
        type_b = fpga_link_wave_type(result->components[1].type);
    }

    body[0] = 1U;
    body[1] = result->valid ? 1U : 0U;
    body[2] = type_a;
    body[3] = type_b;
    dual_spi_put_u32_le(&body[4], frequency_a_mhz);
    dual_spi_put_u32_le(&body[8], frequency_b_mhz);
}

bool FpgaLink_SubmitSignalResult(const SignalVoteResult *result)
{
    if ((result == NULL) || (__get_IPSR() != 0U) || !g_fpga_link_diagnostics.initialized)
    {
        ++g_fpga_link_diagnostics.dropped_results;
        return false;
    }
    if (s_mailbox.pending)
    {
        ++g_fpga_link_diagnostics.dropped_results;
        return false;
    }

    fpga_link_pack_result(result, s_mailbox.body);
    s_mailbox.tag = s_next_tag++;
    if (s_next_tag == 0U)
    {
        s_next_tag = 1U;
    }
    __DMB();
    s_mailbox.pending = true;
    ++g_fpga_link_diagnostics.submitted_results;
    g_fpga_link_diagnostics.last_tag = s_mailbox.tag;

    if (s_task_id != NULL)
    {
        (void)osThreadFlagsSet(s_task_id, FPGA_LINK_NOTIFY_APP_TX);
    }
    return true;
}

static void fpga_link_drain_mailbox(void)
{
    if (!s_mailbox.pending)
    {
        return;
    }

    if (dual_spi_session_queue_tlv(&s_sessions[DUAL_SPI_LINK_A],
                                   FPGA_LINK_TLV_SIGNAL_RESULT,
                                   s_mailbox.tag,
                                   s_mailbox.body,
                                   sizeof(s_mailbox.body)) == DUAL_SPI_RESULT_OK)
    {
        __DMB();
        s_mailbox.pending = false;
    }
}

static void fpga_link_receive_page(dual_spi_link_id_t link_id)
{
    dual_spi_transport_link_t *transport =
        dual_spi_stm32_port_link(link_id);
    const uint8_t *rx_page = dual_spi_stm32_port_rx_page(link_id);
    dual_spi_rx_disposition_t disposition = DUAL_SPI_RX_UNSEQUENCED;
    dual_spi_tx_observation_t observation =
        DUAL_SPI_TX_OBSERVE_NO_RESULT;
    dual_spi_result_t result;
    dual_spi_page_view_t page;

    if ((transport == NULL) || (rx_page == NULL))
    {
        return;
    }

    result = dual_spi_session_receive_page(&s_sessions[link_id],
                                           rx_page,
                                           DUAL_SPI_PAGE_BYTES,
                                           &disposition,
                                           &observation);
    if (result == DUAL_SPI_RESULT_OK)
    {
        ++g_fpga_link_diagnostics.received_pages;

        /*
         * 当前业务只要求 STM32 上报识别结果。收到的 TLV 完成协议校验后释放，
         * 后续若需要 FPGA 下发命令，可在这里按 type 增加明确分支。
         */
        if (dual_spi_session_peek_rx_page(&s_sessions[link_id], &page))
        {
            dual_spi_session_release_rx_page(&s_sessions[link_id]);
        }
    }
    else
    {
        ++g_fpga_link_diagnostics.protocol_errors;
        (void)AppOutput_Logf("FPGA_LINK,LINK=%u,PROTO_ERR=%u\n",
                             (unsigned int)link_id,
                             (unsigned int)result);
    }

    (void)dual_spi_transport_release_rx_page(transport);
}

static void fpga_link_recover(dual_spi_link_id_t link_id)
{
    dual_spi_transport_link_t *transport =
        dual_spi_stm32_port_link(link_id);

    if ((transport == NULL)
        || (dual_spi_transport_state(transport)
            != DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED))
    {
        return;
    }

    dual_spi_transport_fault_t fault =
        dual_spi_transport_last_fault(transport, NULL);
    dual_spi_port_result_t result =
        dual_spi_stm32_port_recover(link_id);
    if (result == DUAL_SPI_PORT_RESULT_OK)
    {
        ++g_fpga_link_diagnostics.recoveries;
    }
    else if (result != DUAL_SPI_PORT_RESULT_NSS_ACTIVE)
    {
        (void)AppOutput_Logf("FPGA_LINK,LINK=%u,RECOVERY_ERR=%u,FAULT=%u\n",
                             (unsigned int)link_id,
                             (unsigned int)result,
                             (unsigned int)fault);
    }
}

static bool fpga_link_build_page(dual_spi_link_id_t link_id)
{
    dual_spi_transport_link_t *transport =
        dual_spi_stm32_port_link(link_id);
    if ((transport == NULL)
        || (dual_spi_transport_state(transport)
            != DUAL_SPI_TRANSPORT_STATE_IDLE))
    {
        return false;
    }

    uint8_t *tx_page = dual_spi_transport_tx_page(transport);
    if (tx_page == NULL)
    {
        return false;
    }

    return dual_spi_session_build_tx_page(
               &s_sessions[link_id],
               tx_page,
               fpga_link_time_us(),
               dual_spi_session_rx_credit_pages(&s_sessions[link_id]))
           == DUAL_SPI_RESULT_OK;
}

static void fpga_link_service_one(dual_spi_link_id_t link_id)
{
    if (!fpga_link_valid_id(link_id))
    {
        return;
    }

    fpga_link_recover(link_id);
    dual_spi_transport_link_t *transport =
        dual_spi_stm32_port_link(link_id);
    if (transport == NULL)
    {
        return;
    }

    if (dual_spi_transport_state(transport)
        == DUAL_SPI_TRANSPORT_STATE_RX_READY)
    {
        fpga_link_receive_page(link_id);
    }
    if (!fpga_link_build_page(link_id))
    {
        return;
    }

    dual_spi_port_result_t result =
        (link_id == DUAL_SPI_LINK_A)
            ? dual_spi_stm32_port_start_master_a()
            : dual_spi_stm32_port_arm_slave_b();
    if (result == DUAL_SPI_PORT_RESULT_OK)
    {
        dual_spi_session_mark_tx_attempted(&s_sessions[link_id]);
    }
}

void FpgaLink_Run(void)
{
    memset((void *)&g_fpga_link_diagnostics, 0,
           sizeof(g_fpga_link_diagnostics));
    memset(&s_mailbox, 0, sizeof(s_mailbox));
    dual_spi_session_init(&s_sessions[DUAL_SPI_LINK_A], DUAL_SPI_LINK_A);
    dual_spi_session_init(&s_sessions[DUAL_SPI_LINK_B], DUAL_SPI_LINK_B);

    s_task_id = osThreadGetId();
    if ((s_task_id == NULL)
        || (dual_spi_stm32_port_init(s_task_id)
            != DUAL_SPI_PORT_RESULT_OK))
    {
        (void)AppOutput_Logf("FPGA_LINK,INIT=FAILED\n");
        for (;;)
        {
            osDelay(fpga_link_timeout_ticks(1000U));
        }
    }

    g_fpga_link_diagnostics.initialized = 1U;

    for (;;)
    {
        fpga_link_drain_mailbox();
        fpga_link_service_one(DUAL_SPI_LINK_A);
        fpga_link_service_one(DUAL_SPI_LINK_B);

        (void)osThreadFlagsWait(FPGA_LINK_WAIT_FLAGS,
                                osFlagsWaitAny,
                                fpga_link_timeout_ticks(FPGA_LINK_POLL_MS));
        (void)dual_spi_stm32_port_poll_timeouts(
            FPGA_LINK_TRANSFER_TIMEOUT_MS);
    }
}


