/** @file service.h
*
* @brief This module initializes all BMDware services
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __SERVICES_INIT_H__
#define __SERVICES_INIT_H__

#include "ble_beacon_config.h"
#include "ble_nus.h"

void services_init(void);

ble_beacon_config_t * services_get_beacon_config_obj(void);
ble_nus_t * services_get_nus_config_obj(void);
void services_beacon_config_evt(ble_evt_t * p_ble_evt);
void services_ble_nus_evt(ble_evt_t * p_ble_evt);
void service_set_connected_state(bool state);
bool service_get_connected_state(void);
void service_update_status_pin(void);

#endif
