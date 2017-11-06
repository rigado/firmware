/** @file at_commands_gpio.c
*
* @brief AT command processing for GPIO commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdlib.h>
#include <stdio.h>

#include "nrf.h"
#include "nrf_error.h"
#include "gpio_ctrl.h"
#include "at_commands.h"
#include "storage_intf.h"
#include "service.h"

static bool validate_input_arguments(uint8_t argc, char ** argv);

static uint32_t gpio_command_config(uint8_t argc, char ** argv, bool query)
{    
    if(!validate_input_arguments(argc, argv))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t pin = (uint8_t)strtol(argv[1], NULL, 16);
    uint8_t dir = (uint8_t)strtol(argv[2], NULL, 16);
    uint8_t pull = (uint8_t)strtol(argv[3], NULL, 16);
    
    if(!IS_VALID_PIN(pin))
    {
        return AT_RESULT_ERROR;
    }
    
    if(dir != NRF_GPIO_PIN_DIR_INPUT && dir != NRF_GPIO_PIN_DIR_OUTPUT)
    {
        return AT_RESULT_ERROR;
    }
    
    if(pull != NRF_GPIO_PIN_NOPULL && pull != NRF_GPIO_PIN_PULLDOWN && pull != NRF_GPIO_PIN_PULLUP)
    {
        return AT_RESULT_ERROR;
    }
    
    if(gpio_ctrl_config_pin((gpio_ctrl_pin_e)pin, (nrf_gpio_pin_dir_t)dir, (nrf_gpio_pin_pull_t)pull) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    return AT_RESULT_OK;
}

static uint32_t gpio_command_set(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    if(!validate_input_arguments(argc, argv))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t pin = (uint8_t)strtol(argv[1], NULL, 16);
    uint8_t state = (uint8_t)strtol(argv[2], NULL, 16);
    
    if(!IS_VALID_PIN(pin))
    {
        return AT_RESULT_ERROR;
    }
    
    /* Any non-zero value will enable the pin */
    if(state > 0)
        state = 1;
    
    if(gpio_ctrl_set_pin_state((gpio_ctrl_pin_e)pin, state) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    return AT_RESULT_OK;
}

static uint32_t gpio_command_read(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    if(!validate_input_arguments(argc, argv))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t pin = (uint8_t)strtol(argv[1], NULL, 16);
    
    if(!IS_VALID_PIN(pin))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t state = 0;
    if(gpio_ctrl_get_pin_state((gpio_ctrl_pin_e)pin, &state) != NRF_SUCCESS)
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t rsp_buf[3] = { 0, 0, 0 };
    snprintf((char*)rsp_buf, sizeof(rsp_buf), "%02x", state);
    at_util_uart_put_string(rsp_buf);
    
    return AT_RESULT_QUERY;
}

static uint32_t gpio_command_get_config(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        return AT_RESULT_ERROR;
    }
    
    if(!validate_input_arguments(argc, argv))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t pin = (uint8_t)strtol(argv[1], NULL, 16);
    
    char rsp_buf[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    gpio_pin_config_t config;
    uint32_t err = gpio_ctrl_pin_config_get((gpio_ctrl_pin_e)pin, &config);
    if(NRF_SUCCESS != err)
    {
        return AT_RESULT_ERROR;
    }
    
    snprintf(rsp_buf, sizeof(rsp_buf), "%02x %02x %02x", 
        (uint8_t)config.mapping->pin_id, (uint8_t)config.dir, (uint8_t)config.pull);
    at_util_uart_put_string((const uint8_t*)rsp_buf);
    
    return AT_RESULT_QUERY;
}

static uint32_t gpio_command_status_pin_config(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        const default_app_settings_t * p_settings = storage_intf_get();
        char rsp_buf[6] = { 0, 0, 0, 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x %02x", p_settings->status_pin, p_settings->status_pin_polarity);
        at_util_uart_put_string((const uint8_t*)rsp_buf);
        return AT_RESULT_QUERY;
    }
    
    if(argc != 3)
    {
        return AT_RESULT_ERROR;
    }
    
    if(!validate_input_arguments(argc, argv))
    {
        return AT_RESULT_ERROR;
    }
    
    uint8_t pin = (uint8_t)strtol(argv[1], NULL, 16);
    uint8_t polarity = (uint8_t)strtol(argv[2], NULL, 16);
    
    if(pin == gpio_ctrl_invalid)
    {
        (void)gpio_ctrl_unconfig_status_pin();
    }
    else
    {
        uint32_t err = gpio_ctrl_config_status_pin((gpio_ctrl_pin_e)pin, polarity);
        if(NRF_SUCCESS != err)
        {
            return AT_RESULT_ERROR;
        }
        service_update_status_pin();
    }
    return AT_RESULT_OK;
}

static uint32_t gpio_command_status_pin_read(uint8_t argc, char ** argv, bool query)
{
    if(query)
    {
        const default_app_settings_t * p_settings = storage_intf_get();
        
        if((gpio_ctrl_pin_e)p_settings->status_pin == gpio_ctrl_invalid)
        {
            return AT_RESULT_ERROR;
        }
        
        uint8_t pin_state = 0xFF;
        uint32_t err = gpio_ctrl_get_pin_state((gpio_ctrl_pin_e)p_settings->status_pin, &pin_state);
        if(NRF_SUCCESS != err)
        {
            return AT_RESULT_ERROR;
        }
        
        if(STATUS_POLARITY_ACTIVE_LOW == p_settings->status_pin_polarity)
        {
            pin_state ^= 0x01;
        }
        
        char rsp_buf[3] = { 0, 0, 0 };
        snprintf(rsp_buf, sizeof(rsp_buf), "%02x", pin_state);
        at_util_uart_put_string((const uint8_t*)rsp_buf);
        
        return AT_RESULT_QUERY;
    }
    
    return AT_RESULT_ERROR;
}

const at_command_t gpio_cmds[] = {
    { "gcfg", 3, 3, true, gpio_command_config },
    { "gset", 2, 2, true, gpio_command_set },
    { "gread", 1, 1, true, gpio_command_read },
    { "gcget", 1, 1, true, gpio_command_get_config },
    { "gstc", 0, 2, true, gpio_command_status_pin_config },
    { "gstr", 0, 0, true, gpio_command_status_pin_read },
    
    /* List Terminator */
    { NULL },
};
//AT_STATIC_REGISTER_CMDS(default_cmds);

void at_commands_gpio_init(void)
{
    at_commands_register(gpio_cmds);
}

/* Only works for 8-bit input values */
static bool validate_input_arguments(uint8_t argc, char ** argv)
{
    for(uint8_t i = 1; i < argc; i++)
    {
        if(!at_util_validate_input_str((const uint8_t*)argv[i], EIGHT_BIT_STR_LEN))
        {
            return false;
        }
    }
    
    return true;
}
