/** @file at_commands_misc.c
*
* @brief AT command processing for miscellaneous commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "nrf.h"
#include "nrf_error.h"
#include "nrf_delay.h"
#include "nrf_soc.h"
#include "storage_intf.h"
#include "version.h"
#include "ble_beacon_config.h"
#include "advertising.h"
#include "app_util.h"
#include "rig_firmware_info.h"
#include "bootloader_info.h"
#include "advertising.h"
#include "gap.h"
#include "sys_init.h"

#include "at_commands.h"

#define BOOTLOADER_DFU_START    (0xB1)
#define RESTART_APP             (0xC5)

#define MIN_PASSWORD_LEN        (1)
#define MAX_PASSWORD_LEN        (19)

static uint32_t misc_command_version(uint8_t argc, char ** argv, bool query)
{
    if(!query)
    {
        return AT_RESULT_ERROR;
    }
    
    at_util_uart_put_string((uint8_t*)FIRMWARE_VERSION_STRING);
    return AT_RESULT_QUERY;
}

static uint32_t misc_command_bootloader_version(uint8_t argc, char ** argv, bool query)
{
    if(!query)
    {
        return AT_RESULT_ERROR;
    }
    
    uint32_t error;
    rig_firmware_info_t bl_info;
    uint8_t buf[25];
    
    memset(buf, 0, sizeof(buf));
    memset(&bl_info, 0, sizeof(bl_info));
    error = bootloader_info_read(&bl_info);
    if(error != NRF_SUCCESS)
    {
        at_util_uart_put_string((const uint8_t*)"unavailable");
    }
    else
    {
        sprintf((char*)buf, "rigdfu_%d.%d.%d-%s", bl_info.version_major, bl_info.version_minor, bl_info.version_rev, 
            (bl_info.version_type == VERSION_TYPE_RELEASE) ? "release" : "debug");
        at_util_uart_put_string(buf);
    }
    
    return AT_RESULT_QUERY;
}

static uint32_t misc_command_protocol_version(uint8_t argc, char ** argv, bool query)
{
    if(!query)
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t buf[3] = { 0, 0, 0 };
    sprintf((char*)buf, "%02x", API_PROTOCOL_VERSION);
    at_util_uart_put_string(buf);
    return AT_RESULT_QUERY;
}

static uint32_t misc_command_hw_info(uint8_t argc, char ** argv, bool query)
{
    if(!query)
    {
        return AT_RESULT_ERROR;
    }

    char rsp_buf[20];
    memset(rsp_buf, 0, sizeof(rsp_buf));
#ifdef NRF52    
    uint32_t flash = NRF_FICR->INFO.FLASH;
    uint32_t ram = NRF_FICR->INFO.RAM;
    sprintf(rsp_buf, "NRF52/%lu/%lu", flash, ram);
#elif defined(NRF51)
    uint32_t flash = (NRF_FICR->CODEPAGESIZE * NRF_FICR->CODESIZE) / 1024;
    uint32_t ram = (NRF_FICR->NUMRAMBLOCK * NRF_FICR->SIZERAMBLOCKS) / 1024;
    sprintf(rsp_buf, "NRF51/%lu/%lu", flash, ram);
#else
    sprintf(rsp_buf, "Unknown Part!");
#endif
    at_util_uart_put_string((uint8_t*)rsp_buf);
    
    return AT_RESULT_QUERY;
}

static uint32_t misc_command_connectable_txpwr(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[3] = { 0, 0, 0 };
        sprintf(rsp_buf, "%02x", (uint8_t)cur_settings->connectable_tx_power);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    int8_t pwr = (int8_t)strtol(argv[1], NULL, 16);   
    if(!at_util_validate_power(pwr))
    {
        return AT_RESULT_ERROR;
    }
    
    
    memcpy(&settings, cur_settings, sizeof(settings));
    settings.connectable_tx_power = pwr;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
    if(ble_beacon_set_connectable_tx_power(pwr) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    return AT_RESULT_OK;
}

static uint32_t misc_command_defaults(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    storage_intf_set(&default_settings);
	at_util_save_stored_data();
    gap_update_device_name();
    advertising_restart();
    return AT_RESULT_OK;
}

static uint32_t misc_command_unlock(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t len = strlen((char *)argv[1]);
	
	//length check
	if (len >= MAX_PASSWORD_LEN)
	{
        return AT_RESULT_ERROR;
    }
    
    lock_password_t pw;
    memset(&pw,0,sizeof(pw));
    memcpy(pw.password, argv[1], len);
    
    if(!lock_clear(&pw)) 
    {
        return AT_RESULT_LOCKED;
    }
	
    return AT_RESULT_OK;
}

static uint32_t misc_command_set_password(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t len = strlen((char *)argv[1]);
	
	//length check
	if (len < MIN_PASSWORD_LEN || len > MAX_PASSWORD_LEN) 
	{
		return AT_RESULT_ERROR;
	}

    lock_password_t pw;
    memset(&pw, 0, sizeof(pw));
    memcpy(pw.password, argv[1], len);
    
    if(!lock_set_password(&pw)) 
    {
        return AT_RESULT_ERROR;
    }	
    else
    {
        lock_set();
    }

    at_util_save_stored_data();    
    return AT_RESULT_OK;
}

static uint32_t misc_command_name(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings = storage_intf_get();
    if(query)
    {
        at_util_uart_put_string(cur_settings->device_name);
        return AT_RESULT_QUERY;
    }
    
    uint32_t len = strlen(argv[1]);
    if(MAX_DEVICE_NAME_LEN < len)
    {
        return AT_RESULT_ERROR;
    }
    
    bool valid_name = gap_validate_name(argv[1]);
    if(!valid_name)
    {
        return AT_RESULT_ERROR;
    }
    
    default_app_settings_t settings;
    memcpy(&settings, cur_settings, sizeof(settings));
    memset(settings.device_name, 0, sizeof(settings.device_name));
    memcpy(settings.device_name, argv[1], len);
    storage_intf_set(&settings);
    storage_intf_save();
    gap_update_device_name();
    
    advertising_restart();
    return AT_RESULT_OK;
}

static uint32_t misc_command_reset(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    at_util_print_ok_response();
    nrf_delay_ms(500);
    NVIC_SystemReset();
    
    return AT_RESULT_OK;
}

static uint32_t misc_command_start_bootloader(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    at_util_print_ok_response();
    
    rig_firmware_info_t bl_info;
    uint32_t err = bootloader_info_read(&bl_info);
    
    uint8_t gpregval = 0;
    
    if(NRF_SUCCESS == err)
    {
        if(bl_info.version_major == 3 && bl_info.version_minor >= 1)
        {
            gpregval = BOOTLOADER_DFU_START;
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
        (void)sd_power_gpregret_set(0,gpregval);
    #else
        (void)sd_power_gpregret_set(gpregval);
    #endif
    
    nrf_delay_ms(500);
    NVIC_SystemReset();
    
    return AT_RESULT_OK;
}

static uint32_t misc_command_restart_app(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    at_util_print_ok_response();
    
     #ifdef S132
        (void)sd_power_gpregret_set(0,RESTART_APP);
    #else
        (void)sd_power_gpregret_set(RESTART_APP);
    #endif
    
    nrf_delay_ms(500);
    NVIC_SystemReset();
    
    return AT_RESULT_OK;
}

static uint32_t misc_set_connectable_adv(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[3] = { 0, 0, 0 };
        sprintf(rsp_buf, "%02x", (uint8_t)cur_settings->connectable_adv_enabled);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], EIGHT_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    bool prev_enabled = cur_settings->connectable_adv_enabled;
    uint8_t enabled = (int8_t)strtol(argv[1], NULL, 16);
    
    memcpy(&settings, cur_settings, sizeof(settings));
    settings.connectable_adv_enabled = (enabled > 0);
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
    if(prev_enabled == true && enabled == 0)
    {
        advertising_stop_connectable_adv();
    }
    else if(prev_enabled == false && (enabled > 0))
    {
        advertising_start();
    }
    
    return AT_RESULT_OK;
}

static uint32_t misc_command_connectable_ad_int(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    if(query)
    {
        char rsp_buf[5] = { 0, 0, 0, 0, 0 };
        sprintf(rsp_buf, "%04x", (uint16_t)cur_settings->conn_adv_interval);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(!at_util_validate_input_str((const uint8_t*)argv[1], SIXTEEN_BIT_STR_LEN))
    {
        return AT_RESULT_ERROR;
    }
    
    uint16_t prev_interval = cur_settings->conn_adv_interval;
    uint16_t ad_interval = (uint16_t)strtol(argv[1], NULL, 16);
    
    memcpy(&settings, cur_settings, sizeof(settings));
    settings.conn_adv_interval = ad_interval;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
    if(prev_interval != ad_interval)
    {
        advertising_restart();
    }
    
    return AT_RESULT_OK;
}

static uint32_t misc_command_hotswap_enable(uint8_t argc, char ** argv, bool query)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    
    if(query)
    {
        char rsp_buf[4]; /* 0-255, really should be 1,0 */
        memset(rsp_buf,0,sizeof(rsp_buf));
        
        sprintf(rsp_buf, "%d", cur_settings->at_hotswap_enabled);
        at_util_uart_put_string((uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    /* expect '1' or '0' only */
    if(strlen(argv[1]) != 1 
        || (argv[1][0] != '1' && argv[1][0] != '0'))
    {
        return AT_RESULT_ERROR;
    }
    
    bool prev_hotswap = cur_settings->at_hotswap_enabled;
    bool new_hotswap = false;
    
    if(argv[1][0] == '1')
    {
        new_hotswap = true;
    }
    
    memcpy(&settings, cur_settings, sizeof(settings));
    settings.at_hotswap_enabled = new_hotswap;
    storage_intf_set(&settings);
    at_util_save_stored_data();
    
    if(prev_hotswap != new_hotswap)
    {
        /* this could potentially turn off at mode if the at mode pin is high... */
        sys_init_setup_at_mode_pin();
    }
    
    return AT_RESULT_OK;
}


static uint32_t misc_command_mac(uint8_t argc, char ** argv, bool query)
{
    if(!query)
    {
        return AT_RESULT_ERROR;
    }
    else
    {
        /* format: "xx:xx:xx:xx:xx:xx\0" */
        uint8_t buf[20];
        uint32_t err_code;
        ble_gap_addr_t addr;
        
        err_code = sd_ble_gap_addr_get(&addr);
        if(err_code != NRF_SUCCESS)
        {
            memset(&addr,0,sizeof(addr));
        }
        
        snprintf((void*)buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                        addr.addr[5],
                        addr.addr[4],
                        addr.addr[3],
                        addr.addr[2],
                        addr.addr[1],
                        addr.addr[0]);
            
        at_util_uart_put_string(buf);
        return AT_RESULT_QUERY;
    }
}


const at_command_t default_cmds[] = {
    { "ver",        0, 0, false,    misc_command_version },
    { "blver",      0, 0, false,    misc_command_bootloader_version },
    { "pver",       0, 0, false,    misc_command_protocol_version },
    { "ctxpwr",     0, 1, true,     misc_command_connectable_txpwr },
    { "cadint",     0, 1, true,     misc_command_connectable_ad_int },
    { "defaults",   0, 0, false,    misc_command_defaults },
    { "unlock",     1, 1, false,    misc_command_unlock },
    { "password",   1, 1, true,     misc_command_set_password },
    { "devrst",     0, 0, true,     misc_command_reset },
    { "stbl",       0, 0, true,     misc_command_start_bootloader },
    { "restart",    0, 0, true,     misc_command_restart_app },
    { "conadv",     0, 1, true,     misc_set_connectable_adv },
    { "hwinfo",     0, 0, false,    misc_command_hw_info },
    { "name",       0, 1, true,     misc_command_name },
    { "hotswap",    0, 1, true,     misc_command_hotswap_enable },
    { "mac",        0, 0, false,    misc_command_mac },
    
    /* List Terminator */
    { NULL },
};
//AT_STATIC_REGISTER_CMDS(default_cmds);

void at_commands_misc_init()
{
    at_commands_register(default_cmds);
}
