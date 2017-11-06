/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
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
 
 /** @file ble_nus.c
*
* @brief This module manages the BLE UART service.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
#include <string.h>

#include "service.h"
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "storage_intf.h"
#include "uart.h"
#include "lock.h"
#include "ringbuf.h"
#include "sw_irq_manager.h"
#include "gatt.h"
#include "sys_init.h"
#include "timer.h"
#include "ble_nus.h"
#include "bmd_log.h"

#define UART_CONFIG_BAUD_RATE_UUID          0x0004
#define UART_CONFIG_PARITY_UUID             0x0005
#define UART_CONFIG_FLOW_CONTROL_UUID       0x0006
#define UART_CONFIG_STOP_BITS_UUID          0x0007
#define UART_CONFIG_ENABLE_UUID             0x0008
#define UART_CONFIG_CTRL_POINT_UUID         0x0009

#define UART_CONFIG_BAUD_RATE_NAME_STR      "Baud Rate"
#define UART_CONFIG_PARITY_NAME_STR         "Parity"
#define UART_CONFIG_FLOW_CONTROL_NAME_STR   "Flow Control"
#define UART_CONFIG_STOP_BITS_NAME_STR      "Stop Bits"
#define UART_CONFIG_ENABLE_NAME_STR         "Enable"
#define UART_CONFIG_CONTROL_POINT_NAME_STR  "Control Point"

#define ARRAY_COUNT(array) ((sizeof(array)/sizeof(array[0])))

ble_uuid128_t   nus_base_uuid = 
{
    { 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x6E }
};

uint8_t ble_nus_uuid_type;
static uint32_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static const ble_beacon_config_t * mp_beacon_config;

static bool triggered_buffer_notification = false;
static sw_irq_callback_id_t swi_handle;
static uint8_t swi_notif;

typedef enum
{
    Uart_BaudRate,
    Uart_Parity,
    Uart_FlowControl,
    /*Uart_StopBits,*/
    Uart_Enable,
    Uart_Last
} uart_config_char_config_t;

#define FILL_GATT_STRUCT(gatts_val, length, off, data)  \
    do                                                  \
    {                                                   \
        memset(&gatts_val, 0, sizeof(gatts_val));       \
        gatts_val.len = (length);                            \
        gatts_val.offset = (off);                      \
        gatts_val.p_value = (data);                       \
    } while(0)  

/* Helper function prototypes */
static uint32_t send_ble_data(ble_nus_t * p_nus, uint8_t * string, uint16_t length);
static void set_char_md_properties( ble_gatts_char_md_t * char_md, ble_gatts_attr_md_t * cccd_md, 
                                    bool read, bool write, bool write_wo_resp, bool notify, char * user_desc );
static void set_attr_md_properties( ble_gatts_attr_md_t * attr_md, ble_gap_conn_sec_mode_t read_perm, ble_gap_conn_sec_mode_t write_perm );

/**@brief     Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_nus     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_nus_t * p_nus, ble_evt_t * p_ble_evt)
{
    p_nus->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    m_conn_handle = p_nus->conn_handle;
    p_nus->is_notification_enabled = false;
    //ringBufClear(&ble_tx_ring_buffer);
}


/**@brief     Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110
 *            SoftDevice.
 *
 * @param[in] p_nus     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_nus_t * p_nus, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_nus->conn_handle = BLE_CONN_HANDLE_INVALID;
    m_conn_handle = BLE_CONN_HANDLE_INVALID;
    p_nus->is_notification_enabled = false;
}

static bool is_valid_baud(uint32_t baud)
{
    const uint32_t baud_rate_val[] = {
    1200,
    2400,
    4800,
    9600,
    14400,
    19200,
    28800,
    38400,
    57600,
    76800,
    115200,
    230400,
    460800,
    921600,
    1000000
    };
    
    bool result = false;
    for(uint8_t i = 0; i < ARRAY_COUNT(baud_rate_val); i++)
    {
        if(baud == baud_rate_val[i])
        {
            result = true;
            break;
        }
    }
    
    return result;
}

/**@brief     Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_dfu     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_nus_t * p_nus, ble_evt_t * p_ble_evt)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
    
    if (
        (p_evt_write->handle == p_nus->rx_handles.cccd_handle)
        &&
        (p_evt_write->len == 2)
       )
    {
        if (ble_srv_is_notification_enabled(p_evt_write->data))
        {
            p_nus->is_notification_enabled = true;
        }
        else
        {
            p_nus->is_notification_enabled = false;
        }
    }
    else if( (p_evt_write->handle == p_nus->tx_handles.value_handle)
             && (p_nus->data_handler != NULL) )
    {
        /* only write to uart if enabled */
        if(p_nus->enable && !sys_init_is_at_mode())
        {
            bmd_log("ble_rx: ptr 0x%08x, len %d\n", p_evt_write->data, p_evt_write->len); 
            p_nus->data_handler(p_nus, p_evt_write->data, p_evt_write->len);
        }
    }
    else if (p_evt_write->handle == p_nus->baud_rate_handles.value_handle)
    {
        if( p_evt_write->len != sizeof( p_nus->baud_rate ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED };
            if(p_nus->enable)
            {
                response[0] = DEVICE_COMMAND_INVALID_STATE;
            }
            ble_nus_set_baudrate(p_nus->baud_rate);
            ble_beacon_config_send_notification(mp_beacon_config, mp_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            return;
        }
        
        uint32_t baud;
        memcpy( &baud, p_evt_write->data, sizeof baud );
        if(!is_valid_baud(baud))
        {
            uint8_t response[1] = { DEVICE_COMMAND_INVALID_DATA };
            ble_nus_set_baudrate(p_nus->baud_rate);
            ble_beacon_config_send_notification(mp_beacon_config, mp_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            return;
        }
        
        p_nus->baud_rate = baud;
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof settings);
        settings.baud_rate = p_nus->baud_rate;
        storage_intf_set(&settings);
        
        if(p_nus->enable && !sys_init_is_at_mode())
        {
            uart_configure_passthrough_mode(p_nus);
        }
    }
    else if (p_evt_write->handle == p_nus->parity_handles.value_handle)
    {
        if( p_evt_write->len != sizeof( p_nus->parity ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED };
            if(p_nus->enable)
            {
                response[0] = DEVICE_COMMAND_INVALID_STATE;
            }
            ble_beacon_config_send_notification(mp_beacon_config, mp_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            ble_nus_set_parity(p_nus->parity);
            return;
        }
        
        if(p_evt_write->data[0] >= 1)
        {
            p_nus->parity = true;
        }
        else
        {
            p_nus->parity = false;
        }
        
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.parity = p_nus->parity;
        storage_intf_set(&settings);
        
        if(p_nus->enable && !sys_init_is_at_mode())
        {
            uart_configure_passthrough_mode(p_nus);
        }
    }
    else if (p_evt_write->handle == p_nus->flow_control_handles.value_handle)
    {
        if( p_evt_write->len != sizeof( p_nus->flow_control ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED };
            if(p_nus->enable)
            {
                response[0] = DEVICE_COMMAND_INVALID_STATE;
            }
            ble_beacon_config_send_notification(mp_beacon_config, mp_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            ble_nus_set_flow_control(p_nus->flow_control);
            return;
        }
        
        if(p_evt_write->data[0] >= 1)
        {
            p_nus->flow_control = true;
        }
        else
        {
            p_nus->flow_control = false;
        }
        
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.flow_control = p_nus->flow_control;
        storage_intf_set(&settings);
        
        if(p_nus->enable && !sys_init_is_at_mode())
        {
            uart_configure_passthrough_mode(p_nus);
        }
    }
    else if (p_evt_write->handle == p_nus->enable_handles.value_handle)
    {
        bool previous;
        if( p_evt_write->len != sizeof( p_nus->enable ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED };
            ble_nus_set_baudrate(p_nus->baud_rate);
            ble_beacon_config_send_notification(mp_beacon_config, mp_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            ble_nus_set_enable(p_nus->enable);
            return;
        }
        
        previous = p_nus->enable;
        
        if(p_evt_write->data[0] >= 1)
        {
            p_nus->enable = true;
        }
        else
        {
            p_nus->enable = false;
        }
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.uart_enable = p_nus->enable;
        storage_intf_set(&settings);
        
        if(!sys_init_is_at_mode())
        {
			if(previous == false && p_nus->enable == true)
			{
				uart_configure_passthrough_mode(p_nus);
			}
			else if(previous == true && p_nus->enable == false)
			{
				uart_disable_passthrough_mode();
			}
        }
    }
    else
    {
        //Do nothing as this event does not apply for this service
    }
}

static void uart_tx_buffer_event_callback(ringBuf_t * buf, ringBufEvent_t event)
{
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return;
    }
    
    swi_notif = 0xFF;
    switch(event)
    {
        case RINGBUF_EVENT_ALMOST_FULL:
            bmd_log("uart_tx_buf: RINGBUF_EVENT_ALMOST_FULL\n");
            swi_notif = DEVICE_UART_TX_BUFFER_ALMOST_FULL;
            triggered_buffer_notification = true;
            break;
        case RINGBUF_EVENT_FULL:
            bmd_log("uart_tx_buf: RINGBUF_EVENT_FULL\n");
            swi_notif = DEVICE_UART_TX_BUFFER_FULL;
            triggered_buffer_notification = true;
            break;
        case RINGBUF_EVENT_EMPTY:
            bmd_log("uart_tx_buf: RINGBUF_EVENT_EMPTY\n");
            if(triggered_buffer_notification)
            {
                swi_notif = DEVICE_UART_TX_BUFFER_AVAILABLE;
                triggered_buffer_notification = false;
            }
            
            //use the timer to restart
            timer_start_uart();
            break;
        default:
            break;
    }
    
    if(swi_notif != 0xFF)
    {
        sw_irq_manager_trigger_int(swi_handle);
    }
}

uint8_t ble_nus_set_baudrate(uint32_t val)
{
	ble_nus_t *p_nus = services_get_nus_config_obj();
	if (p_nus == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0;
    }
    
	uint16_t len = sizeof(val);
	uint16_t nus_baudrate_value_handle = p_nus->baud_rate_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, nus_baudrate_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_nus_set_parity(uint8_t val)
{
	ble_nus_t *p_nus = services_get_nus_config_obj();
	if (p_nus == NULL) {
		return 3;
	}
	
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0;
    }
    
    uint16_t len = sizeof(val);
    uint16_t nus_parity_value_handle = p_nus->parity_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, nus_parity_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_nus_set_stop_bits(uint8_t val)
{
	ble_nus_t *p_nus = services_get_nus_config_obj();
	if (p_nus == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0;
    }
    
	uint16_t len = sizeof(val);
	uint16_t nus_stopbits_value_handle = p_nus->stop_bits_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, nus_stopbits_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_nus_set_flow_control(uint8_t val)
{
	ble_nus_t *p_nus = services_get_nus_config_obj();
	if (p_nus == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0;
    }
    
	uint16_t len = sizeof(val);
	uint16_t nus_flowcontrol_value_handle = p_nus->flow_control_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, nus_flowcontrol_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_nus_set_enable(uint8_t val)
{
	ble_nus_t *p_nus = services_get_nus_config_obj();
	if (p_nus == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0;
    }
    
	uint16_t len = sizeof(val);
	uint16_t nus_enable_value_handle = p_nus->enable_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, nus_enable_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

static uint32_t data_char_add(ble_nus_t * p_nus, const ble_nus_init_t * p_nus_init, uart_config_char_config_t char_type)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    char *              user_desc;
    ble_gatts_char_handles_t *    
                        handles;
    ble_srv_cccd_security_mode_t 
                        security;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.write_perm);
    
    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    if (p_nus->is_notification_enabled)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));
    
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    }
    
    switch( char_type )
    {
        case Uart_BaudRate:
            user_desc = UART_CONFIG_BAUD_RATE_NAME_STR;
            ble_uuid.uuid = UART_CONFIG_BAUD_RATE_UUID;
            handles = &p_nus->baud_rate_handles;
            attr_char_value.init_len     = sizeof(p_nus->baud_rate);
            attr_char_value.max_len      = sizeof(p_nus->baud_rate);
            attr_char_value.p_value      = (uint8_t *)&p_nus->baud_rate;
            break;
        case Uart_Parity:
            user_desc = UART_CONFIG_PARITY_NAME_STR;
            ble_uuid.uuid = UART_CONFIG_PARITY_UUID;
            handles = &p_nus->parity_handles;
            attr_char_value.init_len     = sizeof(p_nus->parity);
            attr_char_value.max_len      = sizeof(p_nus->parity);
            attr_char_value.p_value      = (uint8_t *)&p_nus->parity;
            break;
        case Uart_FlowControl:
            user_desc = UART_CONFIG_FLOW_CONTROL_NAME_STR;
            ble_uuid.uuid = UART_CONFIG_FLOW_CONTROL_UUID;
            handles = &p_nus->flow_control_handles;
            attr_char_value.init_len     = sizeof(p_nus->flow_control);
            attr_char_value.max_len      = sizeof(p_nus->flow_control);
            attr_char_value.p_value      = (uint8_t *)&p_nus->flow_control;
            break;
        case Uart_Enable:
            user_desc = UART_CONFIG_ENABLE_NAME_STR;
            ble_uuid.uuid = UART_CONFIG_ENABLE_UUID;
            handles = &p_nus->enable_handles;
            attr_char_value.init_len     = sizeof(p_nus->enable);
            attr_char_value.max_len      = sizeof(p_nus->enable);
            attr_char_value.p_value      = (uint8_t *)&p_nus->enable;
            break;
        case Uart_Last:
        default:
            return NRF_ERROR_INVALID_DATA;
    }
    
    set_char_md_properties( &char_md, &cccd_md, true, true, false, false, user_desc );
    set_attr_md_properties( &attr_md, security.read_perm, security.write_perm );
    
    ble_uuid.type = p_nus->uuid_type;
    
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_offs    = 0;
    
    
    err_code = sd_ble_gatts_characteristic_add(p_nus->service_handle, 
                                                &char_md,
                                                &attr_char_value,
                                                handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    return NRF_SUCCESS;
}
// ------------------------------------------------------------------------------


/**@brief       Function for adding RX characteristic.
 *
 * @param[in]   p_nus        Nordic UART Service structure.
 * @param[in]   p_nus_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t rx_char_add(ble_nus_t * p_nus, const ble_nus_init_t * p_nus_init)
{
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    
    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    
    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;
    
    ble_uuid.type             = p_nus->uuid_type;
    ble_uuid.uuid             = BLE_UUID_NUS_RX_CHARACTERISTIC;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    
    attr_md.vloc              = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth           = 0;
    attr_md.wr_auth           = 0;
    attr_md.vlen              = 1;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint8_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_NUS_MAX_RX_CHAR_LEN;
    
    return sd_ble_gatts_characteristic_add(p_nus->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_nus->rx_handles);
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */

}


/**@brief       Function for adding TX characteristic.
 *
 * @param[in]   p_nus        Nordic UART Service structure.
 * @param[in]   p_nus_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t tx_char_add(ble_nus_t * p_nus, const ble_nus_init_t * p_nus_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    
    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.write            = 1;
    char_md.char_props.write_wo_resp    = 1;
    char_md.p_char_user_desc            = NULL;
    char_md.p_char_pf                   = NULL;
    char_md.p_user_desc_md              = NULL;
    char_md.p_cccd_md                   = NULL;
    char_md.p_sccd_md                   = NULL;
    
    ble_uuid.type                       = p_nus->uuid_type;
    ble_uuid.uuid                       = BLE_UUID_NUS_TX_CHARACTERISTIC;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    
    attr_md.vloc                        = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth                     = 0;
    attr_md.wr_auth                     = 0;
    attr_md.vlen                        = 1;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid              = &ble_uuid;
    attr_char_value.p_attr_md           = &attr_md;
    attr_char_value.init_len            = 1;
    attr_char_value.init_offs           = 0;
    attr_char_value.max_len             = BLE_NUS_MAX_TX_CHAR_LEN;
    
    return sd_ble_gatts_characteristic_add(p_nus->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_nus->tx_handles);
}

#ifdef UART_CTRL_PT_ENABLE
static uint32_t control_char_add(ble_nus_t * p_nus, const ble_nus_init_t * p_nus_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_val[20];
    ble_srv_cccd_security_mode_t 
                        security;
    
    memset(initial_val, 0x00, sizeof(initial_val));
    
    memset(&char_md, 0, sizeof(char_md));
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security.write_perm);
    
    if (p_nus->is_notification_enabled)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));
    
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    }
    
    set_char_md_properties( &char_md, &cccd_md, true, true, true, ( (p_nus->is_notification_enabled) ? true : false ), NULL );
    set_attr_md_properties( &attr_md, security.read_perm, security.write_perm );
    
    ble_uuid.type = p_nus->uuid_type;
    ble_uuid.uuid = UART_CONFIG_CTRL_POINT_UUID;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = 10;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = 10;
    attr_char_value.p_value      = (uint8_t *)&initial_val;
    
    err_code = sd_ble_gatts_characteristic_add(p_nus->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_nus->control_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    return NRF_SUCCESS;
}
// ------------------------------------------------------------------------------
#endif


static void send_more_ble_data(ble_nus_t * p_nus)
{
    timer_start_uart();
}

void ble_nus_on_ble_evt(ble_nus_t * p_nus, ble_evt_t * p_ble_evt)
{
    if ((p_nus == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_nus, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_nus, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_nus, p_ble_evt);
            break;
        
        case BLE_EVT_TX_COMPLETE:
            send_more_ble_data(p_nus);
            break;

        default:
            // No implementation needed.
            break;
    }
}

static void swi_handler(void * param)
{
    (void)ble_beacon_config_send_notification(mp_beacon_config,
                mp_beacon_config->beacon_config_control_handles.value_handle,
                (uint8_t*)param, sizeof(uint8_t));
}


uint32_t ble_nus_init(ble_nus_t * p_nus, const ble_nus_init_t * p_nus_init)
{
    uint32_t        err_code;
    ble_uuid_t      ble_uuid;

    const default_app_settings_t * app_settings;
    
    
    if ((p_nus == NULL) || (p_nus_init == NULL))
    {
        return NRF_ERROR_NULL;
    }
    
    /* apply settings */
    app_settings = storage_intf_get();
    
    // Initialize service structure.
    p_nus->conn_handle              = BLE_CONN_HANDLE_INVALID;
    p_nus->data_handler             = p_nus_init->data_handler;
    p_nus->is_notification_enabled  = true;
    
    // Initialize settings
    p_nus->baud_rate 		= app_settings->baud_rate;
    p_nus->parity 			= app_settings->parity;
    p_nus->flow_control = app_settings->flow_control;
    p_nus->stop_bits 		= app_settings->stop_bits;
    p_nus->enable 			= app_settings->uart_enable;

    /**@snippet [Adding proprietary Service to S110 SoftDevice] */

    // Add custom base UUID.
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &p_nus->uuid_type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    m_conn_handle = BLE_CONN_HANDLE_INVALID;
    ble_nus_uuid_type = p_nus->uuid_type;
    ble_uuid.type = p_nus->uuid_type;
    ble_uuid.uuid = BLE_UUID_NUS_SERVICE;

    // Add service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_nus->service_handle);
    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Add RX Characteristic.
    err_code = rx_char_add(p_nus, p_nus_init);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add TX Characteristic.
    err_code = tx_char_add(p_nus, p_nus_init);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    for(uint8_t i = 0; i < (uint8_t)Uart_Last; i++)
    {
        err_code = data_char_add(p_nus, p_nus_init, (uart_config_char_config_t)i);
        if(err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    
    // Add Control Point Characteristic.
    p_nus->is_notification_enabled = false;
    
    (void)sw_irq_manager_register_callback(swi_handler, &swi_handle, &swi_notif);
    
    return NRF_SUCCESS;
}

uint32_t ble_nus_set_beacon_config_ptr(const ble_beacon_config_t * p_beacon_config)
{
    if(p_beacon_config == NULL)
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    mp_beacon_config = p_beacon_config;
    return NRF_SUCCESS;
}

uint32_t ble_nus_send_string(ble_nus_t * p_nus, uint8_t * string, uint16_t length)
{
    return send_ble_data(p_nus, string, length);
}

void ble_nus_register_uart_callbacks(void)
{
#ifdef NRF52
    uart_reg_tx_buf_event_callback(RINGBUF_EVENT_ALMOST_FULL,   uart_tx_buffer_event_callback);
    uart_reg_tx_buf_event_callback(RINGBUF_EVENT_FULL,          uart_tx_buffer_event_callback);
    uart_reg_tx_buf_event_callback(RINGBUF_EVENT_EMPTY,         uart_tx_buffer_event_callback);
#endif    
}

static uint32_t send_ble_data(ble_nus_t * p_nus, uint8_t * string, uint16_t length)
{
    ble_gatts_hvx_params_t hvx_params;
    if (p_nus == NULL)
    {
        return NRF_ERROR_NULL;
    }
    
    if ((p_nus->conn_handle == BLE_CONN_HANDLE_INVALID) 
        || (!p_nus->is_notification_enabled))
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    if (length > BLE_NUS_MAX_DATA_LEN)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_nus->rx_handles.value_handle;
    hvx_params.p_data = string;
    hvx_params.p_len  = &length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    
    uint32_t err_code = sd_ble_gatts_hvx(p_nus->conn_handle, &hvx_params);
    
    return err_code;
}


/* Helper functions */
static void set_char_md_properties( ble_gatts_char_md_t * char_md, ble_gatts_attr_md_t * cccd_md, 
                                    bool read, bool write, bool write_wo_resp, bool notify,  char * user_desc )
{
    char_md->char_props.notify = (notify) ? 1 : 0;
    char_md->char_props.read = (read) ? 1 : 0;
    char_md->char_props.write = (write) ? 1 : 0;
    char_md->char_props.write_wo_resp = (write_wo_resp) ? 1 : 0;
    char_md->p_char_user_desc  = (user_desc != NULL) ? (uint8_t*)user_desc : NULL;
    char_md->char_user_desc_max_size = (user_desc != NULL) ? strlen( user_desc ) : 0;
    char_md->char_user_desc_size = (user_desc != NULL) ? strlen( user_desc ) : 0;
    char_md->p_char_pf         = NULL;
    char_md->p_user_desc_md    = NULL;
    char_md->p_cccd_md         = (notify) ? cccd_md : NULL;
    char_md->p_sccd_md         = NULL;
}
// ------------------------------------------------------------------------------

static void set_attr_md_properties( ble_gatts_attr_md_t * attr_md, ble_gap_conn_sec_mode_t read_perm, ble_gap_conn_sec_mode_t write_perm )
{
    memset(attr_md, 0, sizeof(ble_gatts_attr_md_t));

    attr_md->read_perm  = read_perm;
    attr_md->write_perm = write_perm;
    attr_md->vloc       = BLE_GATTS_VLOC_STACK;
    attr_md->rd_auth    = 0;
    attr_md->wr_auth    = 0;
    attr_md->vlen       = 0;
}
// ------------------------------------------------------------------------------
