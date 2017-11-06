/** @file gpio_ctrl.c
*
* @brief This module provides interface functions to control GPIO
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "nrf_gpio.h"
#include "nrf_error.h"

#include "gpio_ctrl.h"

#include "storage_intf.h"

// Including a C file is not typically done and it technically against our coding standard.  However,
// it is done here to avoid using #ifdefs to define different structures for different projects.  This
// forces each project to generate their own gpio_ctrl_def.c file and ensures the data contained in
// the file does not have global scope.
#include "gpio_ctrl_def.c"

#define INVALID_STATUS_PIN      gpio_ctrl_invalid

static gpio_ctrl_pin_e m_status_pin;
static uint8_t m_status_pin_polarity = 0x01;

static void update_settings(void);
static uint32_t set_pin_state(gpio_ctrl_pin_e pin, uint8_t pin_state);

void gpio_ctrl_init(void)
{
    const default_app_settings_t * p_settings = storage_intf_get();
    if((gpio_ctrl_pin_e)p_settings->status_pin == gpio_ctrl_invalid)
    {
        gpio_ctrl_unconfig_status_pin();
    }
    else
    {
        gpio_ctrl_config_status_pin((gpio_ctrl_pin_e)p_settings->status_pin, p_settings->status_pin_polarity);
    }
    gpio_ctrl_status_pin_set_state(PIN_CLEAR);
}

uint32_t gpio_ctrl_config_pin(gpio_ctrl_pin_e pin, nrf_gpio_pin_dir_t dir, nrf_gpio_pin_pull_t pull)
{
    gpio_pin_config_t * pin_config;
    if(pin > gpio_ctrl_pin_last)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(pin == m_status_pin)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    /* Validate inputs */
    if(dir != NRF_GPIO_PIN_DIR_INPUT && dir != NRF_GPIO_PIN_DIR_OUTPUT)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(pull != NRF_GPIO_PIN_NOPULL && pull != NRF_GPIO_PIN_PULLUP && pull != NRF_GPIO_PIN_PULLDOWN)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    pin_config = &gpio_pin_config_list[(uint8_t)pin];
    pin_config->dir = dir;
    pin_config->pull = pull;
    
    if(dir == NRF_GPIO_PIN_DIR_OUTPUT)
    {
        nrf_gpio_cfg_output(pin_config->mapping->actual_pin);
    } 
    else if(dir == NRF_GPIO_PIN_DIR_INPUT)
    {
        nrf_gpio_cfg_input(pin_config->mapping->actual_pin, pull);
    }

    return NRF_SUCCESS;
}

uint32_t gpio_ctrl_pin_config_get(gpio_ctrl_pin_e pin, gpio_pin_config_t * const config)
{
    if(pin > gpio_ctrl_pin_last)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(pin == m_status_pin)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    memcpy(config, &gpio_pin_config_list[(uint8_t)pin], sizeof(gpio_pin_config_t));
    return NRF_SUCCESS;
}

uint32_t gpio_ctrl_set_pin_state(gpio_ctrl_pin_e pin, uint8_t pin_state)
{
    
    if( pin > gpio_ctrl_pin_last || (pin_state != PIN_SET && pin_state != PIN_CLEAR) )
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(pin == m_status_pin)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    return set_pin_state(pin, pin_state);
}

uint32_t gpio_ctrl_get_pin_state(gpio_ctrl_pin_e pin, uint8_t * pin_state)
{
    gpio_pin_config_t * pin_config;
    if( pin > gpio_ctrl_pin_last )
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    pin_config = &gpio_pin_config_list[(uint8_t)pin];
    uint32_t state = nrf_gpio_pin_read(pin_config->mapping->actual_pin);
    *pin_state = (uint8_t)state;
    
    return NRF_SUCCESS;
}

uint32_t gpio_ctrl_config_status_pin(gpio_ctrl_pin_e pin, uint8_t polarity)
{
    if(pin > gpio_ctrl_pin_last)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    if(m_status_pin != gpio_ctrl_invalid)
    {
        (void)gpio_ctrl_unconfig_status_pin();
    }
    
    uint32_t err = gpio_ctrl_config_pin(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_NOPULL);
    if(NRF_SUCCESS != err)
    {
        return err;
    }
    
    uint32_t actual_pin = pin_map_list[pin].actual_pin;
    nrf_gpio_cfg(actual_pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
    m_status_pin = pin;
    m_status_pin_polarity = STATUS_POLARITY_ACTIVE_LOW;
    if(polarity > STATUS_POLARITY_ACTIVE_LOW)
    {
        m_status_pin_polarity = STATUS_POLARITY_ACTIVE_HIGH;
    }
    
    update_settings();
    
    return NRF_SUCCESS;
}

uint32_t gpio_ctrl_unconfig_status_pin(void)
{
    uint32_t pin = m_status_pin;
    m_status_pin = INVALID_STATUS_PIN;
    uint32_t actual_pin = pin_map_list[pin].actual_pin;
    gpio_ctrl_config_pin((gpio_ctrl_pin_e)pin, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg(actual_pin, NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_INPUT_DISCONNECT, 
        NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
    
    update_settings();
    
    return NRF_SUCCESS;
}

uint32_t gpio_ctrl_status_pin_set_state(uint8_t state)
{
    if(m_status_pin == INVALID_STATUS_PIN)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    uint32_t err = NRF_ERROR_NOT_FOUND;
    if(STATUS_POLARITY_ACTIVE_HIGH == m_status_pin_polarity)
    {
        err = set_pin_state(m_status_pin, state);
    }
    else
    {
        err = set_pin_state(m_status_pin, (state ^ 0x01));
    }
    
    return err;
}

static void update_settings(void)
{
    const default_app_settings_t * p_settings = storage_intf_get();
    default_app_settings_t new_settings;
    
    memcpy(&new_settings, p_settings, sizeof(new_settings));
    new_settings.status_pin = (uint8_t)m_status_pin;
    new_settings.status_pin_polarity = (uint8_t)m_status_pin_polarity;
    
    if(storage_intf_set(&new_settings))
    {
        (void)storage_intf_save();
    }
}

static uint32_t set_pin_state(gpio_ctrl_pin_e pin, uint8_t pin_state)
{
    gpio_pin_config_t * pin_config = &gpio_pin_config_list[(uint8_t)pin];
    if(pin_config->dir != NRF_GPIO_PIN_DIR_OUTPUT)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    if(pin_state == PIN_SET)
    {
        nrf_gpio_pin_set(pin_config->mapping->actual_pin);
    }
    else
    {
        nrf_gpio_pin_clear(pin_config->mapping->actual_pin);
    }
    
    return NRF_SUCCESS;
}
