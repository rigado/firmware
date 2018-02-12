#include <nrf.h>
#include <nrf_gpio.h>
#include <stdlib.h>

#include "boards.h"
#include "rigdfu_serial.h"

#include "simple_uart.h"
#include "app_util_platform.h"
#include "app_scheduler.h"

/* This serial driver is used for two things:
   (1) initial detection of whether to enter DFU mode
       (no pins are driven until it is clear that DFU is happening)
   (2) the real DFU, with normal settings
*/
static bool m_open = false;
static rigdfu_serial_rx_handler_t m_uart_rx_handler = NULL;
static volatile bool m_tx_pending = false;

void UART0_IRQHandler(void)
{
    if(NRF_UART0->EVENTS_RXDRDY == 1)
    {
        if(m_uart_rx_handler != NULL)
            m_uart_rx_handler(NRF_UART0->RXD);
        else
            (void)NRF_UART0->RXD;
        
        NRF_UART0->EVENTS_RXDRDY = 0;
    }
    
    if(NRF_UART0->EVENTS_ERROR == 1)
    {
        NRF_UART0->EVENTS_ERROR = 0;
        
        //clear the error...
    }
    
    NVIC_ClearPendingIRQ(UART0_IRQn);
}


/* Initialize UART for the initial host detection. */
void rigdfu_serial_init(bool force_init)
{
    m_open = true;
    m_tx_pending = false;
    m_uart_rx_handler = NULL;

#ifdef EVAL_BOARD_BUILD
    nrf_gpio_cfg_input(RTS_PIN_NUMBER, NRF_GPIO_PIN_PULLDOWN);
#endif
    
    if(!force_init)
    {
        //configure rx pin as input with pulldown and check to see if it is high
        //if it is high, then a uart connection is likely present, continue with
        //rx only init
        nrf_gpio_cfg_input(RX_PIN_NUMBER, NRF_GPIO_PIN_PULLDOWN);
        uint32_t rx_pin = nrf_gpio_pin_read(RX_PIN_NUMBER);
        if(!rx_pin)
        {
            NRF_GPIO->PIN_CNF[RX_PIN_NUMBER] = (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
            return;
        }
    }
    
    //config the uart for rx only
    simple_uart_config_t config;
    config.rx_pin   = RX_PIN_NUMBER;
    config.tx_pin   = 0xff;   
    config.cts_pin  = 0xff;
    config.rts_pin  = 0xff;
    config.hwfc     = false;
    config.baud_enum = UART_BAUDRATE_BAUDRATE_Baud115200;
    simple_uart_init(&config);
    
    //setup rx interrupt
    NRF_UART0->INTENSET = (UART_INTENSET_RXDRDY_Set << UART_INTENSET_RXDRDY_Pos);
    NRF_UART0->INTENSET = (UART_INTENSET_ERROR_Set << UART_INTENSET_ERROR_Pos);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_SetPriority(UART0_IRQn,APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(UART0_IRQn);
    
    //tx disconnected, just pull it
    nrf_gpio_cfg_input(TX_PIN_NUMBER, NRF_GPIO_PIN_PULLUP);
}

/* Re-initialize UART for the actual DFU, which may have different
   settings (flow control) */
void rigdfu_serial_reinit_dfu(void)
{
    if (m_open) {
        /* Wait for outstanding characters to be sent */
        while (m_tx_pending)
            continue;

        /* Close serial port */
        m_uart_rx_handler = NULL;
        m_open = false;

        rigdfu_serial_close();
    }

    m_open = true;
    m_tx_pending = false;
    m_uart_rx_handler = NULL;

    simple_uart_config_t config;
    config.rx_pin   = RX_PIN_NUMBER;
    config.tx_pin   = TX_PIN_NUMBER;
    config.cts_pin  = 0xff;
    config.rts_pin  = 0xff;
    config.hwfc     = false;
    config.baud_enum = UART_BAUDRATE_BAUDRATE_Baud115200;
    simple_uart_init(&config);
    
    //setup interrupt
    NRF_UART0->INTENSET = (UART_INTENSET_RXDRDY_Set << UART_INTENSET_RXDRDY_Pos);
    NRF_UART0->INTENSET = (UART_INTENSET_ERROR_Set << UART_INTENSET_ERROR_Pos);
    
    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_HIGH);
    NVIC_EnableIRQ(UART0_IRQn);
    
    //appease the jlink?
    //This isn't really a good idea to be doing in the general bootloader
    //nrf_gpio_cfg_input(RTS_PIN_NUMBER, NRF_GPIO_PIN_PULLDOWN);
    //nrf_gpio_cfg_input(CTS_PIN_NUMBER, NRF_GPIO_PIN_PULLDOWN);
    
    #ifdef EVAL_BOARD_BUILD
        nrf_gpio_cfg_input(RTS_PIN_NUMBER, NRF_GPIO_PIN_PULLDOWN);
    #endif
}

/* Fully close UART. */
void rigdfu_serial_close(void)
{
    if (!m_open)
        return;

    /* Wait for outstanding characters to be sent */
    while (m_tx_pending)
        continue;
    
    //clear IRQs
    NVIC_DisableIRQ(UART0_IRQn);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    
    //deinit peripheral/gpio
    simple_uart_deinit();
    
    //cleanup handles
    m_uart_rx_handler = NULL;
    m_open = false;
}

/* Set the UART RX handler */
void rigdfu_serial_set_rx_handler(rigdfu_serial_rx_handler_t handler)
{
    m_uart_rx_handler = handler;
}

/* Send byte to the UART */
void rigdfu_serial_put(uint8_t x)
{    
    simple_uart_put(x);
}

/* Send string */
void rigdfu_serial_puts(const char *s)
{
    while (*s)
        rigdfu_serial_put(*s++);
}

