#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "nrf_error.h"
#include "storage_intf.h"
#include "advertising.h"
#include "ble_beacon_config.h"

#include "at_commands.h"

#define BEACON_UUID_STR_LEN         (32) //16 hex digit pairs
#define BEACON_UUID_LEN             (16) //128-bit UUID

#define CUSTOM_BEACON_MAX_STR_LEN   (CUSTOM_BEACON_DATA_MAX_LEN * 2)

/* Utility Functions */
static void uint8_to_char(uint8_t num, char * high, char * low);
static void uuid_to_string(uint8_t *buf, uint8_t buf_len, ble_uuid128_t uuid);
static void custom_beacon_to_string(uint8_t *buf, uint8_t buf_len, const uint8_t * beacon_data, uint8_t beacon_data_len);

static uint32_t beacon_command_uuid(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    uint8_t udata[BEACON_UUID_LEN];
    
    cur_settings = storage_intf_get();
    if(query)
    {
        uint8_t rsp_buf[BEACON_UUID_STR_LEN + 1];
        memset(rsp_buf, 0, sizeof(rsp_buf));
        uuid_to_string(rsp_buf, sizeof(rsp_buf), cur_settings->uuid);
        at_util_uart_put_string(rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], BEACON_UUID_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }

    memcpy(&settings, cur_settings, sizeof(settings));
    at_util_hex_str_to_array(argv[1], settings.uuid.uuid128, BEACON_UUID_LEN);
    
    storage_intf_set(&settings);
    at_util_save_stored_data();
    if(ble_beacon_set_uuid(udata, sizeof(ble_uuid128_t)) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_major(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%04x", (uint16_t)cur_settings->major);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], SIXTEEN_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t offset = offsetof(default_app_settings_t, major);
    uint32_t value = 0;
    at_util_set_stroage_val((const uint8_t*)argv[1], sizeof(settings.major), offset, &value);
    
    uint32_t err_code = ble_beacon_set_major((uint16_t)value);
    if(err_code != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_minor(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%04x", (uint16_t)cur_settings->minor);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], SIXTEEN_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t offset = offsetof(default_app_settings_t, minor);
    uint32_t value = 0;
    at_util_set_stroage_val((const uint8_t*)argv[1], sizeof(settings.minor), offset, &value);
    uint32_t err_code = ble_beacon_set_minor((uint16_t)value);
    if(err_code != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_advint(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%04x", (uint16_t)cur_settings->adv_interval);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], SIXTEEN_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t offset = offsetof(default_app_settings_t, adv_interval);
    uint32_t value = 0;
    at_util_set_stroage_val((const uint8_t*)argv[1], sizeof(settings.adv_interval), offset, &value);
    uint32_t err_code = ble_beacon_set_ad_interval((uint16_t)value);
    if(err_code != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_txpwr(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", (uint8_t)cur_settings->beacon_tx_power);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], EIGHT_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t offset = offsetof(default_app_settings_t, beacon_tx_power);
    uint32_t value = 0;
    at_util_set_stroage_val((const uint8_t*)argv[1], sizeof(settings.beacon_tx_power), offset, &value);
    
    if(!at_util_validate_power((int8_t)value))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t err_code = ble_beacon_set_beacon_tx_power((int8_t)value);
    if(err_code != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_enable(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", cur_settings->enable);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], EIGHT_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t offset = offsetof(default_app_settings_t, enable);
    uint32_t value = 0;
    at_util_set_stroage_val((const uint8_t*)argv[1], sizeof(settings.enable), offset, &value);
    uint32_t err_code = ble_beacon_set_enable((uint8_t)value);
    if(err_code != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    advertising_restart();
    return AT_RESULT_OK;
}

static uint32_t beacon_command_custom(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    uint8_t length = strlen((char *)argv[1]);
    
    cur_settings = storage_intf_get();
    if(query)
    {
        uint8_t rsp_buf[CUSTOM_BEACON_MAX_STR_LEN + 1];
        memset(rsp_buf, 0, sizeof(rsp_buf));
        custom_beacon_to_string(rsp_buf, sizeof(rsp_buf), cur_settings->beacon_data, cur_settings->beacon_data_len);
        at_util_uart_put_string(rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(length == 0 || length > CUSTOM_BEACON_MAX_STR_LEN || (length % 2) != 0)
    {
        return AT_RESULT_ERROR;
    }
    
    if(!at_util_validate_hex_str(argv[1], length))
    {
        return AT_RESULT_ERROR;
    }
    
    memcpy(&settings, cur_settings, sizeof(default_app_settings_t));
    
    uint8_t array_len = length / 2;
    uint8_t len_to_unhex = CUSTOM_BEACON_DATA_MAX_LEN;
    if(array_len < CUSTOM_BEACON_DATA_MAX_LEN)
        len_to_unhex = array_len;
    
    at_util_hex_str_to_array(argv[1], settings.beacon_data, len_to_unhex);
    
    settings.beacon_data_len = length / 2;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_custom_clear(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    cur_settings = storage_intf_get();
    memcpy(&settings, cur_settings, sizeof(default_app_settings_t));
    memset(&settings.beacon_data, 0, sizeof(settings.beacon_data));
    settings.beacon_data_len = 0;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
   if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

static uint32_t beacon_command_set_cal(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    uint8_t data_len = strlen((const char *)argv[1]);
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[6] = { 0, 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x %02x", (uint8_t)cur_settings->rssi_cal.tx_power, (uint8_t)cur_settings->rssi_cal.rssi);
        at_util_uart_put_string((const uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(data_len > 4) {
        return AT_RESULT_ERROR;
    }
    
    uint8_t temp; 
    at_util_hex_str_to_array(&argv[1][0], &temp, 1);
    int8_t pwr = (int8_t)temp;
    at_util_hex_str_to_array(&argv[1][2], &temp, 1);
    int8_t rssi = (int8_t)temp;
    if(!at_util_validate_power(pwr))
    {
        return AT_RESULT_ERROR;
    }
    
	memcpy(&settings, cur_settings, sizeof(settings));
    settings.rssi_cal.rssi = rssi;
    settings.rssi_cal.tx_power = pwr;
    storage_intf_set(&settings);
	at_util_save_stored_data();
    
    if(cur_settings->enable)
    {
        advertising_restart();
    }
    return AT_RESULT_OK;
}

const at_command_t beacon_cmds[] = {
    { "buuid", 0, 1, true, beacon_command_uuid },
    { "uuid", 0, 1, true, beacon_command_uuid },
    { "bmjid", 0, 1, true, beacon_command_major },
    { "mjid", 0, 1, true, beacon_command_major },
    { "bmnid", 0, 1, true, beacon_command_minor },
    { "mnid", 0, 1, true, beacon_command_minor },
    { "badint", 0, 1, true, beacon_command_advint },
    { "adint", 0, 1, true, beacon_command_advint },
    { "btxpwr", 0, 1, true, beacon_command_txpwr },
    { "ben", 0, 1, true, beacon_command_enable },
    { "cusbcn", 0, 1, true, beacon_command_custom },
    { "cbclr", 0, 0, true, beacon_command_custom_clear },
    { "bcal", 0, 1, true, beacon_command_set_cal },
    
    /* List terminator */
    { NULL },
};
//AT_STATIC_REGISTER_CMDS(beacon_cmds);

void at_commands_beacon_init(void)
{
    at_commands_register(beacon_cmds);
}

static void uint8_to_char(uint8_t num, char * high, char * low)
{
    uint8_t lookup[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    uint8_t upper;
    uint8_t lower;
                         
    upper = (num & 0xF0) >> 4;
    lower = num & 0x0F;
    
    *high = lookup[upper];
    *low = lookup[lower];
}

static void uuid_to_string(uint8_t *buf, uint8_t buf_len, ble_uuid128_t uuid)
{
    uint8_t index;
    uint8_t buf_idx = 0;
    char high;
    char low;
    
    memset(buf, 0, buf_len);
    for(index = 0; index < 16; index++)
    {
        uint8_to_char(uuid.uuid128[index], &high, &low);
        buf[buf_idx++] = high;
        buf[buf_idx++] = low;
    }
}

static void custom_beacon_to_string(uint8_t *buf, uint8_t buf_len, const uint8_t * beacon_data, uint8_t beacon_data_len)
{
    uint8_t index;
    uint8_t buf_idx = 0;
    char high;
    char low;
    
    memset(buf, 0, buf_len);
    for(index = 0; index < beacon_data_len; index++)
    {
        uint8_to_char(beacon_data[index], &high, &low);
        buf[buf_idx++] = high;
        buf[buf_idx++] = low;
    }
}
