/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file main.c
*
* @brief BMDware main module
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdbool.h>
#include <stdint.h>
#include "ble_advdata.h"
//#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "ble_beacon_config.h"
#include "ble_debug_assert_handler.h"
#include "pstorage_platform.h"
#include "pstorage.h"
#include "advertising.h"
#include "gap.h"
#include "gap_cfg.h"
#include "service.h"
#include "storage_intf.h"
#include "lock.h"
#include "timer.h"
#include "at_commands.h"
#include "uart.h"
#include "gpio_ctrl.h"
#ifdef BMD200_EVAL_V31
#include "app_timer.h"
#endif

#include "bmdware_gpio_def.h"
#include "sys_init.h"
#include "nrf_gpiote.h"
#include "ble_dtm.h"
#include "ble_conn_params.h"
#include "bmd200_dtm.h"
#include "nrf_delay.h"
#include "sw_irq_manager.h"
#include "nrf_advertiser.h"
#include "nrf_gpio.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT 1                                   /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define ADVERTISING_LED_PIN_NO          LED_0                               /**< Is on when device is advertising. */

#define DEAD_BEEF                       0xDEADBEEF                          /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static uint32_t                         m_conn_handle;

#ifdef BMD200_EVAL_V31
    static app_timer_id_t                   m_led_timer;
#endif

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t        err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            btle_hci_adv_enable(BTLE_ADV_DISABLE);
            #ifdef BMD200_EVAL_V31
                app_timer_stop(m_led_timer);
                nrf_gpio_pin_set(BMD_LED_GREEN);
                nrf_gpio_pin_clear(BMD_LED_RED);
            #endif
            service_set_connected_state(true);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            if( storage_intf_is_dirty() )
            {
                err_code = storage_intf_save();
                APP_ERROR_CHECK(err_code);
            }
        
            m_conn_handle = BLE_CONN_HANDLE_INVALID;  
            lock_set();
            advertising_start();
            #ifdef BMD200_EVAL_V31
                app_timer_start(m_led_timer, APP_TIMER_TICKS(500, 0), NULL);
                nrf_gpio_pin_clear(BMD_LED_GREEN);
            #endif
            
            uart_clear_buffer();
            service_set_connected_state(false);
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if( storage_intf_is_dirty() )
            {
                err_code = storage_intf_save();
                APP_ERROR_CHECK(err_code);
            }
						
            /* advertising timeout, we switch advertising modes when we start again */
            advertising_start();
            break;
        
        /* Note - With no device manager, this event response is 100000% NECESSARY to read
         * the value of the Client Characterisitic Configuration (aka Notifications) !!!! */
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    services_beacon_config_evt(p_ble_evt);
    services_ble_nus_evt(p_ble_evt);
    //ble_conn_params_on_ble_evt(p_ble_evt);
    on_ble_evt(p_ble_evt);
}

/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    if(sys_evt == NRF_EVT_FLASH_OPERATION_SUCCESS ||
        sys_evt == NRF_EVT_FLASH_OPERATION_ERROR)
    {
        pstorage_sys_event_handler(sys_evt);
    }
    else
    {
        btle_hci_adv_sd_evt_handler(sys_evt);
    }
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
    
    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_TEMP_4000MS_CALIBRATION, false);

    // Enable BLE stack 
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    #ifdef TEST_S120
        ble_enable_params.gap_enable_params.role = BTLE_CONNECTION_ROLE_SLAVE;
    #endif
    
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
        
    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // Register with the SoftDevice handler module for system events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for doing power management.
 */
static void power_manage(void)
{
	uint8_t sden = 0;
	if( sd_softdevice_is_enabled(&sden) == NRF_SUCCESS
			&& sden )
	{
        uint32_t err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
	}
	else
	{
		__WFI();
	}
}

#ifdef BMD200_EVAL_V31
static void led_timer_timeout_handler(void * p_context)
{
    nrf_gpio_pin_toggle(BMD_LED_RED);
}
#endif

static void BMDware_Init(void)
{
	timers_init();
    
    #ifdef BMD200_EVAL_V31
        app_timer_create(&m_led_timer, APP_TIMER_MODE_REPEATED, led_timer_timeout_handler);
	#endif
    
	ble_stack_init();
	storage_intf_init();
	gap_params_init();
    gpio_ctrl_init();
    lock_init();

	services_init();
    advertising_init();
    
	advertising_init_non_beacon();
    
    at_commands_init();
    
    #if defined (BMD200_EVAL_V31) || defined (BMDWARE_SCS_001)
    nrf_gpio_cfg_output(BMD_LED_GREEN);
    nrf_gpio_cfg_output(BMD_LED_RED);
    nrf_gpio_cfg_output(BMD_LED_BLUE);
  
    nrf_gpio_pin_clear(BMD_LED_GREEN);
    nrf_gpio_pin_clear(BMD_LED_RED);
    nrf_gpio_pin_clear(BMD_LED_BLUE);
    #endif
    
    #if BMD200_EVAL_V31
        app_timer_start(m_led_timer, APP_TIMER_TICKS(500, 0), NULL);
    #endif

	//Start execution.
	advertising_start();
}

static void DTM_Init(void)
{
	timers_init();
    uart_init_dtm();
	bmd_dtm_init();
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{	
    /* Enable DCDC */
	//NRF_POWER->DCDCEN = NRF_POWER_DCDC_MODE_ON;

    sys_init();
    
    sw_irq_manager_init();
    
    if(sys_init_is_dtm())
    {
        DTM_Init();
    }
    else
    {
        BMDware_Init();
    }
    
	// Enter main loop.
	for (;;)
	{        
        if( sys_init_is_dtm() )
        {
            dtm_wait();
            //uart xfer
            uart_transfer_data();
        }
        else if( sys_init_is_uart_enabled() )
        {
            if( uart_get_mode() == UART_MODE_BMDWARE 
						&& at_proc_is_cmd_ready())
            {
                at_proc_process_command();
            }
            
            //uart xfer
            uart_transfer_data();
            
            power_manage();
		}
        else
        {
            if( uart_get_mode() == UART_MODE_BMDWARE 
						&& at_proc_is_cmd_ready())
            {
                at_proc_process_command();
            }
            
            power_manage();
        }
	}
}
/**
 * @}
 */
