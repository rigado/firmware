/** @file advertising.c
*
* @brief This module initializes all advertising data for connectable and beacon 
*        advertisements
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>

#include "app_error.h"
#include "ble_advdata.h"
#include "ble_beacon_config.h"
#include "ble_nus.h"
#include "nordic_common.h"
#include "storage_intf.h"
#include "sys_init.h"
#include "app_timer.h"
#include "service.h"

#include "advertising_cfg.h"
#include "advertising.h"
#include "nrf_advertiser.h"
#include "rigdfu.h"
#include "ts_peripheral.h"

#define ADV_MODE_BEACON                 0
#define ADV_MODE_CONNECTABLE            1
#define DEFAULT_TX_POWER                0

#define APP_BEACON_INFO_LENGTH          0x17                                /**< Total length of information advertised by the Beacon. */
#define APP_ADV_DATA_LENGTH             0x15                                /**< Length of manufacturer specific data in the advertisement. */
#define APP_DEVICE_TYPE                 0x02                                /**< 0x02 refers to Beacon. */
#define APP_MEASURED_RSSI               0xC3                                /**< The Beacon's measured RSSI at 1 meter distance in dBm. */
#define APP_COMPANY_IDENTIFIER          0x004C                              /**< Company identifier for Apple Inc. as per www.bluetooth.org. */
#define APP_MAJOR_VALUE                 0x01, 0x02                          /**< Major value used to identify Beacons. */ 
#define APP_MINOR_VALUE                 0x03, 0x04                          /**< Minor value used to identify Beacons. */ 
#define APP_BEACON_UUID                 0x01, 0x12, 0x23, 0x34, \
                                        0x45, 0x56, 0x67, 0x78, \
                                        0x89, 0x9a, 0xab, 0xbc, \
                                        0xcd, 0xde, 0xef, 0xf0              /**< Proprietary UUID for Beacon. */
                                        
#define MAJ_VAL_OFFSET_IN_BEACON_INFO    18                                 /**< Position of the MSB of the Major Value in m_beacon_info array. */

#define ADVERTISING_RESTART_TIMEOUT_MS  (125u)                              /**< Milliseconds to wait before restaring advertising. */                          

static uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH] =                     /**< Information advertised by the Beacon. */
{
    APP_DEVICE_TYPE,     // Manufacturer specific information. Specifies the device type in this 
                         // implementation. 
    APP_ADV_DATA_LENGTH, // Manufacturer specific information. Specifies the length of the 
                         // manufacturer specific data in this implementation.
    APP_BEACON_UUID,     // 128 bit UUID value. 
    APP_MAJOR_VALUE,     // Major arbitrary value that can be used to distinguish between Beacons. 
    APP_MINOR_VALUE,     // Minor arbitrary value that can be used to distinguish between Beacons. 
    APP_MEASURED_RSSI    // Manufacturer specific information. The Beacon's measured TX power in 
                         // this implementation. 
};

static ble_gap_adv_params_t m_adv_params;

#if (SDK_VERSION>=11)
    APP_TIMER_DEF(m_restart_timer);
#else
    #error "nRF IC Not Defined!"
#endif

static bool m_restart_triggered = false;

static void advertising_start_connectable_adv(void);
static void restart_timer_timeout(void * p_context);

static bool validTXPower(const int8_t tx_power)
{
	const int8_t valid_tx_power_list[] = { -40, -30, -20, -16, -12, -8, -4, 0, 4 };
	
	bool valid = false;
	
	for(uint8_t i = 0; i < sizeof(valid_tx_power_list); i++)
	{
		if(valid_tx_power_list[i] == tx_power)
		{
			valid = true;
			break;
		}
	}
	
	return valid;
}

static void update_beacon_tx_power(void)
{
    const default_app_settings_t * cur_settings = storage_intf_get();
    int8_t tx_power = DEFAULT_TX_POWER;
	
    if( validTXPower(cur_settings->beacon_tx_power) )
    {
        tx_power = cur_settings->beacon_tx_power;
        periph_radio_power_bin_set(tx_power);
    }
}

static void update_connectable_tx_power(void)
{
    const default_app_settings_t * cur_settings = storage_intf_get();
	int8_t tx_power = DEFAULT_TX_POWER;

    if( validTXPower(cur_settings->beacon_tx_power) )
    {
        tx_power = cur_settings->connectable_tx_power;
        sd_ble_gap_tx_power_set(tx_power);
	}
    
        
}


void advertising_init(void)
{
    btle_hci_adv_init();
    
    uint32_t err_code = app_timer_create(&m_restart_timer, APP_TIMER_MODE_SINGLE_SHOT, restart_timer_timeout);
    APP_ERROR_CHECK(err_code);
}

void advertising_restart(void)
{
    if(service_get_connected_state())
    {
        return;
    }
    if(!m_restart_triggered)
    {
        advertising_stop_connectable_adv();
    
        uint32_t err_code = app_timer_start(m_restart_timer, APP_TIMER_TICKS(ADVERTISING_RESTART_TIMEOUT_MS, 0), NULL);
        APP_ERROR_CHECK(err_code);
        
        m_restart_triggered = true;
    }
}

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
void advertising_init_beacon(void)
{
    btle_cmd_param_le_write_advertising_parameters_t    adv_params;
    btle_cmd_param_le_write_advertising_data_t          adv_data;
    const default_app_settings_t                        * settings;
    
    settings = storage_intf_get();
	
    rigado_get_ficr(&adv_params.direct_address[0], true);
    
		/* want to maximize potential scan requests */
		adv_params.channel_map = BTLE_CHANNEL_MAP_ALL;
		adv_params.direct_address_type = BTLE_ADDR_TYPE_RANDOM;
		adv_params.filter_policy = BTLE_ADV_FILTER_ALLOW_ANY;
		adv_params.interval_min = MSEC_TO_UNITS(settings->adv_interval, UNIT_0_625_MS);
		adv_params.interval_max = MSEC_TO_UNITS(settings->adv_interval + 50, UNIT_0_625_MS);
		
		adv_params.own_address_type = BTLE_ADDR_TYPE_RANDOM;
		
		/* Only want scan requests */
		adv_params.type = BTLE_ADV_TYPE_NONCONN_IND;

		btle_hci_adv_params_set(&adv_params);

    /* iBeacon Style broadcast */
    if(settings->beacon_data_len == 0)
    {
        memcpy(&m_beacon_info[2], &settings->uuid, sizeof(ble_uuid128_t));
        
        uint8_t index = MAJ_VAL_OFFSET_IN_BEACON_INFO;

        m_beacon_info[index++] = MSB_16(settings->major);
        m_beacon_info[index++] = LSB_16(settings->major);

        m_beacon_info[index++] = MSB_16(settings->minor);
        m_beacon_info[index++] = LSB_16(settings->minor);    
        
        m_beacon_info[index++] = settings->rssi_cal.rssi;
        
        adv_data.data_length = APP_BEACON_INFO_LENGTH + 7;
        adv_data.advertising_data[0] = 0x02;
        adv_data.advertising_data[1] = 0x01; 
        adv_data.advertising_data[2] = 0x04;
        adv_data.advertising_data[3] = 0x1A;
        adv_data.advertising_data[4] = 0xFF;
        adv_data.advertising_data[5] = APP_COMPANY_IDENTIFIER & 0xFF;
        adv_data.advertising_data[6] = (APP_COMPANY_IDENTIFIER >> 8) & 0xFF;
        memcpy((void*) &adv_data.advertising_data[7], (void*) &m_beacon_info, APP_BEACON_INFO_LENGTH);
    }
    else
    {
        adv_data.data_length = settings->beacon_data_len;
        memcpy(adv_data.advertising_data, settings->beacon_data, settings->beacon_data_len);
    }
    
    btle_hci_adv_data_set(&adv_data);
	
		/* all parameters are set up, enable advertisement */
		btle_hci_adv_enable(BTLE_ADV_ENABLE);
}

/* Set up advertising data for non-beacon advertisement */
void advertising_init_non_beacon(void)
{
    uint32_t        err_code;
    ble_advdata_t   advdata;
    ble_advdata_t   scandata;
    uint8_t         flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    ble_uuid_t      ble_uuid;
    ble_uuid_t      ble_nus;
    bool            uart_enabled;
    
    uart_enabled = sys_init_is_uart_enabled();
    
    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));
    memset(&scandata, 0, sizeof(scandata));
    
    if(uart_enabled)
    {
        ble_nus.type = ble_nus_uuid_type;
        ble_nus.uuid = BLE_UUID_NUS_SERVICE;
    }
    
    ble_uuid.type = ble_beacon_config_get_uuid_type();
    ble_uuid.uuid = ((ble_beacon_config_service_uuid128.uuid128[12]) + (ble_beacon_config_service_uuid128.uuid128[13] << 8));
    
    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = false;
    advdata.flags                   = flags;
        
    if(uart_enabled)
    {
        advdata.uuids_complete.uuid_cnt = 1;
        advdata.uuids_complete.p_uuids  = &ble_nus;
        
        scandata.name_type                  = BLE_ADVDATA_NO_NAME;
        scandata.include_appearance         = false;
        
        scandata.uuids_complete.uuid_cnt    = 1;
        scandata.uuids_complete.p_uuids     = &ble_uuid;
    
        err_code = ble_advdata_set(&advdata, &scandata);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        advdata.uuids_complete.uuid_cnt = 1;
        advdata.uuids_complete.p_uuids  = &ble_uuid;
        
        err_code = ble_advdata_set(&advdata, NULL);
        APP_ERROR_CHECK(err_code);
    }
    
    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.p_peer_addr = NULL;                             // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.timeout     = APP_CFG_CONN_ADV_TIMEOUT;
    
    const default_app_settings_t * cur_settings = storage_intf_get();
    
    #ifdef BMD_DEBUG
        uint32_t adv_intvl_ms  = 100;
    #else
        uint32_t adv_intvl_ms  = cur_settings->conn_adv_interval;
    #endif
    m_adv_params.interval    = MSEC_TO_UNITS(adv_intvl_ms, UNIT_0_625_MS);
}

void advertising_start(void)
{
    const default_app_settings_t * settings;
    
    settings = storage_intf_get();
    
    if(settings->connectable_adv_enabled)
    {
        advertising_start_connectable_adv();
    }
    
    if(settings->enable == true)
    {
		update_beacon_tx_power();
        advertising_init_beacon();
    }
    else
    {
        btle_hci_adv_enable(BTLE_ADV_DISABLE);
    }
}

static void advertising_start_connectable_adv(void)
{
    advertising_init_non_beacon();
    update_connectable_tx_power();
    uint32_t err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);
}

void advertising_stop_beacon(void)
{
    btle_hci_adv_enable(BTLE_ADV_DISABLE);
}

void advertising_stop_connectable_adv(void)
{
    sd_ble_gap_adv_stop();
}

static void restart_timer_timeout(void * p_context)
{
    advertising_start();
    m_restart_triggered = false;
}
