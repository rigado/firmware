#ifndef __CUSTOM_BOARD_H__
#define __CUSTOM_BOARD_H__

#ifdef BMD200_EVAL_V31
#define BMD_UART_RTS      	        3
#define BMD_UART_CTS      	        2
#define BMD_UART_RXD      	        9
#define BMD_UART_TXD      	        10
#define BMD_UART_HWFC               true
#define BMD_AT_MODE_PIN   	        11
#define BMD_BCN_ONLY_PIN            5
#define BMD_LED_RED                 1
#define BMD_LED_GREEN               24
#define BMD_LED_BLUE                25
#elif BMDWARE_SCS_001
#define BMD_UART_RTS      	        11
#define BMD_UART_CTS      	        8 
#define BMD_UART_RXD      	        9
#define BMD_UART_TXD      	        10
#define BMD_UART_HWFC               false
#define BMD_AT_MODE_PIN   	        6
#define BMD_BCN_ONLY_PIN            5
#define BMD_LED_RED                 13
#define BMD_LED_GREEN               15
#define BMD_LED_BLUE                14
#define SCS_AMP_CPS                 18
#define SCS_AMP_CSD                 17
#else
#define BMD_UART_RTS      	        11
#define BMD_UART_CTS      	        8 
#define BMD_UART_RXD      	        9
#define BMD_UART_TXD      	        10
#define BMD_UART_HWFC               false
#define BMD_AT_MODE_PIN   	        6
#define BMD_BCN_ONLY_PIN            5

#endif

#define BMD_DTM_UART_RXD			0
#define BMD_DTM_UART_TXD			1
#define BMD_DTM_UART_BAUD		    UART_BAUDRATE_BAUDRATE_Baud19200	
#define BMD_DTM_UART_HWFC 		    false
#define BMD_DTM_UART_PARITY	        0

#endif
