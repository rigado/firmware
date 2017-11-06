#ifndef __CUSTOM_BOARD_H__
#define __CUSTOM_BOARD_H__

#ifdef S132
#define BMD_UART_RTS      	        5
#define BMD_UART_CTS      	        7
#define BMD_UART_RXD      	        8
#define BMD_UART_TXD      	        6
#define BMD_UART_HWFC               true
#define BMD_AT_MODE_PIN   	        14
#define BMD_BCN_ONLY_PIN            13
#define BMD_SOFT_RESET_PIN          21
#define BMD_DTM_UART_RXD		    12
#define BMD_DTM_UART_TXD		    11
#define BMD_DTM_UART_BAUD		    19200	
#define BMD_DTM_UART_HWFC 		    false
#define BMD_DTM_UART_PARITY	        0
#elif defined(S130)
#define BMD_UART_RTS      	        11
#define BMD_UART_CTS      	        8 
#define BMD_UART_RXD      	        9
#define BMD_UART_TXD      	        10
#define BMD_UART_HWFC               false
#define BMD_AT_MODE_PIN   	        6
#define BMD_BCN_ONLY_PIN            5
#define BMD_DTM_UART_RXD			0
#define BMD_DTM_UART_TXD			1
#define BMD_DTM_UART_BAUD		    19200	
#define BMD_DTM_UART_HWFC 		    false
#define BMD_DTM_UART_PARITY	        0
#endif

#endif
