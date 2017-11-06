#include <string.h>
#include <nrf51.h>

#include "bmd200_dtm.h"
#include "ble_dtm.h"
#include "simple_uart.h"
#include "timer.h"
#include "softdevice_handler.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "bmd200_selftest.h"
#include "rigdfu.h"
#include "version.h"

/* define error handler here */
#define Error_Handler(...)

/* define initialization functions here */
#define UART_Tx(pData,ucLen)         uartTX(pData,ucLen)

/* byte to byte timeous 5ms from BT spec */
#define BYTE_TIMEOUT_MS                 5

typedef struct
{
    bool        is_msb_read;        // True when MSB of the DTM command has been read and the application is waiting for LSB.
    uint16_t    dtm_cmd_from_uart;  // Packed command containing command_code:freqency:length:payload in 2:6:6:2 bits.
    bool        found_at_command;
} dtm_ctxt_t;


static dtm_ctxt_t s_dtm_ctxt = {0};

static void     resetCtxt(void);
static void     uartTX(const uint8_t * const data, uint8_t len);
static uint32_t putDTMCommand(uint16_t command);
static uint32_t swap_le_to_be(uint32_t input);

void dtm_timeout_handler(void * p_context)
{
    bmd_dtm_proc_rx(0x00,true);
}

void bmd_dtm_init(void)
{
    //reset command parser state
    memset(&s_dtm_ctxt, 0, sizeof(s_dtm_ctxt));
    
    //allocate timer
    timer_create_uart( dtm_timeout_handler, BYTE_TIMEOUT_MS, false );
    timer_start_uart();
    
    softdevice_handler_sd_disable();
    
    //init the dtm state machine
    if( dtm_init() != DTM_SUCCESS )
        Error_Handler();

    (void)dtm_reset();

    if( dtm_set_txpower( 4 ) != true )
        Error_Handler();
}    

void bmd_dtm_proc_rx(uint8_t byte, bool init)
{
    if(init)
    {
        resetCtxt();
        return;
    }

    if(!s_dtm_ctxt.is_msb_read)
    {
        // This is first byte of two-byte command.
        s_dtm_ctxt.is_msb_read                 = true;
        s_dtm_ctxt.dtm_cmd_from_uart     = ((dtm_cmd_t)byte) << 8;

        //wait for 2nd byte of command word.
        timer_start_uart();
        return;
    }
    else
    {
        //stop timer
        timer_stop_uart();
        
        //2-byte UART command received.
        s_dtm_ctxt.is_msb_read        = false;
        s_dtm_ctxt.dtm_cmd_from_uart |= (dtm_cmd_t)byte;
    }

    dtm_event_t  dtm_result;                          // Result of a DTM operation.
    if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_SELFTEST_CMD )
    {
      uint32_t result = bmd200_selftest();
      //UART0_Init();
      UART_Tx( (uint8_t*)&result, sizeof(result));
      
      //GPIO_Init();
    }
    else if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_GETMAC_CMD )
    {
        uint8_t buf[6];
        (void)rigado_get_mac(buf);
        
        UART_Tx(buf, sizeof buf);
    }
    else if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_GETVERSION_CMD )
    {
        const uint8_t * version_str = (const uint8_t *)FIRMWARE_VERSION_STRING;
        uint8_t version_str_len = strlen(FIRMWARE_VERSION_STRING);
        UART_Tx(version_str, version_str_len);
    }
    else if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_GETDEVICEID_CMD )
    {
        uint8_t buf[8];
        memcpy(buf, (const uint32_t*)NRF_FICR->DEVICEID, 8);
        UART_Tx(buf, sizeof(buf));
    }
    else if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_GETHWID_CMD )
    {
        const uint32_t configId = NRF_FICR->CONFIGID;
        uint8_t buf[sizeof(uint32_t)];
        memcpy(buf, &configId, sizeof(uint32_t));
        UART_Tx(buf, sizeof(buf));
    }
    else if( s_dtm_ctxt.dtm_cmd_from_uart == BMD200_GETPARTINFO_CMD )
    {
        uint32_t flash_size = (NRF_FICR->CODEPAGESIZE * NRF_FICR->CODESIZE) / 1024;
        uint32_t ram_size = (NRF_FICR->NUMRAMBLOCK * NRF_FICR->SIZERAMBLOCKS) / 1024;
        uint32_t part = 0x51822;
        uint8_t buf[20];

        part = swap_le_to_be(part);
        flash_size = swap_le_to_be(flash_size);
        ram_size = swap_le_to_be(ram_size);
        
        memset(buf, 0, sizeof(buf));
        memcpy(&buf[0], &part, sizeof(part));
        memcpy(&buf[12], &flash_size, sizeof(flash_size));
        memcpy(&buf[16], &ram_size, sizeof(ram_size));
        UART_Tx(buf, sizeof(buf));
    }
    else if (putDTMCommand( s_dtm_ctxt.dtm_cmd_from_uart) != DTM_SUCCESS)
    {
            // Extended error handling may be put here. 
            // Default behavior is to return the event on the UART (see below);
            // the event report will reflect any lack of success.
    }

    // Retrieve result of the operation. This implementation will busy-loop
    // for the duration of the byte transmissions on the UART
    if (dtm_event_get(&dtm_result))
    {
        uint8_t dtm_uart_result[2];
    
        /* send the result big endian */
        dtm_uart_result[0] = (dtm_result >> 8) & 0xff;
        dtm_uart_result[1] = (dtm_result) & 0xff;
 
        UART_Tx(dtm_uart_result, sizeof(dtm_uart_result));
    }
    
    //we are done, flush the current state
    resetCtxt();
}

/**@brief Split UART command bit fields into separate command parameters for the DTM library.
*
 * @param[in]   command   The packed UART command.
 * @return      result status from dtmlib.
 */
static uint32_t putDTMCommand(uint16_t command)
{
    dtm_cmd_t      command_code = (command >> 14) & 0x03;
    dtm_freq_t     freq         = (command >> 8) & 0x3F;
    uint32_t       length       = (command >> 2) & 0x3F;
    dtm_pkt_type_t payload      = command & 0x03;

    // Check for Vendor Specific payload.
    if (payload == 0x03) 
    {
        /* Note that in a HCI adaption layer, as well as in the DTM PDU format,
             the value 0x03 is a distinct bit pattern (PRBS15). Even though BLE does not
             support PRBS15, this implementation re-maps 0x03 to DTM_PKT_VENDORSPECIFIC,
             to avoid the risk of confusion, should the code be extended to greater coverage. 
        */
        payload = DTM_PKT_VENDORSPECIFIC;
    }
    return dtm_cmd(command_code, freq, length, payload);
}

static void uartTX(const uint8_t * const data, uint8_t len)
{
    if(data && len)
    {
        for(uint8_t i= 0; i<len; i++)
        {
            simple_uart_put(data[i]);
        }
    }
}

static uint32_t swap_le_to_be(uint32_t input)
{
    uint32_t result;
    uint32_t b0,b1,b2,b3;

    b0 = (input & 0x000000ff) << 24u;
    b1 = (input & 0x0000ff00) << 8u;
    b2 = (input & 0x00ff0000) >> 8u;
    b3 = (input & 0xff000000) >> 24u;

    result = (b0 | b1 | b2 | b3);
    
    return result;
}

static void resetCtxt(void)
{
    timer_stop_uart();
    s_dtm_ctxt.dtm_cmd_from_uart = 0;
    s_dtm_ctxt.is_msb_read = false;
}
