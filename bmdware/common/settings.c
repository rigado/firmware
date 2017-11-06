/** @file settings.c
*
* @brief This module provides an interface to access and manage all BMDware
*        settings.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "app_util.h"
#include "app_error.h"
#include "ble_types.h"
#include "lock.h"

#include "settings.h"

typedef struct
{
    /* Beacon Settings */
    uint8_t uuid[16];
    uint16_t major;
    uint16_t minor;
    uint16_t adv_interval;
    int8_t beacon_tx_power;
    int8_t calculated_tx_pwr;
    bool enable;
    
    //25 bytes
	
    /* UART Passthrough Settings */
    uint32_t baud_rate;
    uint8_t parity;
    uint8_t stop_bits;
    bool flow_control;
    bool uart_enable; //8 bytes
    
    //33 bytes
    
	/* TX power for non-ibeacon modes */
	int8_t connectable_tx_power;
    
    //34 bytes
    
    uint8_t custom_beacon_data[30];
    uint8_t custom_beacon_data_len;
    
    //65 bytes
		
    /* Note: For pstorage, the block size must be a multiple
    of 4bytes, including the CRC */
	uint8_t pad[2];
    //67 bytes
    
    uint8_t crc8;
    //68 bytes
    
} settings_t;

/* Data stored in flash must be multiple of 4 */
STATIC_ASSERT((sizeof(settings_t) % 4) == 0);

/* Advertising interval range in Milliseconds */
static const uint32_t adv_int_val[] = { 10, 10000 };

/* Available TX Power levels.  See sd_ble_gap_tx_power_set for more info */
static const int8_t tx_pwr_val[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };

/* Enables are either true or false */
static const uint8_t enable_val[] = { 0, 1 };

/* Available UART baud rates.  See nrf51_bitfields.h */
static const uint32_t baud_rate_val[] = {
    1200,
    2400,
    4800,
    9600,
    14400,
    19200,
    28800,
    38400,
    57600,
    76800,
    115200,
    230400
};

//add length of each element
static const settings_validation_t validation_table[] =
{
    //UUID
    { 0, 0, false, 0 },
    //Major
    { 0, 0, false, 0 },
    //Minor
    { 0, 0, false, 0 },
    //Adv Interval
    { 4, sizeof(adv_int_val), false, adv_int_val },
    //Beacon Tx Power
    { 1, sizeof(tx_pwr_val), true, tx_pwr_val },
    //Beacon Enable
    { 1, sizeof(enable_val), true, enable_val },
    
    //Baud Rate
    { 4, sizeof(baud_rate_val), true, baud_rate_val },
    //Parity
    { 1, sizeof(enable_val), true, enable_val },
    //Stop Bits
    { 0, 0, 0 },
    //Uart Enable
    { 1, sizeof(enable_val), true, enable_val },
    
    //Non-beacon TX Power
    { 1, sizeof(tx_pwr_val), true, tx_pwr_val },
    
    { 0, 0, 0 },
    { 0, 0, 0 }
};

const settings_t def_settings = {
    /* Beacon Configuration */
	{
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    },						//uuid
    0x0000, 				//major
    0x0000, 				//minor
    0x64, 					//adv_interval (100 ms)
    -4, 					//tx power
    0,                      //calculated tx power
    false,                  //enable
    
    /* Passthrough UART Configuration */
    115200,					//baud
    0x00,					//parity
    0x01,					//stop bits
    true,                   //flow control
    false,					//enable
       
    /* TX Power for non-iBeacon modes */
    -4,
    
    /* EPS Note: We should just store all of the beacon data in one way.
     * Do we really need to store other beacon data in a differen fashion?
     * Do we need to support concurrent beacon type operations for BMDware? */
    /* Custom beacon data */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    /* Custom beacon data length */
    0,
      
    /* Padding */
    { 0, 0 },
      
    /* CRC */
    0
};

static settings_t runtime_settings;

//function also needs data length for validation
static bool is_valid( settings_e setting, void * data )
{
    bool result = false;
    
    /* Get settings validation structure */
    const settings_validation_t * validator = &validation_table[setting];
    if(validator->count == 0)
        return true;
    
    /* Validate against a discrete list of values */
    if(validator->is_discrete)
    {
        uint8_t count = 0;
        const uint8_t * val_data_ptr = (const uint8_t *)validator->data;
        while(count < validator->count)
        {
            if(memcmp(val_data_ptr, data, validator->size) == 0)
            {
                result = true;
                break;
            }
            val_data_ptr += validator->size;
            count++;
        }
    }
    else
    {
        if(validator->size == sizeof(uint8_t))
        {
            uint8_t val_min = *((const uint8_t*)validator->data);
            uint8_t val_max = *((const uint8_t*)validator->data+sizeof(uint8_t));
            uint8_t value = *((uint8_t*)data);
            if(value >= val_min && value <= val_max)
            {
                result = true;
            }
        }
        else if(validator->size == sizeof(uint16_t))
        {
            uint16_t val_min = *((const uint16_t*)validator->data);
            uint16_t val_max = *((const uint16_t*)validator->data+sizeof(uint16_t));
            uint16_t value = *((uint8_t*)data);
            if(value >= val_min && value <= val_max)
            {
                result = true;
            }
        }
        else if(validator->size == sizeof(uint32_t))
        {
            uint32_t val_min = *((const uint32_t*)validator->data);
            uint32_t val_max = *((const uint32_t*)validator->data+sizeof(uint32_t));
            uint32_t value = *((uint8_t*)data);
            if(value >= val_min && value <= val_max)
            {
                result = true;
            }
        }
    }
    
    return result;
}

uint32_t settings_init( void )
{
    /* storage_intf_init( runtime_settings, sizeof(settings_t) */
    return NRF_SUCCESS;
}

uint32_t settings_set_value( settings_e setting, void * data, uint32_t length)
{
    if(!is_valid(setting, data))
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    /* copy settings to runtime settings */
    switch(setting)
    {
        case Setting_UUID:
            memcpy(runtime_settings.uuid, data, SETTINGS_UUID_LEN);
            break;
        case Setting_Major:
            memcpy(&runtime_settings.major, (uint16_t*)data, SETTINGS_MAJOR_LEN);
            break;
        case Setting_Minor:
            memcpy(&runtime_settings.minor, data, SETTINGS_MINOR_LEN);
            break;
        case Setting_AdvInt:
            memcpy(&runtime_settings.adv_interval, data, SETTINGS_ADV_INT_LEN);
            break;
        case Setting_BeaconTxPower:
            memcpy(&runtime_settings.beacon_tx_power, data, SETTINGS_BCN_TX_PWR_LEN);
            break;
        case Setting_BeaconEnable:
            memcpy(&runtime_settings.enable, data, SETTINGS_BCN_ENABLE_LEN);
            break;
        case Setting_BaudRate:
            memcpy(&runtime_settings.baud_rate, data, SETTINGS_BAUD_RATE_LEN);
            break;
        case Setting_Parity:
            memcpy(&runtime_settings.parity, data, SETTINGS_PARITY_LEN);
            break;
        case Setting_StopBits:
            memcpy(&runtime_settings.stop_bits, data, SETTINGS_STOP_BITS_LEN);
            break;
        case Setting_FlowControl:
            memcpy(&runtime_settings.flow_control, data, SETTINGS_FLOW_CTRL_LEN);
            break;
        case Setting_UartEnable:
            memcpy(&runtime_settings.uart_enable, data, SETTINGS_UART_ENABLE_LEN);
            break;
        case Setting_NonBeaconTxPower:
            memcpy(&runtime_settings.connectable_tx_power, data, SETTINGS_CON_TX_PWR_LEN);
            break;
        case Setting_CustomBeaconData:
            memcpy(runtime_settings.custom_beacon_data, data, SETTINGS_CUSTOM_BCN_DATA_LEN);
            break;
        case Setting_CustomBeaconDataLen:
            memcpy(&runtime_settings.custom_beacon_data_len, data, SETTINGS_CUSTOM_BCN_LEN_LEN);
            break;
        default:
            return NRF_ERROR_NOT_FOUND;
    }
    
    //should mark data as dirty but not save at this time
    
    return NRF_SUCCESS;
}

uint32_t settings_get_value( settings_e setting, void * data )
{    
    /* copy runtime setting to data array */
    switch(setting)
    {
        case Setting_UUID:
            memcpy(data, runtime_settings.uuid, SETTINGS_UUID_LEN);
            break;
        case Setting_Major:
            memcpy(data, &runtime_settings.major, SETTINGS_MAJOR_LEN);
            break;
        case Setting_Minor:
            memcpy(data, &runtime_settings.minor, SETTINGS_MINOR_LEN);
            break;
        case Setting_AdvInt:
            memcpy(data, &runtime_settings.adv_interval, SETTINGS_ADV_INT_LEN);
            break;
        case Setting_BeaconTxPower:
            memcpy(data, &runtime_settings.beacon_tx_power, SETTINGS_BCN_TX_PWR_LEN);
            break;
        case Setting_BeaconEnable:
            memcpy(data, &runtime_settings.enable, SETTINGS_BCN_ENABLE_LEN);
            break;
        case Setting_BaudRate:
            memcpy(data, &runtime_settings.baud_rate, SETTINGS_BAUD_RATE_LEN);
            break;
        case Setting_Parity:
            memcpy(data, &runtime_settings.parity, SETTINGS_PARITY_LEN);
            break;
        case Setting_StopBits:
            memcpy(data, &runtime_settings.stop_bits, SETTINGS_STOP_BITS_LEN);
            break;
        case Setting_FlowControl:
            memcpy(data, &runtime_settings.flow_control, SETTINGS_FLOW_CTRL_LEN);
            break;
        case Setting_UartEnable:
            memcpy(data, &runtime_settings.uart_enable, SETTINGS_UART_ENABLE_LEN);
            break;
        case Setting_NonBeaconTxPower:
            memcpy(data, &runtime_settings.connectable_tx_power, SETTINGS_CON_TX_PWR_LEN);
            break;
        case Setting_CustomBeaconData:
            memcpy(data, runtime_settings.custom_beacon_data, SETTINGS_CUSTOM_BCN_DATA_LEN);
            break;
        case Setting_CustomBeaconDataLen:
            memcpy(data, &runtime_settings.custom_beacon_data_len, SETTINGS_CUSTOM_BCN_LEN_LEN);
            break;
        default:
            return NRF_ERROR_NOT_FOUND;
    }
    return NRF_SUCCESS;
}

uint32_t settings_get_len_of_value( settings_e setting )
{
    /* copy runtime setting to data array */
    switch(setting)
    {
        case Setting_UUID:
            return SETTINGS_UUID_LEN;
        case Setting_Major:
            return SETTINGS_MAJOR_LEN;
        case Setting_Minor:
            return SETTINGS_MINOR_LEN;
        case Setting_AdvInt:
            return SETTINGS_ADV_INT_LEN;
        case Setting_BeaconTxPower:
            return SETTINGS_BCN_TX_PWR_LEN;
        case Setting_BeaconEnable:
            return SETTINGS_BCN_ENABLE_LEN;
        case Setting_BaudRate:
            return SETTINGS_BAUD_RATE_LEN;
        case Setting_Parity:
            return SETTINGS_PARITY_LEN;
        case Setting_StopBits:
            return SETTINGS_STOP_BITS_LEN;
        case Setting_FlowControl:
            return SETTINGS_FLOW_CTRL_LEN;
        case Setting_UartEnable:
            return SETTINGS_UART_ENABLE_LEN;
        case Setting_NonBeaconTxPower:
            return SETTINGS_CON_TX_PWR_LEN;
        case Setting_CustomBeaconData:
            return SETTINGS_CUSTOM_BCN_DATA_LEN;
        case Setting_CustomBeaconDataLen:
            return SETTINGS_CUSTOM_BCN_LEN_LEN;
        default:
            return NRF_ERROR_NOT_FOUND;
    }
}

uint32_t settings_clear( void )
{
    return NRF_SUCCESS;
}

uint32_t settings_save( void )
{
    return NRF_SUCCESS;
}
