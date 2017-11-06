/** @file main.c
*
* @brief BMDware Main Application file
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdbool.h>
#include <stdint.h>

#include "ble_advdata.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "ble_beacon_config.h"
#include "ble_debug_assert_handler.h"
#include "fstorage.h"
#include "advertising.h"
#include "gap.h"
#include "gap_cfg.h"
#include "service.h"
#include "storage_intf.h"
#include "lock.h"
#include "timer.h"
#include "at_commands.h"
#include "uart.h"
#include "sw_irq_manager.h"

#include "bmdware_gpio_def.h"
#include "sys_init.h"
#include "nrf_gpiote.h"
#include "ble_dtm.h"
#include "ble_conn_params.h"
#include "bmd_dtm.h"
#include "nrf_delay.h"
#include "nrf_advertiser.h"
#include "nrf_gpio.h"
#include "gpio_ctrl.h"
#include "gatt.h"
#include "version.h"
#include "bmd_log.h"

#define CENTRAL_LINK_COUNT              0                                   /**< Number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                   /**< Number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define IS_SRVC_CHANGED_CHARACT_PRESENT 1                                   /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define ADVERTISING_LED_PIN_NO          LED_0                               /**< Is on when device is advertising. */

#define DEAD_BEEF                       0xDEADBEEF                          /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static ble_gap_conn_params_t            m_preferred_conn_params;
static uint32_t                         m_conn_handle;

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
            uart_reset_counters();
            bmd_log("BLE_GAP_EVT_CONNECTED\n");
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
            gatt_set_runtime_mtu(GATT_MTU_SIZE_DEFAULT);
            service_set_connected_state(false);
            bmd_log("BLE_GAP_EVT_DISCONNECTED\n");
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
        
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

    #ifdef S132
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            {
                /* rx mtu is the mtu supported by client */
                uint16_t rx_mtu = p_ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
                if(rx_mtu < GATT_EXTENDED_MTU_SIZE)
                {
                    gatt_set_runtime_mtu(rx_mtu);
                    err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, rx_mtu);
                }
                else
                {
                    gatt_set_runtime_mtu(GATT_EXTENDED_MTU_SIZE);
                    err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, GATT_EXTENDED_MTU_SIZE);
                }
                
                bmd_log("runtime_mtu: %d\n", gatt_get_runtime_mtu());
                
                APP_ERROR_CHECK(err_code);
            }
            break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
    #endif
            
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
    ble_conn_params_on_ble_evt(p_ble_evt);
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
    fs_sys_event_handler(sys_evt);
    btle_hci_adv_sd_evt_handler(sys_evt);
}

/**@brief     Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error Error code.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    m_preferred_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;
    m_preferred_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    m_preferred_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    m_preferred_conn_params.slave_latency = SLAVE_LATENCY;
    
    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = &m_preferred_conn_params;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = NULL;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
    
    nrf_clock_lf_cfg_t clock_lf_cfg = { NRF_CLOCK_LF_SRC_RC, 16, 4, 0 };
    
    // Initialize the SoftDevice handler module.	
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
#ifdef S132
    ble_enable_params.gatt_enable_params.att_mtu = GATT_EXTENDED_MTU_SIZE;
#endif
    
    ble_enable_params.gatts_enable_params.service_changed = 1;
    ble_enable_params.common_enable_params.vs_uuid_count = 2;
    ble_enable_params.gatts_enable_params.attr_tab_size = 0x800;

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
#ifdef S132
    ble_opt_t ble_opt;
    ble_opt.gap_opt.ext_len.rxtx_max_pdu_payload_size = 54; //Example: set max length to 54bytes
    sd_ble_opt_set(BLE_GAP_OPT_EXT_LEN, &ble_opt);
#endif
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
    sw_irq_manager_init();
	timers_init();
    
	ble_stack_init();
    storage_intf_init();
    gap_params_init();
    gpio_ctrl_init();
	lock_init();

	services_init();
    conn_params_init();
    advertising_init();
    
	advertising_init_non_beacon();
    
    at_commands_init();
    
    /* init the at/passthrough mode */
    sys_init_setup_at_mode_pin();
    
    if(sys_init_is_at_mode())
    {
        uart_configure_at_mode();
    }
    else
    {
        const default_app_settings_t * const settings = storage_intf_get();
        if(settings->uart_enable)
        {
            uart_configure_passthrough_mode(services_get_nus_config_obj());
        }
    }

	//Start execution.
	advertising_start();
}

static void DTM_Init(void)
{
	timers_init();
    uart_configure_direct_test_mode();
	bmd_dtm_init();
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{	
#ifdef NRF52
    /* Enable DCDC */
	NRF_POWER->DCDCEN = 0x01;
    
    nrf_gpio_cfg_input(BMD_SOFT_RESET_PIN, NRF_GPIO_PIN_PULLUP);
    if(nrf_gpio_pin_read(BMD_SOFT_RESET_PIN) == 0)
    {
        NVIC_SystemReset();
    }
#endif
    
    bmd_log_init();
    bmd_log("bmdware %s\n", FIRMWARE_VERSION_STRING);
    
    sys_init();
    
    if(sys_init_is_dtm())
    {
        DTM_Init();
    }
    else
    {
        BMDware_Init();
    }
    
    #if BMD200_EVAL_V31
        app_timer_start(m_led_timer, APP_TIMER_TICKS(500, 0), NULL);
    #endif
    
	bool m_was_at_mode = sys_init_is_at_mode();
	for (;;)
	{        
        uart_mode_t uart_mode = uart_get_mode();
        if(UART_MODE_DTM == uart_mode)
        {
            dtm_wait();
        }
        else
        {
            bool is_at_mode = sys_init_is_at_mode();
            if(is_at_mode != m_was_at_mode)
            {
                if(is_at_mode)
                {
                    uart_configure_at_mode();
                }
                else
                {
                    const default_app_settings_t * settings = 
                        storage_intf_get();
                    if(settings->uart_enable)
                    {
                        uart_configure_passthrough_mode(
                            services_get_nus_config_obj());
                    }
                    else
                    {
                        uart_deinit();
                    }
                }
            }
            m_was_at_mode = is_at_mode;
        }
        
        if(UART_MODE_BMDWARE_PT == uart_mode || 
            UART_MODE_DTM == uart_mode)
        {
            uart_transfer_data();           
		}
        else if(UART_MODE_BMDWARE_AT == uart_mode)
        {
            at_proc_process_command();
        }
        
        if(UART_MODE_DTM != uart_mode)
        {
            power_manage();
        }
	}
}
/**
 * @}
 */
