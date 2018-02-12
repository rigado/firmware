#ifndef BOARDS_H
#define BOARDS_H

/* UART */
#ifdef NRF52
#define RX_PIN_NUMBER  8
#define TX_PIN_NUMBER  6
#define RTS_PIN_NUMBER 5
#define CTS_PIN_NUMBER 7
#elif defined(NRF51)
#define RX_PIN_NUMBER  9
#define TX_PIN_NUMBER  10
#define RTS_PIN_NUMBER 3
#define CTS_PIN_NUMBER 2
#endif

#define BAUDRATE_ENUM UART_BAUDRATE_BAUDRATE_Baud115200
#define BAUDRATE_INT  1152000u

/* If HWFC is true, use hardware flow control during serial DFU.

   Regardless of HWFC, the CTS and RTS pins are left floating during
   initial host detection, so that external signals are not
   unnecessarily driven.  If RTS is needed by the external hardware,
   as is the case with the JLink-OB, an external pulldown should be
   used. */
#define HWFC true

#endif
