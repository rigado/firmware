#include "gpio_ctrl.h"

static const pin_mapping_t pin_map_list[] = {
    { gpio_ctrl_pin_0, 1 },
    { gpio_ctrl_pin_1, 24 },
    { gpio_ctrl_pin_2, 25 },
    { gpio_ctrl_pin_3, 0 },
    { gpio_ctrl_pin_4, 4 },
    { gpio_ctrl_pin_5, 6 },
    { gpio_ctrl_pin_6, 8 }
};

static gpio_pin_config_t gpio_pin_config_list[] __attribute((unused)) =
{
    { &pin_map_list[0], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[1], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[2], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[3], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[4], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[5], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL },
    { &pin_map_list[6], NRF_GPIO_PIN_DIR_INPUT, NRF_GPIO_PIN_NOPULL }
};
