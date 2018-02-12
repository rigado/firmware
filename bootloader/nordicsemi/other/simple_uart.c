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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "nrf.h"
#include "simple_uart.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

static bool                 s_uart_initd = false;
static simple_uart_config_t s_uart_config;

uint8_t simple_uart_get(void)
{
    while (NRF_UART0->EVENTS_RXDRDY != 1)
    {
        // Wait for RXD data to be received
    }

    NRF_UART0->EVENTS_RXDRDY = 0;
    return (uint8_t)NRF_UART0->RXD;
}


bool simple_uart_get_with_timeout(int32_t timeout_ms, uint8_t * rx_data)
{
    bool ret = true;

    while (NRF_UART0->EVENTS_RXDRDY != 1)
    {
        if (timeout_ms-- >= 0)
        {
            // wait in 1ms chunk before checking for status.
            nrf_delay_us(1000);
        }
        else
        {
            ret = false;
            break;
        }
    } // Wait for RXD data to be received.

    if (timeout_ms >= 0)
    {
        // clear the event and set rx_data with received byte.
        NRF_UART0->EVENTS_RXDRDY = 0;
        *rx_data                 = (uint8_t)NRF_UART0->RXD;
    }

    return ret;
}

void simple_uart_put(uint8_t cr)
{
    if(s_uart_initd && NRF_UART0->PSELTXD < 32)
    {
        //configure for tx
        NRF_UART0->TASKS_STARTTX = 1;
        nrf_gpio_cfg_output(NRF_UART0->PSELTXD);
        
        //write byte
        NRF_UART0->TXD = (uint8_t)cr;
        
        //wait for tx
        while(NRF_UART0->EVENTS_TXDRDY != 1);
        
        //clear evt
        NRF_UART0->EVENTS_TXDRDY = 0;
        
        //stop tx
        NRF_UART0->TASKS_STOPTX = 1;
        nrf_gpio_cfg_input(s_uart_config.tx_pin,NRF_GPIO_PIN_PULLUP);
    }
}


void simple_uart_putstring(const uint8_t * str)
{
    uint_fast8_t i  = 0;
    uint8_t      ch = str[i++];

    while (ch != '\0')
    {
        simple_uart_put(ch);
        ch = str[i++];
    }
}


void simple_uart_deinit(void)
{
    s_uart_initd = false;
    
    NRF_UART0->TASKS_STOPTX = 1;
    NRF_UART0->TASKS_STOPRX = 1;
    
    NRF_UART0->ENABLE   = (UART_ENABLE_ENABLE_Disabled << UART_ENABLE_ENABLE_Pos);
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->PSELTXD  = 0xffffffff;
    NRF_UART0->PSELRXD  = 0xffffffff;
    NRF_UART0->PSELRTS  = 0xffffffff;
    NRF_UART0->PSELCTS  = 0xffffffff;
    NRF_UART0->CONFIG   = (UART_CONFIG_HWFC_Disabled << UART_CONFIG_HWFC_Pos);
    
    //set input pullups on rx/tx
    if(s_uart_config.rx_pin < 32)
        nrf_gpio_cfg_input(s_uart_config.rx_pin, NRF_GPIO_PIN_PULLUP);
    
    if(s_uart_config.tx_pin < 32)
        nrf_gpio_cfg_input(s_uart_config.tx_pin, NRF_GPIO_PIN_PULLUP);
    
    //rts/cts pull?
    
    //clear config
    memset(&s_uart_config, 0, sizeof(s_uart_config));
}

void simple_uart_init(simple_uart_config_t* config)
{
    if(config != NULL)
    {
        simple_uart_deinit();
        
        //tx pin
        if(config->tx_pin < 32)
        {
            NRF_UART0->PSELTXD = config->tx_pin;
        }
        
        //rx pin
        if(config->rx_pin < 32)
        {
            NRF_UART0->PSELRXD = config->rx_pin;
            nrf_gpio_cfg_input(config->rx_pin, NRF_GPIO_PIN_PULLUP);
        }
        
        //hwfc config
        if(config->hwfc)
        {
            nrf_gpio_cfg_output(config->rts_pin);
            nrf_gpio_cfg_input(config->cts_pin, NRF_GPIO_PIN_NOPULL);
            NRF_UART0->PSELCTS = config->cts_pin;
            NRF_UART0->PSELRTS = config->rts_pin;
            NRF_UART0->CONFIG  = (UART_CONFIG_HWFC_Enabled << UART_CONFIG_HWFC_Pos);
        }

        NRF_UART0->INTENCLR      = 0xffffffff;
        NRF_UART0->BAUDRATE      = (config->baud_enum << UART_BAUDRATE_BAUDRATE_Pos);
        NRF_UART0->ENABLE        = (UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos);
        
        NRF_UART0->TASKS_STOPTX = 1;
        NRF_UART0->TASKS_STOPRX = 1;
        
        //start rx
        if(NRF_UART0->PSELRXD < 32)
        {
            NRF_UART0->TASKS_STARTRX = 1;
            NRF_UART0->EVENTS_RXDRDY = 0;
        }
        
        //tx will be started/stopped automatically via put()
        
        //save
        memcpy(&s_uart_config, config, sizeof(s_uart_config));
        
        //init flag
        s_uart_initd = true;
    }
}
