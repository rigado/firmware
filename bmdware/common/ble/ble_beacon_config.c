/** @file ble_beacon_config.c
*
* @brief This module manages the beacon configuration service.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <string.h>
#include "nrf_delay.h"
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "app_util.h"
#include "app_error.h"
#include "storage_intf.h"
#include "lock.h"
#include "service.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include "softdevice_handler.h"
#include "gpio_ctrl.h"
#include "at_commands.h"
#include "bootloader_info.h"
#include "gap.h"
#include "version.h"
#include "sys_init.h"

#include "ble_beacon_config.h"
#include "nrf_advertiser.h"

#define BOOTLOADER_DFU_START        0xB1
#define RESTART_APP                 0xC5

#define MINIMUM_LOCK_CODE_LEN       4
#define MAXIMUM_LOCK_CODE_LEN       19 

#define MAX_CUSTOM_BEACON_DATA1     19
#define MAX_CUSTOM_BEACON_DATA2     12

#define MINIMUM_CONN_ADV_INT        20
#define MAXIMUM_CONN_ADV_INT        2500

/* Control Point Commands */
#define DEVICE_SET_CUSTOM_BCN_DATA1     0x20
#define DEVICE_SET_CUSTOM_BCN_DATA2     0x21
#define DEVICE_SET_CUSTOM_BCN_SAVE      0x22
#define DEVICE_CLEAR_CUSTOM_BCN_DATA    0x23
#define DEVICE_SET_RSSI_CAL             0x40
#define DEVICE_GET_RSSI_CAL             0x41
#define DEVICE_SET_CONN_ADV_INT         0x42
#define DEVICE_GET_CONN_ADV_INT         0x43
#define DEVICE_CONFIG_GPIO              0x50
#define DEVICE_WRITE_GPIO               0x51
#define DEVICE_READ_GPIO                0x52
#define DEVICE_CONFIG_GPIO_GET          0x53
#define DEVICE_CONFIG_STATUS_PIN        0x54
#define DEVICE_UNCONFIG_STATUS_PIN      0x55
#define DEVICE_GET_STATUS_PIN_CONFIG    0x56
#define DEVICE_READ_STATUS_PIN          0x57
#define DEVICE_BOOTLOADER_INFO_GET      0x60
#define DEVICE_SET_NAME                 0x61
#define DEVICE_GET_NAME                 0x62
#define DEVICE_GET_PROTOCOL_VERSION     0x63
#define DEVICE_UNLOCK				    0xF8
#define DEVICE_SET_PASSWORD			    0x31
#define DEVICE_SET_AT_HOTSWAP           0x70
#define DEVICE_GET_AT_HOTSWAP           0x71
#define DEVICE_SYSTEM_RESET             0x372f104b
#define BOOTLOADER_RESET_COMMAND        0x57305603
#define BOOTLOADER_RESET_COMMAND_NEW    0x0bf4df96

#define ANCS_DATA_CHUNK_LENGTH      20
#define MIN_ADV_INTERVAL            100
#define MAX_ADV_INTERVAL            4000

//#define UUID_STR "a816b4c5-0754-4beb-94a8-0e8c7fd89529"
#define BEACON_CONFIG_UUID_STR "b7717580-b82a-4502-bd90-7f703fb31324"

#define BEACON_CONFIG_CTRL_POINT_UUID           0xB43F
#define BEACON_CONFIG_UUID_UUID                 0xB53F
#define BEACON_CONFIG_MAJOR_UUID                0xB63F
#define BEACON_CONFIG_MINOR_UUID                0xB73F
#define BEACON_CONFIG_INTERVAL_UUID             0xB83F
#define BEACON_CONFIG_TX_POWER_UUID             0xB93F
#define BEACON_CONFIG_ENABLE_UUID               0xBA3F
#define BEACON_CONFIG_CONNECTABLE_TX_POWER_UUID	0xBB3F

#define BEACON_CONFIG_CTRL_POINT_NAME_STR       "Control Point"
#define BEACON_CONFIG_UUID_NAME_STR             "UUID"
#define BEACON_CONFIG_MAJOR_NAME_STR            "Major Number"
#define BEACON_CONFIG_MINOR_NAME_STR            "Minor Number"
#define BEACON_CONFIG_INTERVAL_NAME_STR         "Adv Interval"
#define BEACON_CONFIG_TX_POWER_NAME_STR         "Beacon Tx Power"
#define BEACON_CONFIG_ENABLE_NAME_STR           "Enable"
#define BEACON_CONFIG_CONNECTABLE_TX_POWER_NAME_STR     \
                                                "Connectable Tx Power"

typedef enum
{
    Beacon_UUID,
    Beacon_Major,
    Beacon_Minor,
    Beacon_Interval,
    Beacon_TxPower,
    Beacon_Enable,
	Beacon_Connectable_TxPower,
	Beacon_Last,
} beacon_config_char_config_t;

static uint8_t m_ble_beacon_config_uuid_type;
//48b0fcd616ab43ac83453daeb999c29c
static const uint8_t settings_reset[] =
    { 0x48, 0xb0, 0xfc, 0xd6, 0x16, 0xab, 0x43, 0xac, 0x83, 0x45, 0x3d, 0xae, 0xb9, 0x99, 0xc2, 0x9c };

const ble_uuid128_t ble_beacon_config_service_uuid128 =
{
   {
    // b7717580-b82a-4502-bd90-7f703fb31324
    // 2413b33f-707f-90bd-0245-2ab8807571b7
    // a816b4c5-0754-4beb-94a8-0e8c7fd89529
    // 29957fd8-8c0e-a894-eb4b-5407c5b416a8 - displayed in Master Control Panel and iOS/Android
    0xb7, 0x71, 0x75, 0x80, 0xb8, 0x2a, 0x45, 0x20,
    0xbd, 0x90, 0x7f, 0x70, 0x3f, 0xb3, 0x13, 0x24

   }
};

#define FILL_GATT_STRUCT(gatts_val, length, off, data)  \
    do                                                  \
    {                                                   \
        memset(&gatts_val, 0, sizeof(gatts_val));       \
        gatts_val.len = (length);                            \
        gatts_val.offset = (off);                      \
        gatts_val.p_value = (data);                       \
    } while(0)                                          

// ------------------------------------------------------------------------------

static uint32_t m_conn_handle;
static uint8_t m_temp_beacon_data[CUSTOM_BEACON_DATA_MAX_LEN];
static uint8_t m_temp_beacon_data_len = 0;

/* Helper function prototypes */
static void set_char_md_properties( ble_gatts_char_md_t * char_md, ble_gatts_attr_md_t * cccd_md, 
                                    bool read, bool write, bool write_wo_resp, bool notify, char * user_desc );
static void set_attr_md_properties( ble_gatts_attr_md_t * attr_md, ble_gap_conn_sec_mode_t read_perm, ble_gap_conn_sec_mode_t write_perm, bool vlen );
// ------------------------------------------------------------------------------
static void send_command_response(ble_beacon_config_t * p_beacon_config, uint8_t * response, uint8_t rsp_len)
{
    uint8_t rsp[20];
    memset(rsp, 0, sizeof(rsp));
    memcpy(rsp, response, rsp_len);
    ble_beacon_config_send_notification(p_beacon_config, 
                p_beacon_config->beacon_config_control_handles.value_handle, 
                rsp, 
                rsp_len);
}

static void send_command_invalid(ble_beacon_config_t * p_beacon_config)
{
    uint8_t response = DEVICE_COMMAND_INVALID_COMMAND;
    send_command_response(p_beacon_config, &response, sizeof(response));
}

static void send_command_success(ble_beacon_config_t * p_beacon_config)
{
    uint8_t response = COMMAND_SUCCESS;
    send_command_response(p_beacon_config, &response, sizeof(response));
}

static void handle_beacon_config_ctrl_write( ble_beacon_config_t * p_beacon_config, uint8_t * data, uint16_t length )
{
    uint8_t response[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; 
        
    /* Check for settings reset command */
    {
        if((length == sizeof(settings_reset))
            && (memcmp(data, settings_reset, sizeof(settings_reset)) == 0))
        {            
            storage_intf_set(&default_settings);
            storage_intf_save();
            
            gap_update_device_name();
            
            send_command_success(p_beacon_config);
            return;
        }
    }
    
    if(DEVICE_GET_PROTOCOL_VERSION == data[0])
    {
        uint8_t resp[2] = { data[0], API_PROTOCOL_VERSION };
        send_command_response(p_beacon_config, resp, sizeof(resp));
        return;
    }
    
    if(data[0] == DEVICE_UNLOCK )
    {
        //EPS: More than 20 byte write is not possible, however, it may be possible in next chip revision.
        //This is left here as a note to verify the validity of this check on future versions.
        if((length - 1) < MINIMUM_LOCK_CODE_LEN || (length - 1) > MAXIMUM_LOCK_CODE_LEN )
        {
            response[0] = DEVICE_COMMAND_INVALID_LEN;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        } 
    }
    
    if(lock_is_locked() && data[0] != DEVICE_UNLOCK)
    {
        response[0] = DEVICE_LOCKED;
        send_command_response(p_beacon_config, response, sizeof(response));
        return;
    }
    
    if(data[0] == DEVICE_UNLOCK)
    {
        lock_password_t pw;
		uint8_t pw_len = length-1;
		memset(&pw, 0, sizeof pw);
		memcpy(&pw.password[0], &data[1], pw_len );
			
        if(!lock_clear(&pw))
        {
            response[0] = DEVICE_UNLOCK_FAILED;
        }
        else
        {
            response[0] = COMMAND_SUCCESS;
        }
		
        send_command_response(p_beacon_config, response, sizeof(response));		
		return;
    }
    else if(data[0] == DEVICE_SET_PASSWORD)
    {
        lock_password_t pw;
        uint8_t pw_len = length-1;
		memset(&pw, 0, sizeof pw);
		memcpy(&pw.password[0], &data[1], pw_len );
        response[0] = COMMAND_SUCCESS;
        
        if((length - 1) < MINIMUM_LOCK_CODE_LEN || (length - 1) > MAXIMUM_LOCK_CODE_LEN )
        {
            response[0] = DEVICE_COMMAND_INVALID_LEN;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        } 
        
        if(!lock_set_password(&pw))
        {
            response[0] = DEVICE_UPDATE_PIN_FAILED;
        }
        
        send_command_response(p_beacon_config, response, sizeof(response));
        
        lock_set();
        return;
    }
    else if(data[0] == DEVICE_SET_RSSI_CAL)
    {
        rssi_cal_t cal;
        
        /* Verify tx power */
        if(length != 3 || !at_util_validate_power((int8_t)data[1]))
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        cal.tx_power = data[1];
        cal.rssi = data[2];
        
        const default_app_settings_t * cur_settings = storage_intf_get();
        default_app_settings_t settings;
        memcpy(&settings, cur_settings, sizeof(settings));
        memcpy(&settings.rssi_cal, &cal, sizeof(rssi_cal_t));
        storage_intf_set(&settings);
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_GET_RSSI_CAL)
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        response[0] = DEVICE_GET_RSSI_CAL;
        response[1] = cur_settings->rssi_cal.tx_power;
        response[2] = cur_settings->rssi_cal.rssi;
        
        send_command_response(p_beacon_config, response, sizeof(response));
        return;
    }
    else if(data[0] == DEVICE_GET_CONN_ADV_INT)
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        response[0] = DEVICE_GET_CONN_ADV_INT;
        memcpy(&response[1], &cur_settings->conn_adv_interval, sizeof(cur_settings->conn_adv_interval));
        
        send_command_response(p_beacon_config, response, sizeof(response));
        return;
    }
    else if(data[0] == DEVICE_SET_CONN_ADV_INT)
    {
        uint16_t conn_adv_int;
        
        memcpy(&conn_adv_int, &data[1], sizeof(conn_adv_int));
        if(conn_adv_int < MINIMUM_CONN_ADV_INT || conn_adv_int > MAXIMUM_CONN_ADV_INT)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        const default_app_settings_t * cur_settings = storage_intf_get();
        default_app_settings_t settings;
        memcpy(&settings, cur_settings, sizeof(settings));
        memcpy(&settings.conn_adv_interval, &conn_adv_int, sizeof(conn_adv_int));
        storage_intf_set(&settings);
        
        send_command_success(p_beacon_config);
        return;
        
    }
    else if(data[0] == DEVICE_SET_CUSTOM_BCN_DATA1)
    {
        memcpy(m_temp_beacon_data, &data[1], length - 1);
        m_temp_beacon_data_len = length - 1;
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_SET_CUSTOM_BCN_DATA2)
    {
        if(m_temp_beacon_data_len > MAX_CUSTOM_BEACON_DATA1)
        {
            m_temp_beacon_data_len = MAX_CUSTOM_BEACON_DATA1;
        }
        
        if(m_temp_beacon_data_len != MAX_CUSTOM_BEACON_DATA1)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t copy_len;
        if(length - 1 < MAX_CUSTOM_BEACON_DATA2)
        {
            copy_len = length - 1;
        }
        else
        {
            copy_len = MAX_CUSTOM_BEACON_DATA2;
        }
        memcpy(&m_temp_beacon_data[MAX_CUSTOM_BEACON_DATA1], &data[1], copy_len);
        m_temp_beacon_data_len += copy_len;
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_SET_CUSTOM_BCN_SAVE)
    {
        if(m_temp_beacon_data_len == 0)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        const default_app_settings_t * cur_settings = storage_intf_get();
        default_app_settings_t settings;
        memcpy(&settings, cur_settings, sizeof settings);
        memcpy(&settings.beacon_data, m_temp_beacon_data, m_temp_beacon_data_len);
        memset(m_temp_beacon_data, 0, sizeof m_temp_beacon_data);
        settings.beacon_data_len = m_temp_beacon_data_len;
        m_temp_beacon_data_len = 0;
        storage_intf_set(&settings);
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_CLEAR_CUSTOM_BCN_DATA)
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        default_app_settings_t settings;
        memcpy(&settings, cur_settings, sizeof settings);
        memset(&settings.beacon_data, 0, sizeof settings.beacon_data);
        settings.beacon_data_len = 0;
        storage_intf_set(&settings);
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_CONFIG_GPIO)
    {
        if(length != 4)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint32_t err_code;
        err_code = gpio_ctrl_config_pin((gpio_ctrl_pin_e)data[1], (nrf_gpio_pin_dir_t)data[2], (nrf_gpio_pin_pull_t)data[3]);
        if(err_code != NRF_SUCCESS)
        {
            response[0] = DEVICE_COMMAND_INVALID_PARAM;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_WRITE_GPIO)
    {
        if(length != 3)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response);
            return;
        }
        
        uint32_t err_code = gpio_ctrl_set_pin_state((gpio_ctrl_pin_e)data[1], data[2]);
        if(err_code != NRF_SUCCESS)
        {
            if(err_code == NRF_ERROR_INVALID_PARAM)
            {
                response[0] = DEVICE_COMMAND_INVALID_PARAM;
            }
            else if(err_code == NRF_ERROR_INVALID_STATE)
            {
                response[0] = DEVICE_COMMAND_INVALID_STATE;
            }
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }  
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(data[0] == DEVICE_READ_GPIO)
    {
        if(length != 2)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t state;
        uint32_t err_code = gpio_ctrl_get_pin_state((gpio_ctrl_pin_e)data[1], &state);        
        if(err_code != NRF_SUCCESS)
        {            
            response[0] = DEVICE_COMMAND_INVALID_PARAM;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t pin_response[3];
        pin_response[0] = data[0];
        pin_response[1] = data[1];
        pin_response[2] = state;
        
        send_command_response(p_beacon_config, pin_response, sizeof(pin_response));
        return;
    }
    else if(data[0] == DEVICE_CONFIG_GPIO_GET)
    {
        if(length != 2)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        gpio_pin_config_t config;
        uint32_t err = gpio_ctrl_pin_config_get((gpio_ctrl_pin_e)data[1], &config);
        if(NRF_SUCCESS != err)
        {
            response[0] = DEVICE_COMMAND_INVALID_PARAM;
            if(NRF_ERROR_INVALID_STATE == err)
            {
                response[0] = DEVICE_COMMAND_INVALID_STATE;
            }
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t config_response[4];
        config_response[0] = data[0];
        config_response[1] = config.mapping->pin_id;
        config_response[2] = config.dir;
        config_response[3] = config.pull;
        send_command_response(p_beacon_config, config_response, sizeof(config_response));
        return; 
    }
    else if(DEVICE_CONFIG_STATUS_PIN == data[0])
    {
        if(length != 3)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint32_t err = gpio_ctrl_config_status_pin((gpio_ctrl_pin_e)data[1], data[2]);
        if(NRF_SUCCESS != err)
        {
            response[0] = DEVICE_COMMAND_INVALID_PARAM;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        send_command_success(p_beacon_config);
        service_update_status_pin();
        return;
    }
    else if(DEVICE_UNCONFIG_STATUS_PIN == data[0])
    {
        gpio_ctrl_unconfig_status_pin();
        send_command_success(p_beacon_config);
        return;
    }
    else if(DEVICE_GET_STATUS_PIN_CONFIG == data[0])
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        if((gpio_ctrl_pin_e)cur_settings->status_pin == gpio_ctrl_invalid)
        {
            response[0] = DEVICE_COMMAND_INVALID_STATE;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t config_response[3];
        config_response[0] = data[0];
        config_response[1] = (uint8_t)cur_settings->status_pin;
        config_response[2] = cur_settings->status_pin_polarity;
        send_command_response(p_beacon_config, config_response, sizeof(config_response));
        return;
    }
    else if(DEVICE_READ_STATUS_PIN == data[0])
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        if((gpio_ctrl_pin_e)cur_settings->status_pin == gpio_ctrl_invalid)
        {
            response[0] = DEVICE_COMMAND_INVALID_STATE;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        uint8_t pin_state = 0xFF;
        uint32_t err = gpio_ctrl_get_pin_state((gpio_ctrl_pin_e)cur_settings->status_pin, &pin_state);
        if(NRF_SUCCESS != err)
        {
            response[0] = DEVICE_COMMAND_INVALID_STATE;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        if(cur_settings->status_pin_polarity == STATUS_POLARITY_ACTIVE_LOW)
        {
            pin_state ^= 0x01;
        }
        
        uint8_t config_response[2];
        config_response[0] = data[0];
        config_response[1] = pin_state;
        send_command_response(p_beacon_config, config_response, sizeof(config_response));
        return;
    }
    else if(DEVICE_BOOTLOADER_INFO_GET == data[0])
    {
        rig_firmware_info_t bl_info;
        uint32_t err;
        
        memset(&bl_info, 0, sizeof(bl_info));
        
        err = bootloader_info_read(&bl_info);
        if(err != NRF_SUCCESS)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        response[0] = DEVICE_BOOTLOADER_INFO_GET;
        uint8_t * p_bl_info_data = (uint8_t*)&bl_info;
        p_bl_info_data += sizeof(bl_info.magic_number_a) + sizeof(bl_info.size);
        memcpy(&response[1], p_bl_info_data, sizeof(rig_firmware_info_t) - 12);
        send_command_response(p_beacon_config, response, sizeof(response));
        return;
    }
    else if(DEVICE_SET_NAME == data[0])
    {
        if(length < 2 || length > MAX_DEVICE_NAME_LEN + 1)
        {
            response[0] = DEVICE_COMMAND_INVALID_LEN;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        char name[MAX_DEVICE_NAME_LEN + 1];
        
        memset(name, 0, sizeof(name));
        memcpy(name, &data[1], length - 1);
        
        bool valid_name = gap_validate_name(name);
        if(!valid_name)
        {
            response[0] = DEVICE_COMMAND_INVALID_DATA;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        const default_app_settings_t * cur_settings = storage_intf_get();
        default_app_settings_t settings;
        memcpy(&settings, cur_settings, sizeof(settings));
        memset(settings.device_name, 0, sizeof(settings.device_name));
        memcpy(settings.device_name, name, strlen(name));
        storage_intf_set(&settings);
        storage_intf_save();
        send_command_success(p_beacon_config);
        gap_update_device_name();
        return;
    }
    else if(DEVICE_GET_NAME == data[0])
    {
        char name_response[1 + MAX_DEVICE_NAME_LEN + 1];
        
        memset(name_response, 0, sizeof(name_response));
        
        name_response[0] = DEVICE_GET_NAME;
        const default_app_settings_t * cur_settings = storage_intf_get();
        memcpy(&name_response[1], cur_settings->device_name, sizeof(name_response) - 1);
        
        send_command_response(p_beacon_config, (uint8_t*)name_response, sizeof(name_response));
        return;
    }
    else if(DEVICE_SET_AT_HOTSWAP == data[0])
    {
        if(length != 2)
        {
            response[0] = DEVICE_COMMAND_INVALID_LEN;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        /* must be 0,1 */
        if(!(data[1] == 0x00 || data[1] == 0x01))
        {
            response[0] = DEVICE_COMMAND_INVALID_PARAM;
            send_command_response(p_beacon_config, response, sizeof(response));
            return;
        }
        
        /* did the value change? */
        bool rx_at_hotswap = data[1];
        const default_app_settings_t * cur_settings = storage_intf_get();
        if(rx_at_hotswap != cur_settings->at_hotswap_enabled)
        {
            default_app_settings_t settings;
            memcpy(&settings, cur_settings, sizeof(settings));
            
            settings.at_hotswap_enabled = rx_at_hotswap;
            storage_intf_set(&settings);
            storage_intf_save();
            
            /* reconfigure */
            /* this could potentially turn off at mode if the at mode pin is high... */
            sys_init_setup_at_mode_pin();
        }
        
        send_command_success(p_beacon_config);
        return;
    }
    else if(DEVICE_GET_AT_HOTSWAP == data[0])
    {
        const default_app_settings_t * cur_settings = storage_intf_get();
        response[0] = DEVICE_GET_AT_HOTSWAP;
        response[1] = cur_settings->at_hotswap_enabled;
        
        send_command_response(p_beacon_config, response, sizeof(response));
        return;
    }
    
    /* Check for bootloader command */
    {
        if(length == 4)
        {
            uint32_t command = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
            uint32_t err_code;
            if(command == BOOTLOADER_RESET_COMMAND)
            {
                /* Disable timeslotting */
                btle_hci_adv_enable(BTLE_ADV_DISABLE);
                btle_hci_adv_deinit();
                /* Force disconnect, disable softdevice, and then reset */
                sd_ble_gap_disconnect( p_beacon_config->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION );
                softdevice_handler_sd_disable();
                nrf_delay_us( 500 * 1000 );
                
                NVIC_SystemReset();
            }
            else if(command == BOOTLOADER_RESET_COMMAND_NEW)
            {
                /* This should only be called when a properly updated bootloader has been installed. */
                 /* Disable timeslotting */
                btle_hci_adv_enable(BTLE_ADV_DISABLE);
                btle_hci_adv_deinit();
                /* Force disconnect, disable softdevice, and then reset */
                sd_ble_gap_disconnect( p_beacon_config->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION );
                rig_firmware_info_t bl_info;
                uint32_t err;
                uint8_t gpregval = 0x00;
                
                memset(&bl_info, 0, sizeof(bl_info));
                
                err = bootloader_info_read(&bl_info);
                if(err == NRF_SUCCESS)
                {
                    if(bl_info.version_major == 3)
                    {
                        if(bl_info.version_minor >= 1)
                        {
                            gpregval = BOOTLOADER_DFU_START;
                        }
                    }
                    else if(bl_info.version_major > 3)
                    {
                        gpregval = BOOTLOADER_DFU_START;
                    }
                    else
                    {
                        gpregval = 0x00;
                    }
                }
                else
                {
                    gpregval = 0x00;
                }
                
                #ifdef S132
                    err_code = sd_power_gpregret_set(0, gpregval);
                #else
                    err_code = sd_power_gpregret_set(gpregval);
                #endif
                APP_ERROR_CHECK(err_code);
                
                softdevice_handler_sd_disable();
                nrf_delay_us( 500 * 1000 );
                
                NVIC_SystemReset();
            }
            else if(command == DEVICE_SYSTEM_RESET)
            {
                send_command_success(p_beacon_config);
                #ifdef S132
                    err_code = sd_power_gpregret_set(0, RESTART_APP);
                #else
                    err_code = sd_power_gpregret_set(RESTART_APP);
                #endif
                APP_ERROR_CHECK(err_code);
                nrf_delay_us( 500 * 1000 );
                
                NVIC_SystemReset();
            }
        }
    }
    
    send_command_invalid(p_beacon_config);
}
// ------------------------------------------------------------------------------

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_beacon_config      Band Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_beacon_config_t * p_beacon_config, ble_evt_t * p_ble_evt)
{
    p_beacon_config->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    m_conn_handle = p_beacon_config->conn_handle;
    memset(m_temp_beacon_data, 0, sizeof m_temp_beacon_data);
    m_temp_beacon_data_len = 0;
}
// ------------------------------------------------------------------------------


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_beacon_config      Band Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_beacon_config_t * p_beacon_config, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_beacon_config->conn_handle = BLE_CONN_HANDLE_INVALID;
    m_conn_handle = BLE_CONN_HANDLE_INVALID;
}
// ------------------------------------------------------------------------------

static uint32_t reset_db_value(uint16_t value_handle, uint8_t * data, uint16_t length)
{
    ble_gatts_value_t gatts_value;
    
    FILL_GATT_STRUCT(gatts_value, length, 0, data);
    return sd_ble_gatts_value_set( m_conn_handle, value_handle, &gatts_value );
}

/**@brief Function for handling the Write event.
 *
 * @param[in]   p_beacon_config      Band Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_beacon_config_t * p_beacon_config, ble_evt_t * p_ble_evt)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
       
    if( p_evt_write->handle == p_beacon_config->beacon_config_control_handles.cccd_handle )
    {
        if (p_beacon_config->is_notification_supported)
        {           
            if (p_beacon_config->evt_handler != NULL)
            {
                ble_beacon_config_evt_t evt;
                
                if (ble_srv_is_notification_enabled(p_evt_write->data))
                {
                    evt.evt_type = BLE_BEACON_CONFIG_EVT_CONTROL_NOTIFICATION_ENABLED;
                }
                else
                {
                    evt.evt_type = BLE_BEACON_CONFIG_EVT_CONTROL_NOTIFICATION_DISABLED;
                }

                p_beacon_config->evt_handler(p_beacon_config, &evt);
            }
        }
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_control_handles.value_handle )
    {
        handle_beacon_config_ctrl_write( p_beacon_config, p_evt_write->data, p_evt_write->len );
    } 
    else if( p_evt_write->handle == p_beacon_config->beacon_config_uuid_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( ble_uuid128_t ) )
        {
            uint8_t response[1] = { DEVICE_COMMAND_INVALID_LEN }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof response );
            reset_db_value(p_beacon_config->beacon_config_uuid_handles.value_handle, (uint8_t*)p_beacon_config->beacon_uuid.uuid128, sizeof(ble_uuid128_t));
            return;
        }
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_uuid_handles.value_handle, (uint8_t*)p_beacon_config->beacon_uuid.uuid128, sizeof(ble_uuid128_t));
            return;
        }
        
        memcpy(p_beacon_config->beacon_uuid.uuid128, p_evt_write->data, sizeof( ble_uuid128_t ));
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        memcpy(&settings.uuid, &p_beacon_config->beacon_uuid, sizeof(ble_uuid128_t));
        storage_intf_set(&settings);
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_major_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint16_t ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_major_handles.value_handle, (uint8_t*)&p_beacon_config->major, sizeof( uint16_t ));
            return;
        }
        
        memcpy( &p_beacon_config->major, p_evt_write->data, sizeof( uint16_t ) );
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.major = p_beacon_config->major;
        storage_intf_set(&settings);
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_minor_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint16_t ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_minor_handles.value_handle, (uint8_t*)&p_beacon_config->minor, sizeof( uint16_t ));
            return;
        }
        
        memcpy( &p_beacon_config->minor, p_evt_write->data, sizeof( uint16_t ) );
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.minor = p_beacon_config->minor;
        storage_intf_set(&settings);
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_interval_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint16_t ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_interval_handles.value_handle, (uint8_t*)&p_beacon_config->interval, sizeof( uint16_t ));
            return;
        }
        
        uint16_t interval = 0;
        memcpy( &interval, p_evt_write->data, sizeof( uint16_t ) );
        
        if(interval > MAX_ADV_INTERVAL || interval < MIN_ADV_INTERVAL)
        {
            uint8_t response[1] = { DEVICE_COMMAND_INVALID_DATA };
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_interval_handles.value_handle, (uint8_t*)&p_beacon_config->interval, sizeof( uint16_t ));
            return;
        }
        
        memcpy( &p_beacon_config->interval, &interval, sizeof( uint16_t ) );
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.adv_interval = p_beacon_config->interval;
        storage_intf_set(&settings);
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_tx_power_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint8_t ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_tx_power_handles.value_handle, &p_beacon_config->beacon_tx_power, sizeof( uint8_t ));
            return;
        }
        
        int8_t valid_pwr[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };
        int8_t tx_power = (int8_t)p_evt_write->data[0];
        uint8_t idx = 0;
        for(idx = 0; idx < sizeof(valid_pwr); idx++)
        {
            if(tx_power == valid_pwr[idx])
                break;
        }
        
        if(idx == sizeof(valid_pwr))
        {
            uint8_t response[1] = { DEVICE_COMMAND_INVALID_DATA };
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_tx_power_handles.value_handle, &p_beacon_config->beacon_tx_power, sizeof( uint8_t ));
            return;
        }
        
        memcpy( &p_beacon_config->beacon_tx_power, p_evt_write->data, sizeof( uint8_t ) );
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.beacon_tx_power = p_beacon_config->beacon_tx_power;
        storage_intf_set(&settings);
        
    }
    else if( p_evt_write->handle == p_beacon_config->beacon_config_enable_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint8_t ) )
            return;
        
        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_enable_handles.value_handle, &p_beacon_config->enable, sizeof( uint8_t ));
            return;
        }
        
        if(p_evt_write->data[0] >= 1)
        {
            p_beacon_config->enable = true;
        }
        else
        {
            p_beacon_config->enable = false;
        }
        
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.enable = p_beacon_config->enable;
        storage_intf_set(&settings);
    }
		else if( p_evt_write->handle == p_beacon_config->beacon_config_connectable_tx_power_handles.value_handle )
    {
        if( p_evt_write->len != sizeof( uint8_t ) )
            return;

        if( lock_is_locked() )
        {
            uint8_t response[1] = { DEVICE_LOCKED }; 
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_connectable_tx_power_handles.value_handle, &p_beacon_config->connectable_tx_power, sizeof( uint8_t ));
            return;
        }
        
        int8_t valid_pwr[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };
        int8_t tx_power = (int8_t)p_evt_write->data[0];
        uint8_t idx = 0;
        for(idx = 0; idx < sizeof(valid_pwr); idx++)
        {
            if(tx_power == valid_pwr[idx])
                break;
        }
        
        if(idx == sizeof(valid_pwr))
        {
            uint8_t response[1] = { DEVICE_COMMAND_INVALID_DATA };
            ble_beacon_config_send_notification(p_beacon_config, p_beacon_config->beacon_config_control_handles.value_handle, response, sizeof(response));
            reset_db_value(p_beacon_config->beacon_config_connectable_tx_power_handles.value_handle, &p_beacon_config->connectable_tx_power, sizeof( uint8_t ));
            return;
        }
        
        memcpy( &p_beacon_config->connectable_tx_power, p_evt_write->data, sizeof( uint8_t ) );
        cur_settings = storage_intf_get();
        memcpy(&settings, cur_settings, sizeof(settings));
        settings.connectable_tx_power = p_beacon_config->connectable_tx_power;
        storage_intf_set(&settings);
    }
}
// ------------------------------------------------------------------------------

void ble_beacon_config_on_ble_evt(ble_beacon_config_t * p_beacon_config, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_beacon_config, p_ble_evt);
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_beacon_config, p_ble_evt);
            break;
            
        case BLE_GATTS_EVT_WRITE:
            on_write(p_beacon_config, p_ble_evt);
            break;
            
        default:
            // No implementation needed.
            break;
    }
}
// ------------------------------------------------------------------------------

static uint32_t control_char_add(ble_beacon_config_t * p_beacon_config, const ble_beacon_config_init_t * p_beacon_config_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t            initial_val[20];
    
    memset(initial_val, 0x00, sizeof(initial_val));
    
    memset(&char_md, 0, sizeof(char_md));
    
    if (p_beacon_config->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));
    
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_beacon_config_init->beacon_config_control_char_attr_md.cccd_write_perm;
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    }
    
    set_char_md_properties( &char_md, &cccd_md, true, true, true, ( (p_beacon_config->is_notification_supported) ? true : false ), NULL );
    set_attr_md_properties( &attr_md, p_beacon_config_init->beacon_config_control_char_attr_md.read_perm, p_beacon_config_init->beacon_config_control_char_attr_md.write_perm, true );
    
    ble_uuid.type = p_beacon_config->uuid_type;
    ble_uuid.uuid = BEACON_CONFIG_CTRL_POINT_UUID;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = 10;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = 20;
    attr_char_value.p_value      = (uint8_t *)&initial_val;
    
    err_code = sd_ble_gatts_characteristic_add(p_beacon_config->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_beacon_config->beacon_config_control_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    return NRF_SUCCESS;
}
// ------------------------------------------------------------------------------

static uint32_t data_char_add(ble_beacon_config_t * p_beacon_config, const ble_beacon_config_init_t * p_beacon_config_init, beacon_config_char_config_t char_type)
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
    const ble_srv_cccd_security_mode_t *
                        security;
    
    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    if (p_beacon_config->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));
    
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_beacon_config_init->beacon_config_control_char_attr_md.cccd_write_perm;
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    }
    
    switch( char_type )
    {
        case Beacon_UUID:
            user_desc = BEACON_CONFIG_UUID_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_UUID_UUID;
            handles = &p_beacon_config->beacon_config_uuid_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(ble_uuid128_t);
            attr_char_value.max_len      = sizeof(ble_uuid128_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->beacon_uuid;
            break;
        case Beacon_Major:
            user_desc = BEACON_CONFIG_MAJOR_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_MAJOR_UUID;
            handles = &p_beacon_config->beacon_config_major_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint16_t);
            attr_char_value.max_len      = sizeof(uint16_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->major;
            break;
        case Beacon_Minor:
            user_desc = BEACON_CONFIG_MINOR_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_MINOR_UUID;
            handles = &p_beacon_config->beacon_config_minor_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint16_t);
            attr_char_value.max_len      = sizeof(uint16_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->minor;
            break;
        case Beacon_Interval:
            user_desc = BEACON_CONFIG_INTERVAL_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_INTERVAL_UUID;
            handles = &p_beacon_config->beacon_config_interval_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint16_t);
            attr_char_value.max_len      = sizeof(uint16_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->interval;
            break;
        case Beacon_TxPower:
            user_desc = BEACON_CONFIG_TX_POWER_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_TX_POWER_UUID;
            handles = &p_beacon_config->beacon_config_tx_power_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint8_t);
            attr_char_value.max_len      = sizeof(uint8_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->beacon_tx_power;
            break;
        case Beacon_Enable:
            user_desc = BEACON_CONFIG_ENABLE_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_ENABLE_UUID;
            handles = &p_beacon_config->beacon_config_enable_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint8_t);
            attr_char_value.max_len      = sizeof(uint8_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->enable;
            break;
				
		case Beacon_Connectable_TxPower:
            user_desc = BEACON_CONFIG_CONNECTABLE_TX_POWER_NAME_STR;
            ble_uuid.uuid = BEACON_CONFIG_CONNECTABLE_TX_POWER_UUID;
            handles = &p_beacon_config->beacon_config_connectable_tx_power_handles;
            security = &p_beacon_config_init->beacon_config_control_char_attr_md;
            attr_char_value.init_len     = sizeof(uint8_t);
            attr_char_value.max_len      = sizeof(uint8_t);
            attr_char_value.p_value      = (uint8_t *)&p_beacon_config->connectable_tx_power;
            break;
								
        case Beacon_Last:
        default:
            return NRF_ERROR_INVALID_DATA;
    }
    
    set_char_md_properties( &char_md, &cccd_md, true, true, true, true, user_desc );
    set_attr_md_properties( &attr_md, security->read_perm, security->write_perm, false );
    
    ble_uuid.type = p_beacon_config->uuid_type;
    
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_offs    = 0;
    
    
    err_code = sd_ble_gatts_characteristic_add(p_beacon_config->service_handle, 
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


uint32_t ble_beacon_config_init(ble_beacon_config_t * p_beacon_config, const ble_beacon_config_init_t * p_beacon_config_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    uint8_t config_id;
    const default_app_settings_t * app_settings;
    
    
    /* apply settings */
    app_settings = storage_intf_get();
    
    // Apply stored settings
    memcpy(&p_beacon_config->beacon_uuid, &app_settings->uuid, sizeof(ble_uuid128_t));
    p_beacon_config->major = app_settings->major;
    p_beacon_config->minor = app_settings->minor;
    p_beacon_config->interval = app_settings->adv_interval;
    p_beacon_config->beacon_tx_power = app_settings->beacon_tx_power;
    p_beacon_config->enable = app_settings->enable;
		p_beacon_config->connectable_tx_power = app_settings->connectable_tx_power;
    
    // Initialize service structure
    p_beacon_config->evt_handler               = p_beacon_config_init->evt_handler;
    p_beacon_config->conn_handle               = BLE_CONN_HANDLE_INVALID;
    m_conn_handle                              = BLE_CONN_HANDLE_INVALID;
    p_beacon_config->is_notification_supported = p_beacon_config_init->support_notification;
    
    // Add service
    ble_uuid.uuid = ((ble_beacon_config_service_uuid128.uuid128[12]) | (ble_beacon_config_service_uuid128.uuid128[13] << 8));
    
    err_code = sd_ble_uuid_vs_add(&ble_beacon_config_service_uuid128, &(ble_uuid.type));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    m_ble_beacon_config_uuid_type = ble_uuid.type;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_beacon_config->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    p_beacon_config->uuid_type = ble_uuid.type;
    
    err_code = control_char_add(p_beacon_config, p_beacon_config_init);
    if( err_code != NRF_SUCCESS)
        return err_code;
    
    for(config_id = 0; config_id < (uint8_t)Beacon_Last; config_id++)
    {
        err_code = data_char_add(p_beacon_config, p_beacon_config_init, (beacon_config_char_config_t)config_id);
        if( err_code != NRF_SUCCESS)
            return err_code;
    }

    return NRF_SUCCESS;
}

uint8_t ble_beacon_set_major(uint16_t val)
{
	uint16_t len = sizeof(val);
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    uint16_t beacon_major_value_handle = ptr->beacon_config_major_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_major_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_minor(uint16_t val)
{
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	uint16_t len = sizeof(val);
    uint16_t beacon_minor_value_handle = ptr->beacon_config_minor_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_minor_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_uuid(uint8_t *data, uint16_t n)
{
	if (n != 16) {
		return 2;
	}
	uint16_t len = n;
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
    uint16_t beacon_uuid_value_handle = ptr->beacon_config_uuid_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, data);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_uuid_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_beacon_tx_power(uint8_t val)
{
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	uint16_t len = sizeof(val);
    uint16_t beacon_txpower_value_handle = ptr->beacon_config_tx_power_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_txpower_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_ad_interval(uint16_t val)
{
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	uint16_t len = sizeof(val);
    uint16_t beacon_adint_value_handle = ptr->beacon_config_interval_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_adint_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_enable(uint8_t val)
{
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	uint16_t len = sizeof(val);
    uint16_t beacon_enable_value_handle = ptr->beacon_config_enable_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
	uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_enable_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ble_beacon_set_connectable_tx_power(uint8_t val)
{
	ble_beacon_config_t *ptr = services_get_beacon_config_obj();
	if (ptr == NULL) {
		return 3;
	}
    
    if(m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return 0;
    }
    
	uint16_t len = sizeof(val);
    uint16_t beacon_connectable_txpower_value_handle = ptr->beacon_config_connectable_tx_power_handles.value_handle;
    
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, len, 0, (uint8_t*)&val);
    uint16_t err_code = sd_ble_gatts_value_set(m_conn_handle, beacon_connectable_txpower_value_handle, &gatts_value);
	if (err_code != NRF_SUCCESS) {
		return 1;
	} else {
		return 0;
	}
}

// ------------------------------------------------------------------------------

uint8_t ble_beacon_config_get_uuid_type( void )
{
    return m_ble_beacon_config_uuid_type;
}

uint32_t ble_beacon_config_send_notification(const ble_beacon_config_t * p_beacon_config, uint16_t value_handle, uint8_t * data, uint16_t length )
{
    uint32_t err_code = NRF_SUCCESS;
    
     // Update database
    ble_gatts_value_t gatts_value;
    FILL_GATT_STRUCT(gatts_value, length, 0, data);
    err_code = sd_ble_gatts_value_set(m_conn_handle,
                                      value_handle,
                                      &gatts_value);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Send value if connected and notifying
    if ((p_beacon_config->conn_handle != BLE_CONN_HANDLE_INVALID) && p_beacon_config->is_notification_supported)
    {
        ble_gatts_hvx_params_t hvx_params;
        
        memset(&hvx_params, 0, sizeof(hvx_params));
        
        hvx_params.handle   = value_handle;
        hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset   = 0;
        hvx_params.p_len    = &length;
        hvx_params.p_data   = data;
        
        err_code = sd_ble_gatts_hvx(p_beacon_config->conn_handle, &hvx_params);
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

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

static void set_attr_md_properties( ble_gatts_attr_md_t * attr_md, ble_gap_conn_sec_mode_t read_perm, ble_gap_conn_sec_mode_t write_perm, bool vlen)
{
    memset(attr_md, 0, sizeof(ble_gatts_attr_md_t));

    attr_md->read_perm  = read_perm;
    attr_md->write_perm = write_perm;
    attr_md->vloc       = BLE_GATTS_VLOC_STACK;
    attr_md->rd_auth    = 0;
    attr_md->wr_auth    = 0;
		attr_md->vlen       = (vlen) ? 1 : 0;
}
// ------------------------------------------------------------------------------
