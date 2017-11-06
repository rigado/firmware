/** @file storage_intf.h
*
* @brief This module manages non-volatile storage opeations for BMDware
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __STORAGE_INTF_H__
#define __STORAGE_INTF_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "app_util.h"
#include "ble_types.h"
#include "lock.h"
#include "ble.h"

#define CUSTOM_BEACON_DATA_MAX_LEN      BLE_GAP_ADV_MAX_SIZE

#pragma pack(1)
typedef struct
{
    int8_t rssi;
    int8_t tx_power;
} rssi_cal_t;
#pragma pack()

typedef struct
{
    /* Beacon Settings */
    ble_uuid128_t uuid;
    uint16_t major;
    uint16_t minor;
    uint16_t adv_interval;
    int8_t beacon_tx_power;
    bool enable; //24
	lock_password_t password; //20
	
    /* UART Passthrough Settings */
    uint32_t baud_rate;
    uint8_t parity;
    uint8_t stop_bits;
    bool flow_control;
    bool uart_enable; //8
    
	/* TX power for non-ibeacon modes */
	int8_t connectable_tx_power;
    uint16_t conn_adv_interval; //3
    
    /* RSSI cal */
    rssi_cal_t rssi_cal; //2
    
    /* Custom Beacon Data */
    uint8_t beacon_data[CUSTOM_BEACON_DATA_MAX_LEN]; 
    uint8_t beacon_data_len; //32
    
    bool connectable_adv_enabled; //1
    
    uint8_t status_pin;           //1
    uint8_t status_pin_polarity;  //1
    
    uint8_t device_name[9];       //8
    	
    bool at_hotswap_enabled;      //1
    	
    /* Note: For pstorage, the block size must be a multiple of 4 bytes, so you may need to pad here */
    
    uint8_t crc8;
    
    //Total: 24 + 20 + 8 + 3 + 2 + 1 + 32 + 1 + 1 + 1 + 9 + 1 + 1 : 100    
} default_app_settings_t;

STATIC_ASSERT((sizeof(default_app_settings_t) % 4) == 0);
STATIC_ASSERT((offsetof(default_app_settings_t, crc8) % 4) == 3); //crc8 must be at the very end, add pad bytes to align

extern const default_app_settings_t default_settings;

/* Interface functions */
uint32_t storage_intf_init( void );
uint32_t storage_intf_load( void );
uint32_t storage_intf_save( void );
uint32_t storage_intf_clear( void );
bool storage_intf_is_dirty( void );

/* Settings functions */
const default_app_settings_t * storage_intf_get( void );
bool storage_intf_set( const default_app_settings_t * const settings );


#endif
