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

typedef struct
{
    uint8_t rx_pin;
    uint8_t tx_pin;
    uint8_t rts_pin;
    uint8_t cts_pin;
    bool    hwfc;
    uint32_t baud_enum;
} simple_uart_config_t;


/** @brief Function for reading a character from UART.
Execution is blocked until UART peripheral detects character has been received.
\return cr Received character.
*/
uint8_t simple_uart_get(void);

/** @brief Function for reading a character from UART with timeout on how long to wait for the byte to be received.
Execution is blocked until UART peripheral detects character has been received or until the timeout expires, which even occurs first
\return bool True, if byte is received before timeout, else returns False.
@param timeout_ms maximum time to wait for the data.
@param rx_data pointer to the memory where the received data is stored.
*/
bool simple_uart_get_with_timeout(int32_t timeout_ms, uint8_t *rx_data);

/** @brief Function for sending a character to UART.
Execution is blocked until UART peripheral reports character to have been send.
@param cr Character to send.
*/
void simple_uart_put(uint8_t cr);

/** @brief Function for sending a string to UART.
Execution is blocked until UART peripheral reports all characters to have been send.
Maximum string length is 254 characters including null character in the end.
@param str Null terminated string to send.
*/
void simple_uart_putstring(const uint8_t *str);

/** @brief Function for configuring UART
*/
void simple_uart_init(simple_uart_config_t* config);

void simple_uart_deinit(void);

/**
 *@}
 **/

/*lint --flb "Leave library region" */
#endif
