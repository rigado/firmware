/** @file timer.c
*
* @brief This module handles interfacing with the app_timer library to
*        manage a timer to handle UART communications
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <string.h>
#include "advertising.h"
#include "app_timer.h"
#include "nordic_common.h"

#include "timer.h"

#define APP_TIMER_PRESCALER                  0                              /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS                 4                              /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE              10                             /**< Size of timer operation queues. */

#if (SDK_VERSION>=11)
    APP_TIMER_DEF(m_uart_timer);
#else
    #error "nRF IC Not Defined!"
#endif
static uint32_t m_uart_timeout_ms;
static bool m_uart_timer_started = false;    

/**@brief Create Uart Timeout Timer
*
* @details Create a timer for the uart sending timeout.  This timer will trigger data to send
*          if no data has been received from the UART for 50 ms.
*
* @param[in]   p_timeout_func   Callback function to handle the timer timeout
* @param[in]   timeout_ms       Milliseconds until timeout
*/
void timer_create_uart( app_timer_timeout_handler_t p_timeout_func, uint32_t timeout_ms, bool repeated )
{
    uint32_t err_code;
    app_timer_mode_t mode;
    
    mode = APP_TIMER_MODE_SINGLE_SHOT;
    if(repeated)
    {
        mode = APP_TIMER_MODE_REPEATED;
    }
    
    // Create timers.
    err_code = app_timer_create(&m_uart_timer,
                                mode,
                                p_timeout_func);
    APP_ERROR_CHECK(err_code);
    
    m_uart_timeout_ms = timeout_ms;
}    

/**@brief Stop Uart Timeout Timer
*/
void timer_stop_uart( void )
{
	if(m_uart_timer)
    {
        app_timer_stop(m_uart_timer);
        m_uart_timer_started = false;
    }
}

/**@brief Start Uart Timeout Timer
*/
void timer_start_uart( void )
{
    uint32_t err;
    
    if(!m_uart_timer_started)
    {
        m_uart_timer_started = true;
    }
    else
    {
        return;
    }
    
    err = app_timer_start(m_uart_timer, APP_TIMER_TICKS(m_uart_timeout_ms, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err);
}

/**@brief Function for the Timer initialization.
 *
* @details Initializes the timer module. This creates and starts application timers.
*/
void timers_init(void)
{
    if(m_uart_timer)
    {
        app_timer_stop_all();
    }
	
    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
}
