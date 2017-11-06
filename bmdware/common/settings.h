/** @file settings.h
*
* @brief This module provides an interface to access and manage all BMDware
*        settings.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    Setting_UUID,
    Setting_Major,
    Setting_Minor,
    Setting_AdvInt,
    Setting_BeaconTxPower,
    Setting_BeaconEnable,
    
    Setting_BaudRate,
    Setting_Parity,
    Setting_StopBits,
    Setting_FlowControl,
    Setting_UartEnable,
    
    Setting_NonBeaconTxPower,
    
    Setting_CustomBeaconData,
    Setting_CustomBeaconDataLen
} settings_e;

typedef struct
{
    uint8_t size;
    uint8_t count;
    bool is_discrete;
    const void * data;
} settings_validation_t;



#define SETTINGS_UUID_POS       (0UL)
#define SETTINGS_UUID_MASK      (0x1UL << AAR_INTENSET_NOTRESOLVED_Pos)
#define SETTINGS_UUID_LEN       (sizeof(ble_uuid128_t))
    
#define SETTINGS_MAJOR_POS      (1UL)
#define SETTINGS_MAJOR_MASK     (0x1UL << SETTINGS_MAJOR_POS)
#define SETTINGS_MAJOR_LEN      (sizeof(uint16_t))
    
#define SETTINGS_MINOR_POS      (2UL)
#define SETTINGS_MINOR_MASK     (0x1UL << SETTINGS_MINOR_POS)
#define SETTINGS_MINOR_LEN      (sizeof(uint16_t))
    
#define SETTINGS_ADV_INT_POS    (3UL)
#define SETTINGS_ADV_INT_MASK   (0x1UL << SETTINGS_ADV_INT_POS)
#define SETTINGS_ADV_INT_LEN    (sizeof(uint16_t))
    
#define SETTINGS_BCN_TX_PWR_POS     (4UL)
#define SETTINGS_BCN_TX_PWR_MASK    (0x1UL << SETTINGS_BCN_TX_PWR_POS)
#define SETTINGS_BCN_TX_PWR_LEN     (sizeof(int8_t))
    
#define SETTINGS_BCN_ENABLE_POS     (5UL)
#define SETTINGS_BCN_ENABLE_MASK    (0x1UL << SETTINGS_BCN_ENABLE_POS)
#define SETTINGS_BCN_ENABLE_LEN     (sizeof(bool))
    
#define SETTINGS_BAUD_RATE_POS  (6UL)
#define SETTINGS_BAUD_RATE_MASK (0x1UL << SETTINGS_BAUD_RATE_POS)
#define SETTINGS_BAUD_RATE_LEN      (sizeof(uint16_t))
    
#define SETTINGS_PARITY_POS     (7UL)
#define SETTINGS_PARITY_MASK    (0x1UL << SETTINGS_PARITY_POS)
#define SETTINGS_PARITY_LEN     (sizeof(uint8_t))
    
#define SETTINGS_STOP_BITS_POS  (8UL)
#define SETTINGS_STOP_BITS_MASK (0x1UL << SETTINGS_STOP_BITS_POS)
#define SETTINGS_STOP_BITS_LEN  (sizeof(uint8_t))
    
#define SETTINGS_FLOW_CTRL_POS  (9UL)
#define SETTINGS_FLOW_CTRL_MASK (0x1UL << SETTINGS_FLOW_CTRL_POS)
#define SETTINGS_FLOW_CTRL_LEN  (sizeof(bool))

#define SETTINGS_UART_ENABLE_POS    (10UL)
#define SETTINGS_UART_ENABLE_MASK   (0x1UL << SETTINGS_UART_ENBL_POS)
#define SETTINGS_UART_ENABLE_LEN    (sizeof(bool))
    
#define SETTINGS_CON_TX_PWR_POS     (11UL)
#define SETTINGS_CON_TX_PWR_MASK    (0x1UL << SETTINGS_CON_TX_PWR_POS)
#define SETTINGS_CON_TX_PWR_LEN     (sizeof(int8_t))
    
#define SETTINGS_CUSTOM_BCN_DATA_POS    (12UL)
#define SETTINGS_CUSTOM_BCN_DATA_MASK   (0x1UL << SETTINGS_CUSTOM_BCN_DATA_POS)
#define SETTINGS_CUSTOM_BCN_DATA_LEN    (30UL)

#define SETTINGS_CUSTOM_BCN_LEN_POS     (13UL)
#define SETTINGS_CUSTOM_BCN_LEN_MASK    (0x1UL << SETTINGS_CUSTOM_BCN_LEN_POS)
#define SETTINGS_CUSTOM_BCN_LEN_LEN     (sizeof(uint8_t))


/** @brief Initializes the settings interface
 *
 *  @details    This function will load the settings from the storage interface. If the
 *              loaded data does not pass a CRC check then the default settings will be
 *              applied and stored to the device. This function WILL block to wait for
 *              storage completion if the default settings need to be applied. This ensures
 *              they are properly stored before continued device operation.
 *
 *  @return     NRF_SUCCESS when settings are successfully loaded
 */
uint32_t settings_init( void );

/** @brief Set the value for a setting
 *
 *  @param[in]  setting The setting for which to set the value
 *  @param[in]  data    The value to copy to the setting
 *  @param[in]  length  The lenght of the input data
 *
 *  @return     NRF_SUCCESS if the new setting value was applied successfully
 *              NRF_ERROR_NOT_FOUND if setting is not a valid value
 *              NRF_ERROR_INVALID_DATA if data is out of range for setting
 **/

uint32_t settings_set_value( settings_e setting, void * data, uint32_t length);

/** @brief Retrieve a setting's value
 *
 *  @details    This function does not verify the length of the value.  The pre-condition assumes
 *              the array data is large enough to fit the value of the setting.
 *
 *  @param[in]  setting The setting to retrieve
 *  @param[out] data    The setting's data
 *
 *  @return     Returns NRF_SUCCESS if the input setting value was valid; otherwise, returns
 *              NRF_ERROR_NOT_FOUND
 **/
uint32_t settings_get_value( settings_e setting, void * data );

/** @brief Retrieves the length of a given setting
 *  
 *  @param[in] setting  The setting for which to retrieve the length
 *
 *  @return     Returns the length of the settings value or NRF_ERROR_NOT_FOUND if an invalid
 *              setting is requested.
 **/
uint32_t settings_get_len_of_value( settings_e setting );

uint32_t settings_clear( void );
uint32_t settings_save( void );

#endif // __SETTINGS_H__
