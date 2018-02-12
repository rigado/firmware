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

/**@file
 *
 * @defgroup ble_sdk_app_bootloader_main main.c
 * @{
 * @ingroup dfu_bootloader_api
 * @brief Bootloader project main file.
 *
 * -# Receive start data packet. 
 * -# Based on start packet, prepare NVM area to store received data. 
 * -# Receive data packet. 
 * -# Validate data packet.
 * -# Write Data packet to NVM.
 * -# If not finished - Wait for next packet.
 * -# Receive stop data packet.
 * -# Activate Image, boot application.
 *
 */
#include "bootloader.h"
#include "bootloader_types.h"
//#include "bootloader_util.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "nordic_common.h"
#include "nrf.h"
#ifdef SDK10
    #include "nrf51.h"
    #include "nrf51_bitfields.h"
#else
    #include "nrf_soc.h"
#endif
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "nrf_error.h"
#include "boards.h"
#include "ble_debug_assert_handler.h"
#include "softdevice_handler.h"
#include "nrf_mbr.h"
#include "fstorage.h"
#include "nrf_clock.h"
#include "dfu.h"

#include "hci_mem_pool_internal.h"
#ifndef MEM_POOL_CORRECT_INCLUDE
#error Got the wrong hci_mem_pool_internal.h -- check include paths
#endif

#include "rigdfu_serial.h"
#include "nrf_delay.h"
#include "version.h"
#include "rigdfu.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT 1                                                       /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define APP_GPIOTE_MAX_USERS            1                                                       /**< Number of GPIOTE users in total. Used by button module and dfu_transport_serial module (flow control). */

#define APP_TIMER_PRESCALER             0                                                       /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            3                                                       /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                                       /**< Size of timer operation queues. */


#define APP_TIMER_SCHED_EVT_SIZE        sizeof(app_timer_evt_schedule_func_t)                   /**< Size of button events being passed through the scheduler (is to be used for computing the maximum size of scheduler events). */
#define SCHED_MAX_EVENT_DATA_SIZE       MAX(APP_TIMER_SCHED_EVT_SIZE, 0)                        /**< Maximum size of scheduler events. */

#define SCHED_QUEUE_SIZE                20                                                      /**< Maximum number of events in the scheduler queue. */

#define TICKS_FROM_MSEC(x) APP_TIMER_TICKS(x, APP_TIMER_PRESCALER)

#ifdef SDK10
/**@brief Function for error handling, which is called when an error has occurred. 
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze 
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name. 
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    //#define DEBUG
    #ifdef DFU_DEBUG
        ble_debug_assert_handler(error_code, line_num, p_file_name);
    #else
        // On assert, the system can only recover on reset.
        NVIC_SystemReset();
    #endif
}
#endif

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] file_name   File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}


/**@brief Function for initializing the GPIOTE handler module.
 */
//static void gpiote_init(void)
//{
//    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
//}


/**@brief Function for initializing the timer handler module (app_timer).
 */
static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void sys_evt_dispatch(uint32_t event)
{
    fstorage_sys_event_handler(event);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 *
 * @param[in] init_softdevice  true if SoftDevice should be initialized. The SoftDevice must only
 *                             be initialized if a chip reset has occured. Soft reset from
 *                             application must not reinitialize the SoftDevice.
 */
#ifdef SDK10
static void ble_stack_init(bool init_softdevice)
{
    uint32_t err_code;
    sd_mbr_command_t com = {SD_MBR_COMMAND_INIT_SD, };

    if (init_softdevice)
    {
        err_code = sd_mbr_command(&com);
        APP_ERROR_CHECK(err_code);
    }

    err_code = sd_softdevice_vector_table_base_set(BOOTLOADER_REGION_START);
    APP_ERROR_CHECK(err_code);

    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_TEMP_4000MS_CALIBRATION, NULL);

    // Enable BLE stack
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
#ifdef S120
    ble_enable_params.gap_enable_params.role = BLE_GAP_ROLE_PERIPH;
#endif
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}
#else
static void ble_stack_init(bool init_softdevice)
{
    uint32_t         err_code;
    sd_mbr_command_t com = {SD_MBR_COMMAND_INIT_SD, };
    nrf_clock_lf_cfg_t clock_lf_cfg = { NRF_CLOCK_LF_SRC_RC, 16, 2, 0 };

    if (init_softdevice)
    {
        err_code = sd_mbr_command(&com);
        APP_ERROR_CHECK(err_code);
    }
    
    err_code = sd_softdevice_vector_table_base_set(BOOTLOADER_REGION_START);
    APP_ERROR_CHECK(err_code);
   
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    // Only one connection as a central is used when performing dfu.
    err_code = softdevice_enable_get_default_config(0, 1, &ble_enable_params);
#ifdef SDK12
    ble_enable_params.gatt_enable_params.att_mtu = BLE_GAP_MTU_MAX;
#endif
    APP_ERROR_CHECK(err_code);

    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}
#endif

/**@brief Function for event scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(12, SCHED_QUEUE_SIZE);
}


#if defined(NRF52)
#define READBACK_PROTECTION_ENABLE      (0x00000000)
#define READBACK_PROTECTION_ENABLED     0x00
#define READBACK_PROTECTION_DISABLED    0xFF
#define READBACK_PROTECTION_MASK        0xFF
static void update_readback_protection(void)
{
    if((NRF_UICR->APPROTECT & READBACK_PROTECTION_MASK) == READBACK_PROTECTION_DISABLED)
    {
        NRF_NVMC->CONFIG = 0x01;
        NRF_UICR->APPROTECT = 0x00;
        while(NRF_NVMC->READY == 0);
        nrf_delay_ms(10);
        NVIC_SystemReset();
    }
}
#else
static void update_readback_protection(void) {}
#endif

/**@brief Function for bootloader main entry.
 */
int main(void)
{
    uint32_t err_code;
    uint8_t gpregret;

    #if defined(NRF52)
    const uint8_t *key;
    //check for readback protection
    if(rigado_get_key(&key)) 
    {
        update_readback_protection();
    }
    #endif    
    
    bool triggered_from_app = false;
    bool force_uart_init = false;
    
    gpregret = NRF_POWER->GPREGRET;

    if (gpregret >= BOOTLOADER_APP_START_MIN &&
        gpregret <= BOOTLOADER_APP_START_MAX &&
        bootloader_app_is_valid()) {
        /* Either the bootloader, or the app, requested a direct
           reboot into the application, and then triggered a reset.
           Launch the application now. */
        NRF_POWER->GPREGRET &= BOOTLOADER_APP_START_MASK;
        bootloader_launch_app_after_reset();
    }

    /* If the app sets BOOTLOADER_DFU_START and jumps to the bootloader,
       the softdevice is already initialized, which we need to propagate
       in order to avoid reinitializing it.  Note that a system reset
       should not have been triggered in this case. */
    if (gpregret == BOOTLOADER_DFU_START)
        triggered_from_app = true;
    
    if (gpregret == BOOTLOADER_DFU_START_W_UART)
    {
        triggered_from_app = true;
        force_uart_init = true;        
    }

    /* This delay is important in case the code reboots too quickly --
       without it, it may be difficult to connect to the chip via SWD,
       because flash operations can trigger SWD WAIT responses on the
       debug interface.  Should be safe to remove in production, if
       needed. */
    nrf_delay_ms(100);

    // Initialize.
    timers_init();

    /******************************/

    (void)bootloader_init();

    /******************************/

    if (bootloader_dfu_sd_in_progress())
    {
        err_code = bootloader_dfu_sd_update_continue();
        APP_ERROR_CHECK(err_code);

        ble_stack_init(true);
        scheduler_init();

        err_code = bootloader_dfu_sd_update_finalize();
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        // If stack is present then continue initialization of bootloader.
        ble_stack_init(true);//!triggered_from_app);
        scheduler_init();
    }

#ifdef SDK12
    err_code = sd_power_gpregret_clr(0, POWER_GPREGRET_GPREGRET_Msk);
#else
    err_code = sd_power_gpregret_clr(POWER_GPREGRET_GPREGRET_Msk);
#endif
    APP_ERROR_CHECK(err_code);

    /* Timeouts for DFU */
    int t_initial  = TICKS_FROM_MSEC(2000); /* before initial connection */
    int t_firstcmd = TICKS_FROM_MSEC(15000); /* before first DFU command */
    int t_nextcmd  = TICKS_FROM_MSEC(10000); /* between each DFU cmd */

    if (triggered_from_app || !bootloader_app_is_valid())
    {
        /* DFU requested by app, or app is invalid -- be more patient */
        t_initial  = TICKS_FROM_MSEC(120000);
    }
    
    rigdfu_serial_init(force_uart_init);

    bool try_app = bootloader_dfu_start(t_initial, t_firstcmd, t_nextcmd);
    rigdfu_serial_close();

    if (try_app) {
        /* Timed out, or a new app was sent.  Run it if valid. */
        if (bootloader_app_is_valid())
            bootloader_app_start();
    }

    /* Either the app is invalid, or the bootloader completed for a
       different reason (e.g. needing to reboot to finish SD or BL
       update).  Reboot now. */
    softdevice_handler_sd_disable();
    nrf_delay_ms(10);

    if(dfu_should_check_readback_protect())
    {
        update_readback_protection();
    }
    
    nrf_delay_ms(10);

    NVIC_SystemReset();
}
