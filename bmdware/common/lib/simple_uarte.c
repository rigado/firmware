/** @file simple_uarte.c
*
* @brief This module configures and uses the DMA enhanced UART peripheral.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
 

#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_uarte.h"

#include "app_util_platform.h"

#include "simple_uarte.h"

//Set up for an event for every byte rx'd
#define DMA_RX_MAX_COUNT    1

/* Callback for RX'd bytes */
static void (*rx_ready_callback)(const uint8_t * const rx_buffer, uint8_t count);

/* Called when TX is complete, set via put function */
static void (*tx_complete_callback)(void);

static uint8_t rx_buf[DMA_BUFFER_SIZE];
static uint8_t tx_buf[DMA_BUFFER_SIZE];

static uint8_t rts_pin;

void simple_uarte_config(uint8_t rts_pin_number,
                        uint8_t txd_pin_number,
                        uint8_t cts_pin_number,
                        uint8_t rxd_pin_number,
                        bool    hwfc,
                        uint32_t baud_select, //TODO: set this up as the correct type
                        uint8_t parity_select)
{
/** @snippet [Configure UART RX and TX pin] */
    nrf_gpio_cfg_output(txd_pin_number);
    nrf_gpio_cfg_input(rxd_pin_number, NRF_GPIO_PIN_NOPULL);

    memset(rx_buf, 0, sizeof(rx_buf));
    memset(tx_buf, 0, sizeof(tx_buf));
    
    nrf_uarte_txrx_pins_set(NRF_UARTE0, txd_pin_number, rxd_pin_number);
    
    nrf_uarte_parity_t parity = NRF_UARTE_PARITY_EXCLUDED;
    nrf_uarte_hwfc_t flow_ctrl = NRF_UARTE_HWFC_DISABLED;
    if (hwfc)
    {
        rts_pin = rts_pin_number;
        nrf_gpio_cfg_output(rts_pin_number);
        nrf_gpio_pin_clear(rts_pin);
        nrf_gpio_cfg_input(cts_pin_number, NRF_GPIO_PIN_NOPULL);
        nrf_uarte_hwfc_pins_set(NRF_UARTE0, 0xFF, cts_pin_number);
        flow_ctrl = NRF_UARTE_HWFC_ENABLED;
    }
    if(parity_select == 1) 
    {
        parity = NRF_UARTE_PARITY_INCLUDED;
    }
    nrf_uarte_configure(NRF_UARTE0, parity, flow_ctrl);
    
    nrf_uarte_rx_buffer_set(NRF_UARTE0, rx_buf, DMA_RX_MAX_COUNT);
    
    
    tx_complete_callback = NULL;
    rx_ready_callback = NULL;    
    
    //TODO: use the appropriate function
    NRF_UARTE0->BAUDRATE = (baud_select << UART_BAUDRATE_BAUDRATE_Pos);
    
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_CTS);
}

void simple_uarte_enable(void (*rx_callback)(const uint8_t * const data, uint8_t len))
{
    rx_ready_callback = rx_callback;
    nrf_uarte_int_enable(NRF_UARTE0, (NRF_UARTE_INT_ENDRX_MASK | NRF_UARTE_INT_RXSTARTED_MASK | NRF_UARTE_INT_ERROR_MASK));
    nrf_uarte_shorts_enable(NRF_UARTE0, NRF_UARTE_SHORT_ENDRX_STARTRX);
    nrf_uarte_enable(NRF_UARTE0);
    
    NVIC_EnableIRQ(UARTE0_UART0_IRQn);
    NVIC_SetPriority(UARTE0_UART0_IRQn, APP_IRQ_PRIORITY_HIGH);
    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STARTRX);
}

void simple_uarte_put(const uint8_t * data, uint8_t size, void (*callback)(void))
{
    tx_complete_callback = callback;
    memcpy(tx_buf, data, size);
    
    nrf_uarte_tx_buffer_set(NRF_UARTE0, tx_buf, size);
    nrf_uarte_int_enable(NRF_UARTE0, NRF_UARTE_INT_ENDTX_MASK);
    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STARTTX);
}

void simple_uarte_putstring(const uint8_t * str, void (*callback)(void))
{
    uint_fast8_t i  = 0;
    uint8_t      ch = str[i++];

    uint8_t len = strlen((const char*)str);
    simple_uarte_put(str, len, callback);
}

void simple_uarte_disable( void )
{
    NVIC_DisableIRQ(UARTE0_UART0_IRQn);
    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STOPRX);
    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STOPTX);
    nrf_uarte_disable(NRF_UARTE0);
    nrf_uarte_int_disable(NRF_UARTE0, (NRF_UARTE_INT_ENDRX_MASK | NRF_UARTE_INT_ENDTX_MASK | NRF_UARTE_INT_ERROR_MASK | NRF_UARTE_INT_RXSTARTED_MASK));
    rx_ready_callback = NULL;
    NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
}

void simple_uarte_disable_rx( void )
{
    nrf_gpio_pin_set(rts_pin);
//    nrf_uarte_shorts_disable(NRF_UARTE0, NRF_UARTE_SHORT_ENDRX_STARTRX);
//    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STOPRX);
//    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_FLUSHRX);
}

void simple_uarte_enable_rx( void )
{
    nrf_gpio_pin_clear(rts_pin);
//    nrf_uarte_shorts_enable(NRF_UARTE0, NRF_UARTE_SHORT_ENDRX_STARTRX);
//    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STARTRX);
}

void UARTE0_UART0_IRQHandler(void)
{
    if(nrf_uarte_event_check(NRF_UARTE0, NRF_UARTE_EVENT_ENDTX))
    {
        nrf_uarte_int_disable(NRF_UARTE0, NRF_UARTE_INT_ENDTX_MASK);
        nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ENDTX);
        nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STOPTX);
        
        if(tx_complete_callback)
        {
            tx_complete_callback();
        }
    }
    
    if(nrf_uarte_event_check(NRF_UARTE0, NRF_UARTE_EVENT_RXTO))
    {
        nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_RXTO);
        nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_FLUSHRX);
        
        if(rx_ready_callback)
        {
            rx_ready_callback(rx_buf, NRF_UARTE0->RXD.AMOUNT);
        }
    }
    
    if(nrf_uarte_event_check(NRF_UARTE0, NRF_UARTE_EVENT_ENDRX))
    {
        nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ENDRX);
        if(rx_ready_callback)
        {
            rx_ready_callback(rx_buf, NRF_UARTE0->RXD.AMOUNT);
        }
    }
    
    if(nrf_uarte_event_check(NRF_UARTE0, NRF_UARTE_EVENT_RXSTARTED))
    {
        nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_RXSTARTED);
    }
    
    if(nrf_uarte_event_check(NRF_UARTE0, NRF_UARTE_EVENT_ERROR))
    {
        nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ERROR);
        volatile uint32_t err_src;
        err_src = nrf_uarte_errorsrc_get_and_clear(NRF_UARTE0);
        
    }
}
