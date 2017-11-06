/** @file sys_init.h
*
* @brief This module provides methods to perform system initialization.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __SYS_INIT_H__
#define __SYS_INIT_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sys_init(void);
bool sys_init_is_dtm(void);
bool sys_init_is_beacon_only(void);
bool sys_init_is_uart_enabled(void);
bool sys_init_is_at_mode(void);
void sys_init_setup_at_mode_pin(void);

#ifdef __cplusplus
}
#endif

#endif
