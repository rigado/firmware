 /* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
/** @file simple_uart.h
*
* @brief Simple uart driver interface
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef SIMPLE_UART_H
#define SIMPLE_UART_H

/*lint ++flb "Enter library region" */

#include <stdbool.h>
#include <stdint.h>

/** @file
* @brief Simple UART driver
*
*
* @defgroup nrf_drivers_simple_uart Simple UART driver
* @{
* @ingroup nrf_drivers
* @brief Simple UART driver
*/

typedef void (*simple_uart_rx_callback_t)(const uint8_t rx_byte);
typedef void (*simple_uart_tx_callback_t)(void);
typedef bool (*simple_uart_canrx_callback_t)(void);

/** @brief Function for sending a character to UART.
Execution is blocked until UART peripheral reports character to have been send.
@param cr Character to send.
*/
void simple_uart_blocking_put(uint8_t cr);
void simple_uart_blocking_putstring(const uint8_t *str);
void simple_uart_blocking_put_n(const uint8_t *p_data, uint32_t length);

void simple_uart_put_nonblocking(uint8_t cr);

/** @brief Function for configuring UART.
@param rts_pin_number Chip pin number to be used for UART RTS
@param txd_pin_number Chip pin number to be used for UART TXD
@param cts_pin_number Chip pin number to be used for UART CTS
@param rxd_pin_number Chip pin number to be used for UART RXD
@param hwfc Enable hardware flow control
@param baud_select Selectable baud rate
*/
void simple_uart_config(uint8_t rts_pin_number, uint8_t txd_pin_number, uint8_t cts_pin_number, uint8_t rxd_pin_number, bool hwfc, uint32_t baud_select, uint8_t parity_select);
void simple_uart_set_rx_callback(simple_uart_rx_callback_t cb);
void simple_uart_set_tx_callback(simple_uart_tx_callback_t cb);
void simple_uart_set_canrx_callback(simple_uart_canrx_callback_t cb);

void simple_uart_disable( void );
void simple_uart_enable_rx( void );
void simple_uart_disable_rx( void );
bool simple_uart_get_rx_enable( void );
/**
 *@}
 **/

/*lint --flb "Leave library region" */
#endif
