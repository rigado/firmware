/** @file sys_init.c
*
* @brief This module provides methods to perform system initialization.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdbool.h>
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#ifdef NRF52
    #include "nrf_nvic.h"
    #include "nrf_gpiote.h"
#endif
#include "bmdware_gpio_def.h"
#include "storage_intf.h"
#include "sys_init.h"

static bool m_is_dtm;
static bool m_is_beacon_only_mode;
static volatile bool m_is_at_mode;

static void init_gpio(void);
static void determine_runtime_mode(void);
static void determine_op_mode(void);

void sys_init(void)
{
    init_gpio();
    
    /* If runtime mode is not DTM, the DTM pin will be disabled
     * after the determination is made */
    determine_runtime_mode();
    
    if(!m_is_dtm)
    {
        /* The beacon only pin will be disconnected after the mode is
         * determined. */
        determine_op_mode();
    }
}

bool sys_init_is_dtm(void)
{
    return m_is_dtm;
}

bool sys_init_is_beacon_only(void)
{
    return m_is_beacon_only_mode;
}

bool sys_init_is_uart_enabled(void)
{
    return !m_is_beacon_only_mode;
}

bool sys_init_is_at_mode(void)
{
    return m_is_at_mode;
}

void sys_init_setup_at_mode_pin(void)
{
    const default_app_settings_t* settings = storage_intf_get();
    
    nrf_gpio_cfg_input(BMD_AT_MODE_PIN, NRF_GPIO_PIN_PULLUP);
    m_is_at_mode = (nrf_gpio_pin_read(BMD_AT_MODE_PIN) == 0);

    /* setup the pin sense if hotswap is enabled */
    if(settings->at_hotswap_enabled)
    {
    #ifdef NRF52
        if(m_is_at_mode)
        {
            nrf_gpio_cfg_sense_input(BMD_AT_MODE_PIN, NRF_GPIO_PIN_PULLUP, 
                NRF_GPIO_PIN_SENSE_HIGH);
        }
        else
        {
            nrf_gpio_cfg_sense_input(BMD_AT_MODE_PIN, NRF_GPIO_PIN_PULLUP, 
                NRF_GPIO_PIN_SENSE_LOW);
        }
    #else
        //no at hotswap support for nrf51
    #endif
    }
    /* disconnect the pin, no gpio sense */
    else
    {
        nrf_gpio_cfg( BMD_AT_MODE_PIN,
                        NRF_GPIO_PIN_DIR_INPUT,
                        NRF_GPIO_PIN_INPUT_DISCONNECT,
                        NRF_GPIO_PIN_PULLUP,
                        NRF_GPIO_PIN_S0S1,
                        NRF_GPIO_PIN_NOSENSE);
    }
}

static void init_gpio(void)
{
    nrf_gpio_cfg_input(BMD_BCN_ONLY_PIN, NRF_GPIO_PIN_PULLUP);
    
#ifdef NRF52
    nrf_gpiote_int_disable(GPIOTE_INTENSET_PORT_Msk);
    nrf_gpio_cfg_sense_input(BMD_SOFT_RESET_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
    nrf_gpiote_int_enable(GPIOTE_INTENSET_PORT_Msk);
    NVIC_SetPriority(GPIOTE_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(GPIOTE_IRQn);
#endif
}

static void determine_runtime_mode(void)
{
    //start in bluetooth mode
	m_is_dtm = false;
    
    /* Configure DTM UART RX pin with pull up; if
    high after configuration, then a UART interface
    is connected to the DTM pin */
    nrf_gpio_cfg_input(BMD_DTM_UART_RXD, NRF_GPIO_PIN_PULLDOWN);
    
    /* Short delay */
    nrf_delay_us(250000);
    
    uint8_t rx_pin_state = nrf_gpio_pin_read(BMD_DTM_UART_RXD);
    if(rx_pin_state == 1)
    {
        /* DTM UART is connected; enable dtm mode */
        m_is_dtm = true;
        
        /* Reconfigure input to have no pull */
        nrf_gpio_cfg_input(BMD_DTM_UART_RXD, NRF_GPIO_PIN_NOPULL);
    }
    else
    {
        NRF_GPIO->PIN_CNF[BMD_DTM_UART_RXD] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    }
    
}

static void determine_op_mode(void)
{
    //start in full bmdware mode
	m_is_beacon_only_mode = false;
    
    /* Configure Beacon only mode pin with pull up; if
    low after configuration, then UART interface and server
    is disabled */
    nrf_gpio_cfg_input(BMD_BCN_ONLY_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Short delay */
    nrf_delay_us(100);
    
    uint8_t rx_pin_state = nrf_gpio_pin_read(BMD_BCN_ONLY_PIN);
    if(rx_pin_state == 0)
    {
        /* DTM UART is connected; enable dtm mode */
        m_is_beacon_only_mode = true;
    }
    
    NRF_GPIO->PIN_CNF[BMD_BCN_ONLY_PIN] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
}


#ifdef NRF52
void GPIOTE_IRQHandler(void)
{
    if(NRF_GPIOTE->EVENTS_PORT == 1)
    {
        NRF_GPIOTE->EVENTS_PORT = 0;
    }
    
    if(nrf_gpio_pin_read(BMD_SOFT_RESET_PIN) == 0)
    {
        NVIC_SystemReset();
    }
    
    if((NRF_GPIO->PIN_CNF[BMD_AT_MODE_PIN] & GPIO_PIN_CNF_SENSE_Msk) == 
        (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos))
    {
        if(nrf_gpio_pin_read(BMD_AT_MODE_PIN) == 0)
        {
            m_is_at_mode = true;
            nrf_gpio_cfg_sense_input(BMD_AT_MODE_PIN, NRF_GPIO_PIN_PULLUP, 
                NRF_GPIO_PIN_SENSE_HIGH);
        }
    }
    else if((NRF_GPIO->PIN_CNF[BMD_AT_MODE_PIN] & GPIO_PIN_CNF_SENSE_Msk) == 
        (GPIO_PIN_CNF_SENSE_High << GPIO_PIN_CNF_SENSE_Pos))
    {
        if(nrf_gpio_pin_read(BMD_AT_MODE_PIN) == 1)
        {
            m_is_at_mode = false;
            nrf_gpio_cfg_sense_input(BMD_AT_MODE_PIN, NRF_GPIO_PIN_PULLUP, 
                NRF_GPIO_PIN_SENSE_LOW);
        }
    }
}
#endif
