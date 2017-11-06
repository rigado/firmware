/** @file storage_intf.c
*
* @brief This module manages non-volatile storage opeations for BMDware
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "app_error.h"
#include "pstorage.h"
#include "pstorage_platform.h"

#include "storage_intf.h"
#include "lock.h"
#include "crc.h"

#define STORAGE_BLOCK_COUNT         1
#define STORAGE_SIZE                sizeof(default_app_settings_t)
    
#define DEFAULT_CONN_ADV_INT        1285
#define DEFAULT_BEACON_ADV_INT      100

const default_app_settings_t default_settings = 
{
    //beacon config
	{
        { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
          0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
    },						//uuid
    0x0000, 				//major
    0x0000, 				//minor
    DEFAULT_BEACON_ADV_INT,	//adv_interval (100 ms)
    -4, 					//tx power 
    false,                  //enable

	{ LOCK_DEFAULT_PASSWORD, 0 },	//password
        
	//uart config
    57600,					//baud
    0x00,					//parity
    0x01,					//stop bits
    false,                   //flow control
    false,					//enable
    
	//service tx power?
    -4, 					//tx power
    
    DEFAULT_CONN_ADV_INT,   //connectable advertising interval
    
    {-63, -4},              //rssi cal value, rssi cal @ tx_power
    
    { 
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0
    },                      //empty beacon data
    0,                      //beacon data len of 0
    true,
    
    0xFF,
    0x01,
    
    { 'R', 'i', 'g', 'C', 'o', 'm', 0, 0, 0 },
                            //device name
    
    { 0 },                  //padding
    
    0, 					    //crc8 calculated at write time
};

static pstorage_handle_t m_storage_handle;
static default_app_settings_t m_app_settings;
static bool m_is_dirty = false;

static bool is_valid( void );
static void dm_pstorage_cb_handler(pstorage_handle_t * p_handle,
                                   uint8_t             op_code,
                                   uint32_t            result,
                                   uint8_t           * p_data,
                                   uint32_t            data_len);
// ------------------------------------------------------------------------------

uint32_t storage_intf_init( void )
{
    uint32_t err_code;
    pstorage_module_param_t pstorage_param;
    
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);
    
    pstorage_param.block_count = STORAGE_BLOCK_COUNT;
    pstorage_param.block_size = STORAGE_SIZE;
    pstorage_param.cb = dm_pstorage_cb_handler;
    
    err_code = pstorage_register(&pstorage_param, &m_storage_handle);
    APP_ERROR_CHECK(err_code);
    
    err_code = storage_intf_load();
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}
// ------------------------------------------------------------------------------

uint32_t storage_intf_load( void )
{
    uint32_t err_code;
    pstorage_handle_t block_handle;
    
    err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
    APP_ERROR_CHECK(err_code);
    
    err_code = pstorage_load((uint8_t*)&m_app_settings, &block_handle, STORAGE_SIZE, 0);
    APP_ERROR_CHECK(err_code);
    
    if(!is_valid())
    {
        storage_intf_set(&default_settings);
        err_code = storage_intf_save();
        APP_ERROR_CHECK(err_code);
    }
    
    return err_code;
}
// ------------------------------------------------------------------------------

uint32_t storage_intf_save( void )
{
    uint32_t err_code;
    pstorage_handle_t block_handle;
    
    if(!m_is_dirty)
        return NRF_ERROR_INVALID_STATE;
    
    err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
    APP_ERROR_CHECK(err_code);
    
    err_code = pstorage_clear(&m_storage_handle, STORAGE_SIZE);
    APP_ERROR_CHECK(err_code);
    
    err_code = pstorage_store(&block_handle, (uint8_t*)&m_app_settings, STORAGE_SIZE, 0);
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}
// ------------------------------------------------------------------------------

uint32_t storage_intf_clear( void )
{
    uint32_t err_code;
    
    err_code = pstorage_clear(&m_storage_handle, STORAGE_SIZE);
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}
// ------------------------------------------------------------------------------

const default_app_settings_t * storage_intf_get( void )
{
    return &m_app_settings;
}
// ------------------------------------------------------------------------------

bool storage_intf_set( const default_app_settings_t * const settings )
{
    if(settings == NULL)
        return false;
    
    memcpy(&m_app_settings, settings, sizeof(m_app_settings));
		
	//update the crc
	m_app_settings.crc8 = crc8((uint8_t*)&m_app_settings, sizeof(default_app_settings_t)-1);
    m_is_dirty = true;
    
    return true;
}
// ------------------------------------------------------------------------------

bool storage_intf_is_dirty( void )
{
    return m_is_dirty;
}
// ------------------------------------------------------------------------------

static bool is_valid( void )
{
	uint8_t crcCalc = 0;
    volatile uint8_t settings_size = sizeof(default_app_settings_t);
	crcCalc = crc8((uint8_t*)&m_app_settings, settings_size - 1);

	return (crcCalc == m_app_settings.crc8);
}

/**@brief Function for pstorage module callback.
 *
 * @param[in] p_handle Identifies module and block for which callback is received.
 * @param[in] op_code  Identifies the operation for which the event is notified.
 * @param[in] result   Identifies the result of flash access operation.
 *                     NRF_SUCCESS implies, operation succeeded.
 * @param[in] p_data   Identifies the application data pointer. In case of store operation, this
 *                     points to the resident source of application memory that application can now
 *                     free or reuse. In case of clear, this is NULL as no application pointer is
 *                     needed for this operation.
 * @param[in] data_len Length of data provided by the application for the operation.
 */
static void dm_pstorage_cb_handler(pstorage_handle_t * p_handle,
                                   uint8_t             op_code,
                                   uint32_t            result,
                                   uint8_t           * p_data,
                                   uint32_t            data_len)
{
    if( op_code == PSTORAGE_STORE_OP_CODE )
    {
        if(result == NRF_SUCCESS)
        {
            m_is_dirty = false;
        }
    }
}
// ------------------------------------------------------------------------------
