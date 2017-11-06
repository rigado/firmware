/** @file uart.c
*
* @brief This module handles interfacing with the UART peripheral for
*        all UART communications (DTM, Passthrough, and AT)
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include "nrf_soc.h"
#include "app_util_platform.h"

//use the nRF52 UARTE?
//#define NRF52_UARTE

//assert on error?
//#define UART_ASSERT_ON_ERROR

#ifdef NRF52_UARTE
    #include "simple_uarte.h"
#else
    #include "simple_uart.h"
#endif

#include "storage_intf.h"
#include "ble_nus.h"
#include "nrf_gpio.h"
#include "timer.h"
#include "at_commands.h" 
#include "ringbuf.h"
#include "storage_intf.h"
#include "nrf_delay.h"
#include "gatt.h"
#include "uart.h"
#include "bmd_log.h"

#ifdef NRF52
    #include "bmd_dtm.h"
    #ifdef NRF52_UARTE
        #define UART NRF_UARTE0
    #else
        #define UART NRF_UART0
    #endif
    #define GATT_MTU_SIZE GATT_EXTENDED_MTU_SIZE
#else
    #include "bmd200_dtm.h"
    #define UART NRF_UART0
    #define GATT_MTU_SIZE GATT_MTU_SIZE_DEFAULT
#endif

#define UART_MAX_AT_LEN         (MAX_AT_COMMAND_LEN + MAX_AT_DATA_LEN)
#define UART_RX_BUF_SIZE        (4096)
#define MAX_UART_BLE_DATA       (GATT_MTU_SIZE - 3)

static bool         m_hwfc = false;
static ble_nus_t * 	mp_uart_service;
static uart_mode_t 	m_mode = UART_MODE_INACTIVE;
static bool         m_should_send;

// rx data
static uint8_t 		data_array_rx[UART_RX_BUF_SIZE];/* This array is used by the ring buffer. Do NOT locally modify! */
static ringBuf_t 	data_ring_buf_rx;
static volatile bool triggered_stop_rx = false;

// tx data
static uint8_t      data_array_tx[UART_TX_BUFFER_SIZE];
static ringBuf_t    data_ring_buf_tx;
static bool         is_tx_in_progress;

#ifdef NRF52_UARTE
    static uint8_t  dma_tx_buffer[DMA_BUFFER_SIZE];
#endif

// ble tx buffer
static uint8_t tx_data_buffer[GATT_MTU_SIZE];

static void config_uart(uint8_t rts_pin_number,
                            uint8_t txd_pin_number,
                            uint8_t cts_pin_number,
                            uint8_t rxd_pin_number,
                            bool    hwfc,
                            uint32_t baudrate_number,
                            uint8_t parity_select);
                        
static uint32_t     get_baud_bitfield_from_rate(uint32_t rate);
static bool         uart_irq_can_rx(void);
static inline void  uart_irq_proc_data(uint8_t data);
static void         uart_rx_ringbuf_event_callback(ringBuf_t *ringBuf, ringBufEvent_t event);
#ifdef NRF52_UARTE
    static void         uarte_rx_callback(const uint8_t * const p_data, uint8_t len);
#endif

void uart_configure_at_mode()
{
    config_uart( BMD_UART_RTS, 
                BMD_UART_TXD, 
                BMD_UART_CTS, 
                BMD_UART_RXD, 
                false, 
                UART_BAUDRATE_BAUDRATE_Baud57600, 
                false);
                    
    m_mode = UART_MODE_BMDWARE_AT;
}

void uart_disable_at_mode()
{
    ringBufClear(&data_ring_buf_tx);
    ringBufClear(&data_ring_buf_rx);
    m_mode = UART_MODE_INACTIVE;
}

void uart_configure_direct_test_mode(void)
{
    //enable UART
    config_uart(0,
                BMD_DTM_UART_TXD, 
                0, 
                BMD_DTM_UART_RXD,
                BMD_DTM_UART_HWFC,
                BMD_DTM_UART_BAUD,
                BMD_DTM_UART_PARITY);
    
	//set mode
	m_mode = UART_MODE_DTM;
}


void uart_configure_passthrough_mode(ble_nus_t * p_uart_service)
{    
    // enable the UART
    config_uart(BMD_UART_RTS, 
                BMD_UART_TXD, 
                BMD_UART_CTS, 
                BMD_UART_RXD, 
                p_uart_service->flow_control, 
                p_uart_service->baud_rate, 
                p_uart_service->parity);
   
    // set UART mode
    m_mode = UART_MODE_BMDWARE_PT;
    
    //register the uart tx buffer callbacks
    ble_nus_register_uart_callbacks();
    
    mp_uart_service = p_uart_service;
    m_should_send = false;	
    timer_stop_uart();
}

void uart_disable_passthrough_mode(void)
{
    ringBufClear(&data_ring_buf_tx);
    ringBufClear(&data_ring_buf_rx);
    m_mode = UART_MODE_INACTIVE;
}

uart_mode_t uart_get_mode(void)
{
	return m_mode;
}


void uart_deinit(void)
{
	timer_stop_uart();
	
	if (1)  // XXX Don't turn off UART if using AT mode
	{
#ifdef NRF52_UARTE
        simple_uarte_disable();
#else
        simple_uart_disable();
#endif
        /* release gpios */
        nrf_gpio_cfg_default(BMD_UART_RXD); 
        nrf_gpio_cfg_default(BMD_UART_TXD);
	}
}

void uart_reg_tx_buf_event_callback(ringBufEvent_t event, 
        ringBufEventCallback_t callback)
{
    uint32_t result = ringBufRegisterEventCallback(&data_ring_buf_tx, event, callback);
    
    bmd_log("tx_cb_reg: %d evt, %08x cb, %d success\n", event, callback, result);
    APP_ERROR_CHECK_BOOL(result == RINGBUF_SUCCESS);
}


void uart_clear_buffer(void)
{
    ringBufClear(&data_ring_buf_rx);
}

static uint32_t ble_tx_count = 0;
void uart_transfer_data(void)
{
    if(m_mode == UART_MODE_INACTIVE)
    {
        return;
    }
    
    if( m_mode == UART_MODE_DTM )
    {
        uint8_t byte;
        while( ringBufReadOne(&data_ring_buf_rx, &byte) == RINGBUF_SUCCESS )
        {
            bmd_dtm_proc_rx(byte, false);
        }
    }
    else if( m_mode == UART_MODE_BMDWARE_PT && m_should_send )
    {	
        // Data needs to be sent if there are at least runtime MTU bytes in the buffer 
        // or more than 50 ms have passed since the last byte was received.
        if(m_should_send) 
        {
            m_should_send = false;
            
            uint32_t err_code;
            memset(tx_data_buffer, 0, sizeof(tx_data_buffer));
            uint32_t total_len = ringBufWaiting(&data_ring_buf_rx);
            uint32_t len;

            while(total_len)
            {
                uint8_t rb_error;
                
                len = total_len;
                uint16_t runtime_mtu = gatt_get_runtime_mtu();
                if(len > runtime_mtu)
                {
                    len = runtime_mtu;
                }
                else
                {
                    timer_stop_uart();
                }
                rb_error = ringBufPeek(&data_ring_buf_rx, tx_data_buffer, len);
                
                if(rb_error != RINGBUF_SUCCESS)
                {
                    bmd_log("ringbuf peek error\n");
                }
                
                err_code = ble_nus_send_string(mp_uart_service, tx_data_buffer, len);
                
                if (err_code == NRF_SUCCESS)
                {
                    ringBufDiscard(&data_ring_buf_rx, len);
                    ble_tx_count += len;
                    
                    bmd_log("ble_tx'd %d, total %d, waiting %d\n", 
                            len, 
                            ble_tx_count,
                            ringBufWaiting(&data_ring_buf_rx));
                }
                else
                {
                    // busy, try again later
                    timer_start_uart();
                    break;
                }
                
                //more to tx?
                total_len = ringBufWaiting(&data_ring_buf_rx);
                
                if(total_len < runtime_mtu)
                {
                   timer_start_uart();
                   break;
                }
            }
        }
    }    
}

#ifdef NRF52_UARTE
static void uarte_tx_complete_callback(void)
{
    uint32_t waiting = ringBufWaiting(&data_ring_buf_tx);
    if(waiting != 0)
    {
        is_tx_in_progress = true;
        uint8_t to_read = (waiting >= DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : waiting;
        ringBufRead(&data_ring_buf_tx, dma_tx_buffer, to_read);
        simple_uarte_put(dma_tx_buffer, to_read, uarte_tx_complete_callback);
    }
    else
    {
        is_tx_in_progress = false;
    }
}
#else
static void uart_tx_complete_callback(void)
{
    uint32_t waiting = ringBufWaiting(&data_ring_buf_tx);
    
    if(waiting != 0)
    {
        uint8_t tx_byte;
        
        is_tx_in_progress = true;
        
        ringBufReadOne(&data_ring_buf_tx, &tx_byte);
        simple_uart_put_nonblocking(tx_byte);
        //bmd_log("uart tx %02x\n", tx_byte);
    }
    else
    {
        is_tx_in_progress = false;
        bmd_log("txcb done\n", waiting);
    }
}
#endif

void uart_set_rx_enable_state(bool state)
{
    #ifdef NRF52_UARTE
        if(state)
        {
            simple_uarte_enable_rx();
        }
        else
        {
            simple_uarte_disable_rx();
        }
    #else
    
    if(state)
    {
        simple_uart_enable_rx();
    }
    else
    {
        simple_uart_disable_rx();
    }
    #endif
}

/**@brief    Function for handling the data from the UART Service.
 *
 * @details  This function will process the data received from the 
 *           UART BLE Service and send it to the UART peripheral.
 */
void uart_ble_data_handler(ble_nus_t * p_uart_service, uint8_t * p_data, uint16_t length)
{
    if(m_mode != UART_MODE_BMDWARE_PT)
    {
        return;
    }
    
    if(length == 0 || p_data == NULL)
    {
        bmd_log("uart_ble_data_handler: illegal params len %d, data 0x%08x\n", length, p_data);
        return;
    }

#ifdef NRF52_UARTE
    //todo: check if flow control is enabled, if not, this step isn't necessary
    if(!is_tx_in_progress)
    {
        is_tx_in_progress = true;
        simple_uarte_put(p_data, (uint8_t)length, uarte_tx_complete_callback);
    }
    else
    {
        (void)ringBufWrite(&data_ring_buf_tx, p_data, length);
    }
#else
    uint8_t result;
    
    //queue to ringbuffer
    result = ringBufWrite(&data_ring_buf_tx, p_data, length);    
    uint32_t waiting = ringBufWaiting(&data_ring_buf_tx);
    
    if(result == RINGBUF_ERROR)
    {
        bmd_log("data_ring_buf_tx write err: %d/%d\n", waiting, ringBufTotalCapacity(&data_ring_buf_tx));
    }
    
    
    for(int i=0; i<length; i++)
    {
        bmd_log("%02x",p_data[i]);
    }
    bmd_log("\n");
    
    //put the byte, if we arent working already
    if(!is_tx_in_progress && waiting)
    {
        uint8_t tx_byte;
        is_tx_in_progress = true;
        
        ringBufReadOne(&data_ring_buf_tx, &tx_byte);
        simple_uart_put_nonblocking(tx_byte);
    }
#endif
}

uint32_t uart_get_tx_buffer_waiting(void)
{
    return ringBufWaiting(&data_ring_buf_tx);
}

void uart_ble_timeout_handler(void * p_context)
{
    m_should_send = true;
}

static void config_uart(uint8_t rts_pin_number,
                        uint8_t txd_pin_number,
                        uint8_t cts_pin_number,
                        uint8_t rxd_pin_number,
                        bool    hwfc,
                        uint32_t baudrate_number,
                        uint8_t parity_select)
{
    
    uint32_t baud_bitval = get_baud_bitfield_from_rate(baudrate_number);
    
    // decode error?
    if(baud_bitval == 0)
    {
        baud_bitval = UART_BAUDRATE_BAUDRATE_Baud57600;
    }  
    
    // quick teardown, stop the timeout and disable interrupts
    timer_stop_uart();
#ifdef NRF52_UARTE
    NVIC_DisableIRQ(UARTE0_UART0_IRQn);
#else
    NVIC_DisableIRQ(UART0_IRQn);
#endif
    
    // setup new config
#ifdef NRF52_UARTE
    simple_uarte_config(rts_pin_number, 
                        txd_pin_number, 
                        cts_pin_number, 
                        rxd_pin_number, 
                        hwfc, 
                        baud_bitval, 
                        parity_select);
    
    simple_uarte_enable(uarte_rx_callback);
    memset(dma_tx_buffer, 0, sizeof(dma_tx_buffer));
#else
    simple_uart_config( rts_pin_number, 
                        txd_pin_number, 
                        cts_pin_number, 
                        rxd_pin_number, 
                        hwfc, 
                        baud_bitval, 
                        parity_select);
		
    m_hwfc = hwfc;
    is_tx_in_progress = false;
    
    //set rx callback
    simple_uart_set_rx_callback(uart_irq_proc_data);
    simple_uart_set_tx_callback(uart_tx_complete_callback);
    simple_uart_set_canrx_callback(uart_irq_can_rx);
#endif
    
    // reinit ringbuffers
    ringBufInit(&data_ring_buf_rx, sizeof(data_array_rx[0]), sizeof(data_array_rx), data_array_rx);
    ringBufInit(&data_ring_buf_tx, sizeof(data_array_tx[0]), sizeof(data_array_tx), data_array_tx);
    
    ringBufRegisterEventCallback(&data_ring_buf_rx, RINGBUF_EVENT_ALMOST_FULL, uart_rx_ringbuf_event_callback);
    ringBufRegisterEventCallback(&data_ring_buf_rx, RINGBUF_EVENT_FULL, uart_rx_ringbuf_event_callback);
    ringBufRegisterEventCallback(&data_ring_buf_rx, RINGBUF_EVENT_EMPTY, uart_rx_ringbuf_event_callback);
}

static uint32_t get_baud_bitfield_from_rate(uint32_t rate)
{
    switch(rate)
    {
    case 1200:
        return UART_BAUDRATE_BAUDRATE_Baud1200;
    case 2400:
        return UART_BAUDRATE_BAUDRATE_Baud2400;
    case 4800:
        return UART_BAUDRATE_BAUDRATE_Baud4800;
    case 9600:
        return UART_BAUDRATE_BAUDRATE_Baud9600;
    case 14400:
        return UART_BAUDRATE_BAUDRATE_Baud14400;
    case 19200:
        return UART_BAUDRATE_BAUDRATE_Baud19200;
    case 28800:
        return UART_BAUDRATE_BAUDRATE_Baud28800;
    case 38400:
        return UART_BAUDRATE_BAUDRATE_Baud38400;
    case 57600:
        return UART_BAUDRATE_BAUDRATE_Baud57600;
    case 76800:
        return UART_BAUDRATE_BAUDRATE_Baud76800;
    case 115200:
        return UART_BAUDRATE_BAUDRATE_Baud115200;
    case 230400:
        return UART_BAUDRATE_BAUDRATE_Baud230400;
    case 250000:
        return UART_BAUDRATE_BAUDRATE_Baud250000;
    case 460800:
        return UART_BAUDRATE_BAUDRATE_Baud460800;
    case 921600:
        return UART_BAUDRATE_BAUDRATE_Baud921600;
    case 1000000:
        return UART_BAUDRATE_BAUDRATE_Baud1M;
    default:
        return 0;
    }
}

static uint32_t rx_count = 0;


static bool uart_irq_can_rx(void)
{
    bool result = simple_uart_get_rx_enable();
    
    if(result)
    {
        //double check that we have space in rx buffer
        uint32_t free = ringBufUnused(&data_ring_buf_rx);
        result = (free != 0);
    }
    
    return result;
}

static inline void uart_irq_proc_data(uint8_t data)
{
    #ifdef UART_ASSERT_ON_ERROR
        if((rx_count & 0xff) != data)
        {
            APP_ERROR_CHECK_BOOL(false);
        }
    #endif
        
    rx_count++;
    
    if((rx_count % 0x100) == 0)
    {
        bmd_log("rx_count %d\n", rx_count);
    }
    
    /* queue the byte to the rx ringbuffer */
    if(m_mode == UART_MODE_BMDWARE_AT)
    {
        /* in at mode, skip newlines */
        if(data != '\r' && data != '\n')
        {
            (void)ringBufWriteOne(&data_ring_buf_rx, &data);
        }
    }
    else
    {
        /* queue all data for other modes */
        uint8_t result = ringBufWriteOne(&data_ring_buf_rx, &data);
        
        if(result != RINGBUF_SUCCESS)
        {
            uint32_t waiting = ringBufWaiting(&data_ring_buf_rx);
            uint32_t total_cap = ringBufTotalCapacity(&data_ring_buf_rx);
            bmd_log("uart_rx: ringbuf overrun, %u/%u\n", waiting, total_cap);
            
            #ifdef UART_ASSERT_ON_ERROR
                APP_ERROR_CHECK_BOOL(result == RINGBUF_SUCCESS);
            #endif
        }
    }

    
    /* process the rx'd data now? */
    uint32_t waiting = ringBufWaiting(&data_ring_buf_rx);
    
    // passthrough mode
    if(m_mode == UART_MODE_BMDWARE_PT)
    {
        uint16_t runtime_mtu = gatt_get_runtime_mtu();
        if(waiting >= runtime_mtu)
        {
            m_should_send = true;
        }
        else
        {
            timer_start_uart();
        }
    }
    // at mode
    else if(m_mode == UART_MODE_BMDWARE_AT)
    {
        // wait for a newline or the buffer to fill to process a command
        if( (data == '\n' || data == '\r') 
            || (waiting == (UART_MAX_AT_LEN - 1)) )
        {
            at_proc_set_cmd_ready(&data_ring_buf_rx, waiting);
        }
        else
        {
            //wait for a complete command...
        }
    }
    // dtm mode
    else if(m_mode == UART_MODE_DTM)
    {
        //do nothing, these commands are handled in the main loop
    }  
}

#ifdef NRF52_UARTE
/**@brief   Function for handling UART interrupts.
 *
 * @details This function will receive a single character from the UART and
 *          place it in the appropriate ring buffer based on the current
 *          mode.  In AT and Pass-through modes, it will also setup the next
 *          action if additional handling of the data is needed.
 */
static void uarte_rx_callback(const uint8_t * const p_data, uint8_t len)
{
    if( m_mode != UART_MODE_INACTIVE )
    {
        uint8_t byte; 
        
        for(uint8_t i = 0; i < len; i++)
        {
            byte = p_data[i];
            uart_irq_proc_data(byte);
        }
    }
}
#endif


static void uart_rx_ringbuf_event_callback(ringBuf_t *ringBuf, ringBufEvent_t event)
{    
    if(!m_hwfc)
    {
        return;
    }
    
    switch(event)
    {
        case RINGBUF_EVENT_ALMOST_FULL:
            bmd_log("uart_rx: almost full\n");
            uart_set_rx_enable_state(false);
            break;
        case RINGBUF_EVENT_FULL:
            bmd_log("uart_rx: full\n");
            uart_set_rx_enable_state(false);
            break;
        case RINGBUF_EVENT_EMPTY:
            bmd_log("uart_rx: empty\n");
            uart_set_rx_enable_state(true);
            uart_force_pt_tx();
            break;
        default:
            break;
    }
}

void uart_force_pt_tx(void)
{
    m_should_send = true;
}

void uart_reset_counters(void)
{
    rx_count = 0;
    ble_tx_count = 0;
}
