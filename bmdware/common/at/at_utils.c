/** @file at_utils.c
*
* @brief AT command utility functions
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "nrf_error.h"
#include "app_error.h"

#ifdef NRF52_UARTE
    #include "simple_uarte.h"
#else
    #include "simple_uart.h"
#endif

#include "storage_intf.h"

#include "at_commands.h"

#ifdef NRF52_UARTE
    static char out_buf[200];
#endif

uint32_t at_util_uart_put_string(const uint8_t * str)
{
    if(str == NULL)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
#ifdef NRF52_UARTE
    sprintf(out_buf, "%s\n", str);
    simple_uarte_putstring((const uint8_t*)out_buf, NULL);
#else
    simple_uart_blocking_putstring(str);
    simple_uart_blocking_put('\n');
#endif
    
    return NRF_SUCCESS;
}

uint32_t at_util_uart_put_bytes(const uint8_t * const bytes, uint32_t len)
{
    if(bytes == NULL || len == 0)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
#ifdef NRF52_UARTE
    simple_uarte_put(bytes, len, NULL);
#else
    for(uint32_t i = 0; i < len; i++)
    {
        simple_uart_blocking_put(bytes[i]);
    }
#endif
    
    return NRF_SUCCESS;
}

static void at_util_print_response(const uint8_t * str)
{
#ifdef NRF52_UARTE
    simple_uarte_putstring(str, NULL);
#else
    simple_uart_blocking_putstring(str);
#endif
}

void at_util_print_ok_response(void)
{
    at_util_print_response((uint8_t*)"OK\n");
}

void at_util_print_error_response(void)
{
    at_util_print_response((uint8_t*)"ERR\n");
}

void at_util_print_unknown_response(void)
{
    at_util_print_response((uint8_t*)"???\n");
}

void at_util_print_locked_response(void)
{
    at_util_print_response((uint8_t*)"LOCKED\n");
}

uint32_t at_util_save_stored_data(void)
{
    uint32_t err_code;
   if( storage_intf_is_dirty() )
    {
        err_code = storage_intf_save();
        APP_ERROR_CHECK(err_code);
    }
    return 0;
}

void at_util_hex_str_to_array(const char * in_str, uint8_t * out_bytes, uint32_t out_bytes_len)
{
    const char * pos = in_str;
    for(uint8_t i = 0; i < out_bytes_len; i++)
    {
        uint8_t val = ((uint8_t)strtol((char []){*pos,0}, NULL, 16) << 4);
        pos++;
        val += ((uint8_t)strtol((char []){*pos,0}, NULL, 16));
        pos++;
        out_bytes[i] = val;
    }
}

bool at_util_validate_hex_str(const char * in_str, uint32_t in_str_len)
{
    bool result = true;
    uint32_t end = (uint32_t)in_str + in_str_len;
    while(result && (uint32_t)in_str != end)
    {
        if(*in_str > 0x39) //if value is greater than '9'
        {
            if(*in_str < 0x61 || *in_str > 0x66)  //make sure value is ['a','f']
            {
                result = false;
            }
        }
        else if(*in_str < 0x30) //if value is less than '0'
        {
            result = false;
        }
        
        in_str++;
    }
    
    return result;
}

bool at_util_validate_input_str(const uint8_t * in_str, uint32_t req_str_len)
{
    /* Check length of string */
    uint32_t len = strlen((const char*)in_str);
    if(len > req_str_len || len == 0)
    {
        return false;
    }
    
    if(!at_util_validate_hex_str((const char*)in_str, len))
    {
        return false;
    }
    
    return true;
}

uint32_t at_util_set_stroage_val(const uint8_t * value, uint8_t val_size, uint32_t storage_offset, uint32_t * converted_value)
{
    const default_app_settings_t * cur_settings;
    default_app_settings_t settings;
    
    cur_settings = storage_intf_get();
    memcpy(&settings, cur_settings, sizeof(settings));
    if(val_size == 1)
    {
        uint8_t num_val = (uint8_t)strtol((const char*)value, NULL, 16);
        uint8_t * val = (uint8_t*)((uint32_t)&settings + storage_offset);
        *val = num_val;
        *converted_value = num_val;
    }
    else if(val_size == 2)
    {
        uint16_t num_val = (uint16_t)strtol((const char*)value, NULL, 16);
        uint16_t * val = (uint16_t*)((uint32_t)&settings + storage_offset);
        *val = num_val;
        *converted_value = num_val;
    }
    else if(val_size == 4)
    {
        uint32_t num_val = (uint32_t)strtol((const char*)value, NULL, 16);
        uint32_t * val = (uint32_t*)((uint32_t)&settings + storage_offset);
        *val = num_val;
        *converted_value = num_val;
    }
    else
    {
        *converted_value = 0;
        return AT_RESULT_ERROR;
    }
    
    storage_intf_set(&settings);
    storage_intf_save();
    
    return AT_RESULT_OK;
}

bool at_util_validate_power(int8_t power)
{
    int8_t valid_pwr[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };
    uint8_t idx = 0;
    for(idx = 0; idx < sizeof(valid_pwr); idx++)
    {
        if(power == valid_pwr[idx])
            break;
    }
    if(idx == sizeof valid_pwr)
    {
        return false;
    }
    
    return true;
}
