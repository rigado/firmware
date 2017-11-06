/** @file gatt.c
*
* @brief GATT utility commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
#include "ble_gatt.h"

#ifdef NRF52
    static uint16_t runtime_mtu = GATT_MTU_SIZE_DEFAULT;
#else
    static uint16_t runtime_mtu = GATT_MTU_SIZE_DEFAULT;
#endif

void gatt_set_runtime_mtu(uint32_t mtu)
{
    runtime_mtu = mtu;
}

uint16_t gatt_get_runtime_mtu(void)
{
    return (runtime_mtu - 3);
}
