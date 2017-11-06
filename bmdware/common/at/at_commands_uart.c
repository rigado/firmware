/** @file at_commands_uart.c
*
* @brief AT command processing for uart commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "nrf_error.h"
#include "storage_intf.h"
#include "ble_nus.h"

#include "at_commands.h"

static uint32_t baud_rate_from_str(char * data);
static uint32_t handle_uart_on_off_setting(uint8_t * value, uint32_t storage_location_offset, uint8_t (*ble_update_func)(uint8_t val));

static uint32_t uart_command_baud(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    if(query)
    {
        char rsp_buf[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%08lx", storage_intf_get()->baud_rate);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    uint32_t br = baud_rate_from_str((char *)argv[1]);
    if (br == 0) {
        return AT_RESULT_ERROR;
    }

    cur_settings = storage_intf_get();
    memcpy(&settings, cur_settings, sizeof(settings));
    settings.baud_rate = br;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    if(ble_nus_set_baudrate(br) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    return AT_RESULT_OK;
}

static uint32_t uart_command_flow_control(uint8_t argc, char ** argv, bool query)
{   
    if(query)
    {
        char rsp_buf[3] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", storage_intf_get()->flow_control);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    uint32_t flow_offset = offsetof(default_app_settings_t, flow_control);
    return handle_uart_on_off_setting((uint8_t*)argv[1], flow_offset, ble_nus_set_flow_control);
}

static uint32_t uart_command_parity(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        char rsp_buf[3] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", storage_intf_get()->parity);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    uint32_t parity_offset = offsetof(default_app_settings_t, parity);
    return handle_uart_on_off_setting((uint8_t*)argv[1], parity_offset, ble_nus_set_parity);
}

static uint32_t uart_command_enable(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        char rsp_buf[3] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", storage_intf_get()->uart_enable);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    uint32_t enable_offset = offsetof(default_app_settings_t, uart_enable);
    return handle_uart_on_off_setting((uint8_t*)argv[1], enable_offset, ble_nus_set_enable);
}

static const at_command_t uart_cmds[] = {
    { "ubr", 0, 1, true, uart_command_baud },
    { "ufc", 0, 1, true, uart_command_flow_control },
    { "upar", 0, 1, true, uart_command_parity },
    { "uen", 0, 1, true, uart_command_enable },
    
    /* List Terminator */
    { NULL },
};
//AT_STATIC_REGISTER_CMDS(uart_cmds);

void at_commands_uart_init(void)
{
    at_commands_register(uart_cmds);
}

static uint32_t baud_rate_from_str(char *br)
{
	if (strcmp(br, "1200") == 0)
		return 1200;
	if (strcmp(br, "2400") == 0)
		return 2400;
	if (strcmp(br, "4800") == 0)
		return 4800;
	if (strcmp(br, "9600") == 0)
		return 9600;
	if (strcmp(br, "19200") == 0)
		return 19200;
	if (strcmp(br, "38400") == 0)
		return 38400;
	if (strcmp(br, "57600") == 0)
		return 57600;
	if (strcmp(br, "115200") == 0)
		return 115200;
    if (strcmp(br, "230400") == 0)
        return 230400;
    if (strcmp(br, "460800") == 0)
        return 460800;
    if (strcmp(br, "921600") == 0)
        return 921600;
    if (strcmp(br, "1000000") == 0)
        return 1000000;
    
	return 0;
}

static uint32_t handle_uart_on_off_setting(uint8_t * value, uint32_t storage_location_offset, uint8_t (*ble_update_func)(uint8_t val))
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    uint8_t * storage_val;
    uint8_t len = strlen((char *)value);
    if (len > 2) 
    {
        return AT_RESULT_ERROR;
    }
    uint8_t val = (uint8_t)strtol((char*)value, NULL, 16);
    cur_settings = storage_intf_get();
    memcpy(&settings, cur_settings, sizeof(settings));
    uint32_t address = (uint32_t)&settings + storage_location_offset;
    storage_val = (uint8_t*)(address);
    *storage_val = val;
    
    storage_intf_set(&settings);
    at_util_save_stored_data();
    if(ble_update_func(val) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    return AT_RESULT_OK;
}
