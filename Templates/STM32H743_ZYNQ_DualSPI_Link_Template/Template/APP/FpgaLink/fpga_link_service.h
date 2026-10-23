#ifndef APP_FPGA_LINK_FPGA_LINK_SERVICE_H_
#define APP_FPGA_LINK_FPGA_LINK_SERVICE_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_vote.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 波形识别结果业务 TLV，正文固定为 12 字节、小端序（UTF-8）。 */
#define FPGA_LINK_TLV_SIGNAL_RESULT UINT16_C(0x0301)

typedef struct
{
    uint32_t initialized;
    uint32_t submitted_results;
    uint32_t dropped_results;
    uint32_t protocol_errors;
    uint32_t received_pages;
    uint32_t recoveries;
    uint16_t last_tag;
    uint16_t reserved;
} FpgaLinkDiagnostics;

extern volatile FpgaLinkDiagnostics g_fpga_link_diagnostics;

/**
 * @brief 把一次 E7 最终结果提交给 FPGA 链路任务。
 * @return true 表示结果已复制到单槽邮箱；不表示 FPGA 已完成 ACK。
 */
bool FpgaLink_SubmitSignalResult(const SignalVoteResult *result);

/** @brief FpgaLinkTask 的常驻服务循环，不从其他任务直接调用。 */
void FpgaLink_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FPGA_LINK_FPGA_LINK_SERVICE_H_ */


