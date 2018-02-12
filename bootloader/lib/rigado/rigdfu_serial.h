#ifndef RIGDFU_SERIAL_H
#define RIGDFU_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include "app_scheduler.h"

typedef void (*rigdfu_serial_rx_handler_t)(uint8_t x);

/* Initialize UART for the initial host detection. */
void rigdfu_serial_init(bool force_init);

/* Re-initialize UART for the actual DFU, which may have different
   settings (flow control) */
void rigdfu_serial_reinit_dfu(void);

/* Fully close UART. */
void rigdfu_serial_close(void);

/* Set the UART RX handler */
void rigdfu_serial_set_rx_handler(rigdfu_serial_rx_handler_t handler);

/* Send byte to the UART */
void rigdfu_serial_put(uint8_t x);

/* Send string */
void rigdfu_serial_puts(const char *s);

/* Print formatted string to UART; defined in snprintf.c */
int rigdfu_serial_printf(const char *fmt, ...);

#endif
