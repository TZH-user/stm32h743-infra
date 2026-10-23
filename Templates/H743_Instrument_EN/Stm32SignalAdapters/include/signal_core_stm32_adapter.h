#ifndef STM32_SIGNAL_ADAPTERS_SIGNAL_CORE_STM32_ADAPTER_H_
#define STM32_SIGNAL_ADAPTERS_SIGNAL_CORE_STM32_ADAPTER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind AD9226 raw-code conversion and CMSIS-RTOS system timing to SignalCore. */
bool SignalCoreStm32Adapter_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32_SIGNAL_ADAPTERS_SIGNAL_CORE_STM32_ADAPTER_H_ */
