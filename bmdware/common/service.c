/** @file service.c
*
* @brief This module initializes all BMDware services
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <string.h>
#include "nrf_soc.h"
#include "app_error.h"
#include "ble_gap.h"
#include "ble_beacon_config.h"
#include "ble_nus.h"
#include "ble_dis.h"
#include "uart.h"
#include "timer.h"
#include "version.h"
#include "nrf_gpio.h"
#include "gpio_ctrl.h"

#include "bmdware_gpio_def.h"
#ifdef NRF52
    #include "bmd_dtm.h"
#elif defined(NRF51)
    #include "bmd200_dtm.h"
#endif
#include "service.h"
#include "sys_init.h"

#include "rigdfu.h"

#define UART_SEND_TIMEOUT_MS    50

static ble_beacon_config_t m_beacon_config;
static ble_nus_t m_nus;
static ble_nus_t * mp_nus;
static bool m_connected = false;

static void init_uart_service(void)
{
    ble_nus_init_t nus_init;
    uint32_t err_code;
    
    memset(&nus_init, 0, sizeof(nus_init));
    
    nus_init.data_handler = uart_ble_data_handler;
        
    timer_create_uart( uart_ble_timeout_handler, UART_SEND_TIMEOUT_MS, true );
    
    if(sys_init_is_uart_enabled())
    {
        err_code = ble_nus_init(&m_nus, &nus_init);
        APP_ERROR_CHECK(err_code);
        
        err_code = ble_nus_set_beacon_config_ptr(&m_beacon_config);
        APP_ERROR_CHECK(err_code);
        
        mp_nus = &m_nus;
    }
}


/**@brief Function for initializing services that will be used by the application.
 */
void services_init(void)
{
    uint32_t err_code;
    ble_beacon_config_init_t cfg_init;
    ble_dis_init_t dis_init;
    
    memset(&cfg_init, 0, sizeof(cfg_init));
    memset(&dis_init, 0, sizeof(dis_init));

    cfg_init.support_notification = true;
    cfg_init.p_report_ref = NULL;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cfg_init.beacon_config_control_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cfg_init.beacon_config_control_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cfg_init.beacon_config_control_char_attr_md.write_perm);
    
    err_code = ble_beacon_config_init(&m_beacon_config, &cfg_init);
    APP_ERROR_CHECK(err_code);
    
    init_uart_service();
    
    // Initialize Device Information Service    
    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, MFG_NAME);
    ble_srv_ascii_to_utf8(&dis_init.fw_rev_str, FIRMWARE_VERSION_STRING );
    ble_srv_ascii_to_utf8(&dis_init.serial_num_str, (char*)rigado_get_mac_string() );
    ble_srv_ascii_to_utf8(&dis_init.model_num_str, MODEL_STRING );
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
    
    //set the mac
    ble_gap_addr_t addr;
    memset(&addr,0,sizeof(addr));
    
    if(rigado_get_mac(addr.addr))
    {
        addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
    }
    else
    {
        addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    }
    
    #ifdef BMD_DEBUG
        for(int i=0; i<6; i++)
        {
            addr.addr[i] = 0xFE;
        }
        addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    #endif
    
    #if (NRF_SD_BLE_API_VERSION == 3) && defined(S132)
        err_code = sd_ble_gap_addr_set(&addr);
    #else // S130
        err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
    #endif
    
    APP_ERROR_CHECK(err_code);
}

/* return beacon config pointer to allow updating characteristics */
ble_beacon_config_t * services_get_beacon_config_obj(void)
{
	return &m_beacon_config;
}

/* return nus config structure to allow updating characteristics */
ble_nus_t * services_get_nus_config_obj(void)
{
	return mp_nus;
}

void services_beacon_config_evt(ble_evt_t * p_ble_evt)
{
    ble_beacon_config_on_ble_evt(&m_beacon_config, p_ble_evt);
}

void services_ble_nus_evt(ble_evt_t * p_ble_evt)
{
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
}

void service_set_connected_state(bool state)
{
    m_connected = state;
    service_update_status_pin();
}

bool service_get_connected_state(void)
{
    return m_connected;
}

void service_update_status_pin(void)
{
    gpio_ctrl_status_pin_set_state((uint8_t)m_connected);
}
