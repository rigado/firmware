/** @file gatt.h
*
* @brief GATT utility commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef GATT_H_
#define GATT_H_

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdint.h>
    
void gatt_set_runtime_mtu(uint16_t mtu);
uint16_t gatt_get_runtime_mtu(void);

#endif
