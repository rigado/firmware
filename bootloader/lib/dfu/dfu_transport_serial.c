#include "dfu_transport_serial.h"
#include "dfu.h"
#include "dfu_types.h"
#ifdef SDK10
    #include "nrf51.h"
#else
    #include "nrf_soc.h"
#endif
#include "app_error.h"
#include "app_util.h"
#include <stddef.h>
#include <string.h>

#include "rigdfu.h"
#include "rigdfu_util.h"
#include "rigdfu_serial.h"
#include "version.h"
#include "queue.h"
#include "patcher.h"

#include "nrf_gpio.h"

/* Serial RX queue */
DECLARE_QUEUE(queue, 512);

int offset = 0;
static uint32_t     m_num_of_firmware_bytes_rcvd;                                            /**< Cumulative number of bytes of firmware data received. */
static bool         m_is_patch_complete = false;

static void app_data_process(serial_frame_t* frame);
static void generic_data_process( serial_dfu_op_t op,
                                    serial_frame_t* frame,
                                    uint32_t (*handler)(dfu_update_packet_t *p),
                                    bool send_response_on_success);
static serial_dfu_resp_val_t nrf_err_code_translate(uint32_t err_code, const uint8_t op_code);
static uint32_t serial_dfu_op_to_dfu_packet_type(serial_dfu_op_t op);
static void put_op_response(uint8_t op_id, uint8_t op_sts);
static void put_frame(uint8_t * op_data, uint16_t op_data_len);
static void process_frame(struct serial_frame *frame);
static void patch_data_process(serial_frame_t *p_frame);

static bool scheduled_process = false;
static void process_rx_bytes(void *p_event_data, uint16_t event_size)
{
    static struct serial_frame frame;
//    static uint8_t * const framebytes = (uint8_t *)&(frame.packet_len); //we need data to be word aligned...
    //static int offset = 0;
    static enum 
        { wait_frame, wait_len, wait_op, wait_data } state = wait_frame;
    static bool escape = false;
    int c;

    /* clear the uart proc event */
    scheduled_process = false;

    while ((c = queue_pop(&queue)) != -1)
    {
        /* Regardless of state, start processing a new frame when
           the marker is received. */
        if (c == SERIAL_FRAME_MARKER) {
            state = wait_len;
            offset = 0;
            escape = false;
            continue;
        }

        /* If we were escaping, decode the escaped character. */
        if (escape) {
            if (c == SERIAL_FRAME_ESCAPE_ESCAPE)
                c = SERIAL_FRAME_ESCAPE;
            else if (c == SERIAL_FRAME_ESCAPE_MARKER)
                c = SERIAL_FRAME_MARKER;
            escape = false;
        } else {
            /* If we get an escape character, wait for the next one. */
            if (c == SERIAL_FRAME_ESCAPE) {
                escape = true;
                continue;
            }
        }

        /* If we want a new frame and don't have it, ignore */
        if (state == wait_frame) {
            continue;
        } 
        
        if(state == wait_len) {
            frame.packet_len = c;
            state = wait_op;
        } else if(state == wait_op) {
            frame.opcode = c;
            state = wait_data;
        } else if(state == wait_data) {
            frame.data[offset] = c;
            offset++;
        }
        
        if (state == wait_data &&
            (offset + 2) >= frame.packet_len) {
            state = wait_frame;
            process_frame(&frame);
        }
    }
}

/* Called whenever a byte is received (from interrupt context) */
static void uart_rx(uint8_t x)
{
    _queue_push(&queue, x);
    /* Schedule a call to the function that will pull bytes from the
       queue and process them (from outside interrupt context) */
    if (!scheduled_process) {
        scheduled_process = true;
        app_sched_event_put(NULL, 0, process_rx_bytes);
    }
}

/* Process a frame of serial data */
static void process_frame(struct serial_frame *frame)
{
    uint32_t err_code;
    serial_dfu_resp_val_t resp_val;
    
    //rigdfu_serial_printf("Got frame: len %d, opcode %d\n", frame->packet_len, frame->opcode);

    //illegal length
    if (frame->packet_len < 2)
        return;
    
    switch(frame->opcode)
    {
        case SERIAL_OP_START_DFU:
            //we will issue the reply after the erase is complete via dfu_cb_handler 
            generic_data_process((serial_dfu_op_t)frame->opcode, frame, dfu_start_pkt_handle, false);
            break;
        
        case SERIAL_OP_INITIALIZE_DFU:
            generic_data_process((serial_dfu_op_t)frame->opcode, frame, dfu_init_pkt_handle, true);
            break;
        
        case SERIAL_OP_INITIALIZE_PATCH:
            generic_data_process((serial_dfu_op_t)frame->opcode, frame, dfu_patch_init_pkt_handle, true);
            break;
        
        case SERIAL_OP_RECEIVE_FIRMWARE_IMAGE:
            app_data_process(frame);
            break;
        
        case SERIAL_OP_RECEIVE_PATCH_IMAGE:
            patch_data_process(frame);
            break;
        
        case SERIAL_OP_VALIDATE_FIRMWARE_IMAGE:
            err_code = dfu_image_validate();
        
            resp_val = nrf_err_code_translate(err_code, SERIAL_OP_VALIDATE_FIRMWARE_IMAGE);
            put_op_response(SERIAL_OP_VALIDATE_FIRMWARE_IMAGE,resp_val);
            break;
        
        case SERIAL_OP_ACTIVATE_FIRMWARE_AND_RESET:
            err_code = dfu_image_activate();
            
            resp_val = nrf_err_code_translate(err_code, SERIAL_OP_ACTIVATE_FIRMWARE_AND_RESET);
            put_op_response(SERIAL_OP_ACTIVATE_FIRMWARE_AND_RESET,resp_val);
        
            if (err_code != NRF_SUCCESS)
            {
                dfu_reset();
            }
            break;
            
        case SERIAL_OP_PROTOCOL_VER:
            put_op_response(SERIAL_OP_PROTOCOL_VER, API_PROTOCOL_VERSION);
            break;
        
        case SERIAL_OP_SYSTEM_RESET:
            dfu_reset();
            break;
        
        case SERIAL_OP_CONFIG:
            generic_data_process((serial_dfu_op_t)frame->opcode, frame, dfu_config_pkt_handle, true);
            break;
            
        
        case SERIAL_OP_RESPONSE: //illegal
        default:
            //rigdfu_serial_printf("invalid opcode\n", frame->opcode);
            break;
    }
}

/* Function for handling the callback events from the dfu module. */
static void dfu_cb_handler(uint32_t packet, uint32_t result, uint8_t * p_data)
{
    switch (packet)
    {
        serial_dfu_resp_val_t resp_val;
        //uint32_t            err_code;

        case START_PACKET:
            // Translate the err_code returned by the above function to DFU Response Value.
            resp_val = nrf_err_code_translate(result, SERIAL_OP_START_DFU);
            put_op_response(SERIAL_OP_START_DFU,resp_val);
            break;

        case DATA_PACKET: 
            break;
        case CONFIG_PACKET:
            break;
        case PATCH_DATA_PACKET:
            if(result == NRF_SUCCESS)
            {
                if(!m_is_patch_complete)
                {
                    patch_data_process(NULL);
                }
            }
            else
            {
                resp_val = nrf_err_code_translate(result, SERIAL_OP_RECEIVE_PATCH_IMAGE);
                put_op_response(SERIAL_OP_RECEIVE_PATCH_IMAGE, resp_val);                
            }
            break;
        default:
            // ignore.
            break;
    }
}

uint32_t dfu_transport_update_start_serial(void)
{
    /* Reinitialize serial port and use our own RX handler */
    rigdfu_serial_reinit_dfu();
    rigdfu_serial_set_rx_handler(uart_rx);

    /* When we start the serial transport, send an identification
       string.  Format is 42-characters:
         RigDfu/3.1.0-release/11:22:33:aa:bb:cc\r\n
       where 3.1.0-release is the version
       and 11:22:33:aa:bb:cc is the hex MAC address from UICR
    */
    rigdfu_serial_puts("RigDfu/" RIGDFU_VERSION "/");
    rigdfu_serial_puts(rigado_get_mac_string());
    rigdfu_serial_put('\r');
    rigdfu_serial_put('\n');
    
    /* Register DFU callback and start processing RX data */
    dfu_register_callback(dfu_cb_handler);

    return NRF_SUCCESS;
}


uint32_t dfu_transport_close_serial(void)
{
    rigdfu_serial_close();
    return NRF_SUCCESS;
}


static void put_op_response(uint8_t op_id, uint8_t op_sts)
{
    uint8_t msg[2];
    msg[0] = op_id;
    msg[1] = op_sts;
    
    put_frame(msg,sizeof(msg));
}

static void put_frame(uint8_t * op_data, uint16_t op_data_len)
{
    //calculate the data length with escapes
    uint16_t pkt_len = op_data_len;
    for(uint8_t i=0; i<op_data_len; i++)
    {
        if( op_data[i] == SERIAL_FRAME_MARKER 
            || op_data[i] == SERIAL_FRAME_ESCAPE )
            pkt_len++;
    }
    
    //check length
    if((pkt_len + SERIAL_FRAME_HDR_SZ) <= 0xff)
    {
        //frame start marker
        rigdfu_serial_put(SERIAL_FRAME_MARKER);
        
        //frame length
        rigdfu_serial_put(pkt_len + SERIAL_FRAME_HDR_SZ);
        
        //frame op, always response
        rigdfu_serial_put(SERIAL_OP_RESPONSE);
        
        //frame data
        for(uint8_t i=0; i<op_data_len; i++)
        {
            //escape it
            if(op_data[i] == SERIAL_FRAME_MARKER || op_data[i] == SERIAL_FRAME_ESCAPE)
            {
                rigdfu_serial_put(SERIAL_FRAME_ESCAPE);
                
                if(op_data[i] == SERIAL_FRAME_MARKER)
                    rigdfu_serial_put(SERIAL_FRAME_ESCAPE_MARKER);
                else
                    rigdfu_serial_put(SERIAL_FRAME_ESCAPE_ESCAPE);
            }
            else
            {
                rigdfu_serial_put(op_data[i]);
            }
        }        
    }
}



/**@brief Function for processing data written by the peer to the DFU Packet
 *        Characteristic.
 *
 * @param[in] op        Serial DFU Op Code.
 * @param[in] frame     Pointer to the data freame rx'd
 * @param[in] handler   DFU handler (e.g. dfu_init_pkt_handle)
 * @param[in] send_response_on_success   If true, send response on success.
 *
 */
static void generic_data_process( serial_dfu_op_t op,
                                    serial_frame_t* frame,
                                    uint32_t (*handler)(dfu_update_packet_t *p),
                                    bool send_response_on_success)
{
    uint32_t err_code;
    uint8_t frame_payload_len = frame->packet_len - SERIAL_FRAME_HDR_SZ;
    dfu_update_packet_t dfu_pkt;

    /* Length must be a multiple of 4 bytes */
    if ( (frame_payload_len & 0x3) != 0)
    {
        put_op_response(op, SERIAL_DFU_RESP_VAL_NOT_SUPPORTED);
        return;
    }

    /* Pass the data to the packet handler */
    dfu_pkt.packet_type = serial_dfu_op_to_dfu_packet_type(op);
    dfu_pkt.params.data_packet.p_data_packet = (uint32_t*)frame->data;
    dfu_pkt.params.data_packet.packet_length = frame_payload_len / sizeof(uint32_t);

    err_code = handler(&dfu_pkt);

    if (err_code == NRF_ERROR_INVALID_LENGTH) {
        /* We need more data -- don't send any response */
        return;
    }

    if (err_code == NRF_SUCCESS && send_response_on_success == false)
        return;

    /* Send response (success or error) */
    serial_dfu_resp_val_t resp_val = nrf_err_code_translate(err_code, op);
    put_op_response(op,resp_val);
}

/**@brief     Function to convert an nRF51 error code to a DFU Response Value.
 *
 * @details   This function will convert a given nRF51 error code to a DFU Response Value. The
 *            result of this function depends on the current DFU procedure in progress, given as
 *            input in current_dfu_proc parameter.
 *
 * @param[in] err_code         The nRF51 error code to be converted.
 * @param[in] current_dfu_proc Current DFU procedure in progress.
 *
 * @return    Converted Response Value.
 */
static serial_dfu_resp_val_t nrf_err_code_translate(uint32_t err_code, const uint8_t op_code)
{
    switch (err_code)
    {
        case NRF_SUCCESS:
            return SERIAL_DFU_RESP_VAL_SUCCESS;

        case NRF_ERROR_INVALID_STATE:
            return SERIAL_DFU_RESP_VAL_INVALID_STATE;

        case NRF_ERROR_NOT_SUPPORTED:
            return SERIAL_DFU_RESP_VAL_NOT_SUPPORTED;

        case NRF_ERROR_DATA_SIZE:
            return SERIAL_DFU_RESP_VAL_DATA_SIZE;

        case NRF_ERROR_INVALID_DATA:
            if (op_code == SERIAL_OP_VALIDATE_FIRMWARE_IMAGE || op_code == SERIAL_OP_INITIALIZE_PATCH)
            {
                // When this error is received in Validation phase, then it maps to a CRC Error.
                // Refer dfu_image_validate function for more information.
                return SERIAL_DFU_RESP_VAL_CRC_ERROR;
            }
            return SERIAL_DFU_RESP_VAL_OPER_FAILED;

        default:
            return SERIAL_DFU_RESP_VAL_OPER_FAILED;
    }
}

static uint32_t serial_dfu_op_to_dfu_packet_type(serial_dfu_op_t op)
{
    uint32_t dfu_packet = INVALID_PACKET;
    
    switch(op)
    {
        case SERIAL_OP_START_DFU:
            dfu_packet = START_PACKET;
            break;
        
        case SERIAL_OP_INITIALIZE_DFU:
            dfu_packet = INIT_PACKET;
            break;
        
        case SERIAL_OP_RECEIVE_FIRMWARE_IMAGE:
            dfu_packet = DATA_PACKET;
            break;
        
        case SERIAL_OP_CONFIG:
            dfu_packet = CONFIG_PACKET;
            break;
        
        case SERIAL_OP_INITIALIZE_PATCH:
            dfu_packet = PATCH_INIT_PACKET;
            break;
        
        case SERIAL_OP_RECEIVE_PATCH_IMAGE:
            dfu_packet = PATCH_DATA_PACKET;
            break;
        
        default:
            dfu_packet = INVALID_PACKET;
            break;
    }
    
    return dfu_packet;
}


/**@brief     Function for processing application data written by the peer to the DFU Packet
 *            Characteristic.
 *
 * @param[in] p_dfu     DFU Service Structure.
 * @param[in] p_evt     Pointer to the event received from the S110 SoftDevice.
 */
static void app_data_process(serial_frame_t* frame)
{
    volatile uint32_t err_code;
    uint8_t frame_payload_len = frame->packet_len - SERIAL_FRAME_HDR_SZ;

    if ((frame_payload_len & (sizeof(uint32_t) - 1)) != 0)
    {
        // Data length is not a multiple of 4 (word size).
        put_op_response(SERIAL_OP_RECEIVE_FIRMWARE_IMAGE, SERIAL_DFU_RESP_VAL_NOT_SUPPORTED);
        return;
    }

    //now send the data to the DFU module
    dfu_update_packet_t dfu_pkt;
    dfu_pkt.packet_type                      = DATA_PACKET;
    dfu_pkt.params.data_packet.packet_length = frame_payload_len / sizeof(uint32_t);
    dfu_pkt.params.data_packet.p_data_packet = (uint32_t*)frame->data;
   
    err_code = dfu_data_pkt_handle(&dfu_pkt);

    if (err_code == NRF_SUCCESS)
    {
        // All the expected firmware data has been received and processed successfully.
        m_num_of_firmware_bytes_rcvd += frame_payload_len;

        // Notify the DFU Controller about the success about the procedure.
        put_op_response(SERIAL_OP_RECEIVE_FIRMWARE_IMAGE, SERIAL_DFU_RESP_VAL_SUCCESS);
    }
    else if (err_code == NRF_ERROR_INVALID_LENGTH)
    {
        // Firmware data packet was handled successfully. And more firmware data is expected.
        m_num_of_firmware_bytes_rcvd += frame_payload_len;
        
        //DSC - no response here, this seems to hang up the app_uart driver for some reason
        put_op_response(SERIAL_OP_RECEIVE_FIRMWARE_IMAGE, SERIAL_DFU_RESP_VAL_OK_MORE_DATA_EXP);
    }
    else
    {   
        //indicate the error
        serial_dfu_resp_val_t resp = nrf_err_code_translate(SERIAL_OP_RECEIVE_FIRMWARE_IMAGE,err_code);
        put_op_response(SERIAL_OP_RECEIVE_FIRMWARE_IMAGE,resp);
    }
}

static void patch_data_process(serial_frame_t *p_frame)
{
    volatile uint32_t err_code;
    dfu_update_packet_t dfu_pkt;
    int32_t result = 0;
    
    //if the frame payload is non-null, process the new packet
    //otherwise, just process more patch data since a flash write
    //likely just finished
    if(p_frame != NULL)
    {
        uint8_t frame_payload_len = p_frame->packet_len - SERIAL_FRAME_HDR_SZ;
    
        memset(&dfu_pkt, 0, sizeof(dfu_pkt));
        dfu_pkt.packet_type                      = PATCH_DATA_PACKET;
        dfu_pkt.params.data_packet.packet_length = frame_payload_len / sizeof(uint32_t);
        dfu_pkt.params.data_packet.p_data_packet = (uint32_t*)p_frame->data;
        result = dfu_patch_data_pkt_handle(p_frame->data, frame_payload_len);
    }
    else
    {
        result = dfu_patch_data_pkt_handle(NULL, 0);
    }
    
    if(result == PATCHER_NEED_MORE)
    {
        put_op_response(SERIAL_OP_RECEIVE_PATCH_IMAGE, SERIAL_DFU_RESP_VAL_OK_MORE_DATA_EXP);
    }
    else if(result == PATCHER_FLASHING)
    {
        //do nothing?
    }
    else if(result == PATCHER_COMPLETE)
    {
        put_op_response(SERIAL_OP_RECEIVE_PATCH_IMAGE, SERIAL_DFU_RESP_VAL_SUCCESS);
        m_is_patch_complete = true;
    }
    else
    {
        //indicate the error
        serial_dfu_resp_val_t resp = nrf_err_code_translate(SERIAL_OP_RECEIVE_PATCH_IMAGE, err_code);
        put_op_response(SERIAL_OP_RECEIVE_PATCH_IMAGE,resp);
    }
    
    m_num_of_firmware_bytes_rcvd += patcher_get_bytes_received();
}
