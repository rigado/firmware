/** @file simple_uarte.h
*
* @brief This module configures and uses the DMA enhanced UART peripheral.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
#ifndef SIMPLE_UARTE_H_
#define SIMPLE_UARTE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DMA_BUFFER_SIZE     255
    
void simple_uarte_config(uint8_t rts_pin_number, uint8_t txd_pin_number,
                        uint8_t cts_pin_number, uint8_t rxd_pin_number,
                        bool hwfc, uint32_t baud_select, uint8_t parity_select);
void simple_uarte_enable(void (*rx_callback)(const uint8_t * const data, uint8_t len));
void simple_uarte_disable(void);

void simple_uarte_disable_rx(void);
void simple_uarte_enable_rx(void);

void simple_uarte_put(const uint8_t * data, uint8_t size, void (*callback)(void));
void simple_uarte_putstring(const uint8_t * str, void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif
