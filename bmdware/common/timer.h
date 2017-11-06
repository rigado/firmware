/** @file timer.c
*
* @brief This module handles interfacing with the app_timer library to
*        manage a timer to handle UART communications
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __TIMER_H__
#define __TIMER_H__

#include "app_timer.h"

void timers_init(void);

void timer_create_uart(app_timer_timeout_handler_t p_timeout_func, uint32_t timeout_ms, bool repeated);
void timer_start_uart(void);
void timer_stop_uart(void);

#endif
