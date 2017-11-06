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
/** @file simple_uart.c
*
* @brief Simple uart driver interface
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "simple_uart.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "bmd_log.h"
#include "app_util_platform.h"

static volatile bool rx_enable          = false;
static volatile bool tx_busy            = false;
static simple_uart_rx_callback_t rx_cb  = NULL;
static simple_uart_tx_callback_t tx_cb  = NULL;
static simple_uart_canrx_callback_t can_rx_cb  = NULL;


static inline bool uart_tx_is_busy(void)
{
    return tx_busy;
}

static void uart_tx_interrupt_enable(bool enable)
{
    if(!enable)
    {
        NRF_UART0->INTENCLR = (UART_INTENCLR_TXDRDY_Clear << UART_INTENCLR_TXDRDY_Pos);
    }
    else
    {
        NRF_UART0->INTENSET = (UART_INTENSET_TXDRDY_Set << UART_INTENSET_TXDRDY_Pos);
    }
}


void simple_uart_blocking_put(uint8_t cr)
{
    //protect ongoing tx...
    while(uart_tx_is_busy());
    
    // ok, lets tx the byte...
    uart_tx_interrupt_enable(false);
    
    //turn on tx
    NRF_UART0->TASKS_STARTTX = 1;
    
    NRF_UART0->TXD = (uint8_t)cr;    
    while (NRF_UART0->EVENTS_TXDRDY != 1)
    {
        // Wait for TXD data to be sent.
    }

    //clear flag
    NRF_UART0->EVENTS_TXDRDY = 0;
    
    //turn off tx
    NRF_UART0->TASKS_STOPTX = 1;
}


void simple_uart_blocking_putstring(const uint8_t * str)
{
    uint32_t    i   = 0;
    uint8_t     ch  = str[i++];

    while (ch != '\0')
    {
        simple_uart_blocking_put(ch);
        ch = str[i++];
    }
}

void simple_uart_blocking_put_n(const uint8_t *p_data, uint32_t length)
{
    for(uint32_t i=0; i<length; i++)
    {
        simple_uart_blocking_put(p_data[i]);
    }    
}

void simple_uart_put_nonblocking(uint8_t cr)
{
    while(uart_tx_is_busy());
    
    NRF_UART0->TASKS_STARTTX = 1;
    
    tx_busy = true;
    uart_tx_interrupt_enable(true);
    
    NRF_UART0->TXD = cr;
}

void simple_uart_config(uint8_t rts_pin_number,
                        uint8_t txd_pin_number,
                        uint8_t cts_pin_number,
                        uint8_t rxd_pin_number,
                        bool    hwfc,
                        uint32_t baud_select,
                        uint8_t parity_select)
{
/** @snippet [Configure UART RX and TX pin] */
    NRF_UART0->PSELTXD = txd_pin_number;
    NRF_UART0->PSELRXD = rxd_pin_number;
    
    // flow control?
    if (hwfc)
    {
        nrf_gpio_cfg_output(rts_pin_number);
        nrf_gpio_cfg_input(cts_pin_number, NRF_GPIO_PIN_NOPULL);
        NRF_UART0->PSELCTS = cts_pin_number;
        NRF_UART0->CONFIG |= (UART_CONFIG_HWFC_Enabled << UART_CONFIG_HWFC_Pos);
        
        NRF_UART0->PSELRTS = rts_pin_number;
    }
    
    if(parity_select == 0) 
    {
        NRF_UART0->CONFIG &= ~(UART_CONFIG_PARITY_Included << UART_CONFIG_PARITY_Pos);
    }
    else
    {
        NRF_UART0->CONFIG  |= (UART_CONFIG_PARITY_Included << UART_CONFIG_PARITY_Pos);
    }

    NRF_UART0->BAUDRATE      = (baud_select << UART_BAUDRATE_BAUDRATE_Pos);
    NRF_UART0->ENABLE        = (UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos);
    NRF_UART0->TASKS_STARTRX = 1;
    NRF_UART0->EVENTS_CTS = 0;
    
    // configure the rx/tx pins after enabling the UART to avoid glitching
    nrf_gpio_cfg_output(txd_pin_number);
    nrf_gpio_cfg_input(rxd_pin_number, NRF_GPIO_PIN_NOPULL);
    
    while(NRF_UART0->EVENTS_RXDRDY != 0)
    {
        (void)NRF_UART0->RXD;
        NRF_UART0->EVENTS_RXDRDY = 0;
    }
    
    rx_cb       = NULL;
    tx_cb       = NULL;
    can_rx_cb   = NULL;
    
    rx_enable   = true;
    
    // stop tx
    NRF_UART0->TASKS_STOPTX = 1;
    tx_busy = false;
    
    // setup interrupt for rx/error
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Enabled << UART_INTENSET_RXDRDY_Pos;
    NRF_UART0->INTENSET = UART_INTENSET_ERROR_Enabled << UART_INTENCLR_ERROR_Pos;
    
    // enable UART interrupt
    NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_HIGH);
	NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_EnableIRQ(UART0_IRQn);
}

void simple_uart_disable( void )
{   
    NRF_UART0->TASKS_STOPRX = 1;
    NRF_UART0->TASKS_STOPTX = 1;
    tx_busy = false;
    rx_enable = false;
    
    NVIC_DisableIRQ(UART0_IRQn);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    
    //clear all interrupt sources
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->ENABLE = (UART_ENABLE_ENABLE_Disabled << UART_ENABLE_ENABLE_Pos);
    
    rx_cb = NULL;
    tx_cb = NULL;
}

void simple_uart_set_rx_callback(simple_uart_rx_callback_t cb)
{
    rx_cb = cb;
}

void simple_uart_set_tx_callback(simple_uart_tx_callback_t cb)
{
    tx_cb = cb;
}

void simple_uart_set_canrx_callback(simple_uart_canrx_callback_t cb)
{
    can_rx_cb = cb;
}

void simple_uart_disable_rx( void )
{
    rx_enable = false;
    
    //disable the rx interrupt
    NRF_UART0->INTENCLR = UART_INTENCLR_RXDRDY_Msk;
}

void simple_uart_enable_rx( void )
{
    rx_enable = true;
    
    //enable rx interrupt
    NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Msk;
    
    //set pending if we have data
    if(NRF_UART0->EVENTS_RXDRDY != 0)
    {
        NVIC_SetPendingIRQ(UART0_IRQn);
    }
}

bool simple_uart_get_rx_enable( void )
{
    return rx_enable;
}

void UART0_IRQHandler(void)
{
    if(NRF_UART0->EVENTS_ERROR)
    {
        uint32_t err_src = NRF_UART0->ERRORSRC;
        
        // clear error
        NRF_UART0->ERRORSRC     = err_src;
        NRF_UART0->EVENTS_ERROR = 0;
        
        bmd_log("uart0_error: src 0x%x\n", err_src);
        
        //throw away the byte
        if(err_src & (UART_ERRORSRC_PARITY_Msk|UART_ERRORSRC_FRAMING_Msk)
            && NRF_UART0->EVENTS_RXDRDY)
        {
            NRF_UART0->EVENTS_RXDRDY = 0;
            
            uint8_t byte = NRF_UART0->RXD;
            bmd_log("uart0_err_data: %02x\n", byte);
            (void)byte;
        }
        
        #ifdef UART_ASSERT_ON_ERROR
            APP_ERROR_CHECK_BOOL(false);
        #endif
    }
        
    //dont read the byte if we cant put it to rx buffer
    if(NRF_UART0->EVENTS_RXDRDY && can_rx_cb())
    {  
        NRF_UART0->EVENTS_RXDRDY = 0;
        uint8_t byte = NRF_UART0->RXD;
        
        if(rx_cb)
        {
            rx_cb(byte);
        }
    }
    
    // are we trying to process tx data in the interrupt?
    if(NRF_UART0->EVENTS_TXDRDY 
        && (NRF_UART0->INTENSET & (UART_INTENSET_TXDRDY_Set << UART_INTENSET_TXDRDY_Pos)) )
    {
        NRF_UART0->EVENTS_TXDRDY = 0;
        tx_busy = false;
        
        if(tx_cb)
        {
            tx_cb();
        }
    }
}

