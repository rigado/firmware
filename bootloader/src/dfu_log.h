#ifndef DFU_LOG_H_
#define DFU_LOG_H_

#define NRF_LOG_USES_RTT    1
#define RTT_CHANNEL         0

#include <stdarg.h>
#include <stdlib.h>
#include "SEGGER_RTT.h"
#include "stdint.h"

#define dfu_log_init()  SEGGER_RTT_Init();
#define dfu_log(...)    SEGGER_RTT_printf(RTT_CHANNEL, __VA_ARGS__);

void print_buffer(uint8_t* buffer, uint8_t length);
void print_buffer32(uint32_t* buffer, uint8_t length);
#endif
