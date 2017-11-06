/** @file uart.c
*
* @brief This module handles interfacing with the UART peripheral for
*        all UART communications (DTM, Passthrough, and AT)
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __UART_H__
#define __UART_H__

#include "ble_nus.h"
#include "ringbuf.h"

typedef enum
{
	UART_MODE_INACTIVE,
	UART_MODE_DTM,
    UART_MODE_BMDWARE_AT,
    UART_MODE_BMDWARE_PT,
} uart_mode_t;

#define UART_TX_BUFFER_SIZE     (4096)

void uart_init_dtm(void);
void uart_deinit(void);
void uart_clear_buffer(void);
void uart_reset_counters(void);

void uart_configure_at_mode(void);
void uart_configure_direct_test_mode(void);
void uart_configure_passthrough_mode(ble_nus_t * p_uart_service);

void uart_disable_at_mode(void);
void uart_disable_passthrough_mode(void);

uart_mode_t uart_get_mode(void);

void uart_ble_timeout_handler(void * p_context);
void uart_ble_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length);
void uart_transfer_data(void);
uint32_t uart_get_tx_buffer_waiting(void);

void uart_set_rx_enable_state(bool state);

void uart_reg_tx_buf_event_callback(ringBufEvent_t event, 
        ringBufEventCallback_t callback);

void uart_force_pt_tx(void);

#endif
