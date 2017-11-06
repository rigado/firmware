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
#include "fstorage.h"
#include "section_vars.h"

#include "storage_intf.h"
#include "lock.h"
#include "crc.h"

#define NUM_PAGES         1
//todo - calculate page count by struct size
    
#ifdef BMD_DEBUG
#define DEFAULT_CONN_ADV_INT        100
#else
#define DEFAULT_CONN_ADV_INT        1285
#endif

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
    
    false,                  //at hotswap enabled
    
    0, 					    //crc8 calculated at write time
};

static default_app_settings_t m_app_settings;
static bool m_is_dirty = false;

static bool is_valid( void );
static void fstorage_callback(fs_evt_t const * const evt, fs_ret_t result);

FS_REGISTER_CFG(fs_config_t fs_config) =
{
    .callback  = fstorage_callback, // Function for event callbacks.
    .num_pages = NUM_PAGES,      // Number of physical flash pages required.
    .priority  = 0xFE            // Priority for flash usage. (0xff is reserved)
};
// ------------------------------------------------------------------------------

uint32_t storage_intf_init( void )
{
    uint32_t err_code;
   
    err_code = fs_init();
    APP_ERROR_CHECK(err_code);
    
    err_code = storage_intf_load();
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}
// ------------------------------------------------------------------------------

uint32_t storage_intf_load( void )
{
    uint32_t err_code = NRF_SUCCESS;
    
    /* just memcpy out... */
    memcpy((void*)&m_app_settings, (void*)fs_config.p_start_addr, sizeof(m_app_settings));
    
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
    
    if(!m_is_dirty)
        return NRF_ERROR_INVALID_STATE;
    
    err_code = fs_erase(&fs_config, fs_config.p_start_addr, NUM_PAGES, NULL);
    APP_ERROR_CHECK(err_code);
    
    err_code = fs_store(&fs_config, fs_config.p_start_addr, (void*)&m_app_settings, sizeof(m_app_settings), NULL);
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}
// ------------------------------------------------------------------------------

uint32_t storage_intf_clear( void )
{
    uint32_t err_code;
    
    err_code = fs_erase(&fs_config, fs_config.p_start_addr, NUM_PAGES, NULL);
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
	m_app_settings.crc8 = crc8((uint8_t*)&m_app_settings, sizeof(m_app_settings)-1);
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

/**@brief Function for fstorage module callback.
 *
 * @param[in] evt       Identifies fstorage event
 * @param[in] result    Identifies result of event
 */
static void fstorage_callback(fs_evt_t const * const evt, fs_ret_t result)
{
    if(evt->id == FS_EVT_STORE)
    {
        if(result == FS_SUCCESS)
        {
            m_is_dirty = false;
        }
    }
}
// ------------------------------------------------------------------------------
