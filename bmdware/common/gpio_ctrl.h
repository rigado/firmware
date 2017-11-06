/** @file gpio_ctrl.c
*
* @brief This module provides interface functions to control GPIO
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __GPIO_CTRL_H__
#define __GPIO_CTRL_H__

#include <stdint.h>

#include "nrf_gpio.h"

#define PIN_CLEAR   0
#define PIN_SET     1

#define STATUS_POLARITY_ACTIVE_LOW  0
#define STATUS_POLARITY_ACTIVE_HIGH 1

typedef enum
{
    gpio_ctrl_pin_first,
    gpio_ctrl_pin_0 = gpio_ctrl_pin_first,
    gpio_ctrl_pin_1,
    gpio_ctrl_pin_2,
    gpio_ctrl_pin_3,
    gpio_ctrl_pin_4,
    gpio_ctrl_pin_5,
    gpio_ctrl_pin_6,
    gpio_ctrl_pin_7,
    gpio_ctrl_pin_8,
    gpio_ctrl_pin_9,
    gpio_ctrl_pin_10,
    gpio_ctrl_pin_11,
    gpio_ctrl_pin_12,
    gpio_ctrl_pin_13,
    gpio_ctrl_pin_14,
    gpio_ctrl_pin_15,
    gpio_ctrl_pin_16,
    gpio_ctrl_pin_17,
    gpio_ctrl_pin_18,
    gpio_ctrl_pin_19,
    gpio_ctrl_pin_20,
    gpio_ctrl_pin_21,
    gpio_ctrl_pin_22,
#ifdef NRF51
    gpio_ctrl_pin_last = gpio_ctrl_pin_6,
#elif defined(NRF52)
    gpio_ctrl_pin_last = gpio_ctrl_pin_22,
#endif
    gpio_ctrl_invalid = 0xFF,
} gpio_ctrl_pin_e;

#define IS_VALID_PIN(pin) ((pin > gpio_ctrl_pin_last) ? false : true)

typedef struct
{
    gpio_ctrl_pin_e pin_id; 
    uint8_t actual_pin;
} pin_mapping_t;

typedef struct gpio_pin_config_s
{
    const pin_mapping_t * mapping;
    nrf_gpio_pin_dir_t dir;
    nrf_gpio_pin_pull_t pull;
} gpio_pin_config_t;

void gpio_ctrl_init(void);
uint32_t gpio_ctrl_config_pin(gpio_ctrl_pin_e pin, nrf_gpio_pin_dir_t dir, nrf_gpio_pin_pull_t pull);
uint32_t gpio_ctrl_pin_config_get(gpio_ctrl_pin_e pin, gpio_pin_config_t * const p_config);
uint32_t gpio_ctrl_set_pin_state(gpio_ctrl_pin_e pin, uint8_t pin_state);
uint32_t gpio_ctrl_get_pin_state(gpio_ctrl_pin_e pin, uint8_t * pin_state);
uint32_t gpio_ctrl_config_status_pin(gpio_ctrl_pin_e pin, uint8_t polarity);
uint32_t gpio_ctrl_unconfig_status_pin(void);
uint32_t gpio_ctrl_status_pin_set_state(uint8_t state);

#endif
