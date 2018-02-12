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

#include "bootloader.h"
#include <string.h>
#include "bootloader_types.h"
#ifdef SDK12
#include "nrf_bootloader_app_start.h"
#else
//#include "bootloader_util.h"
#endif
#include "bootloader_settings.h"
#include "dfu.h"
#include "dfu_transport_ble.h"
#include "dfu_transport_serial.h"
#ifdef SDK10
#include "nrf51.h"
#else
#include "nrf_soc.h"
#endif
#include "app_error.h"
#include "nrf_sdm.h"
#include "ble_flash.h"
#include "nordic_common.h"
#include "crc16.h"
#include "fstorage.h"
#include "app_scheduler.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "rigdfu_serial.h"
#include "nrf_mbr.h"
#include "ble_gap.h"
#include "rigdfu.h"

APP_TIMER_DEF(m_bootloader_timer_id);
static uint32_t m_timeout_dfu_ticks;
static uint32_t m_timeout_connect_ticks;

/* If true, using the BLE transport. */
static enum { BLE, SERIAL } m_transport;

/**@brief Enumeration for specifying current bootloader status.
 */
typedef enum
{
    BOOTLOADER_INIT,                                    /**< Waiting for host to start DFU */
    BOOTLOADER_UPDATING,                                /**< Bootloader status for indicating that an update is in progress. */
    BOOTLOADER_SETTINGS_SAVING,                         /**< Bootloader status for indicating that saving of bootloader settings is in progress. */
    BOOTLOADER_COMPLETE,                                /**< Bootloader status for indicating that all operations for the update procedure has completed and it is safe to reset the system. */
    BOOTLOADER_TIMEOUT,                                 /**< Bootloader status field for indicating that a timeout has occured and current update process should be aborted. */
    BOOTLOADER_RESET,                                   /**< Bootloader status field for indicating that a reset has been requested and current update process should be aborted. */
} bootloader_status_t;

static bootloader_status_t      m_update_status;        /**< Current update status for the bootloader module to ensure correct behaviour when updating settings and when update completes. */

/* If true, reboot rather than start the app. */
static bool m_need_reboot;

/* Helper function set the mac address  */
/* static void ble_set_mac(void);       */

/**@brief Function for handling the DFU timeout.
 *
 * @param[in] p_context The timeout context.
 */
static void _bootloader_timeout_evt(void * p_event_data, uint16_t event_size)
{
    /* Called in thread context */
    dfu_update_status_t update_status;
    update_status.status_code = DFU_TIMEOUT;
    bootloader_dfu_update_process(update_status);
}
static void bootloader_timeout_handler(void *p_context)
{
    /* Only wind things down if we're in the INIT or UPDATING state */
    if (m_update_status == BOOTLOADER_INIT ||
        m_update_status == BOOTLOADER_UPDATING) {
        app_sched_event_put(NULL, 0, _bootloader_timeout_evt);
    }
}

static uint32_t bootloader_timeout_reset_ticks(uint32_t ticks)
{
    uint32_t err;

    if (m_update_status == BOOTLOADER_TIMEOUT)
        return NRF_ERROR_INVALID_STATE;

    /* Stop & restart timer */
    err = app_timer_stop(m_bootloader_timer_id);
    if (err == NRF_SUCCESS)
        err = app_timer_start(m_bootloader_timer_id, ticks, NULL);
    return err;
}

/**@brief   Functions for restarting the bootloader timeout.
 *
 * @details Restart bootloader timeout whenever a DFU packet is received.
 */
uint32_t bootloader_timeout_reset(void)
{
    return bootloader_timeout_reset_ticks(m_timeout_dfu_ticks);
}
uint32_t bootloader_timeout_reset_on_first_connect(void)
{
    if (m_update_status == BOOTLOADER_INIT) {
        if (m_transport == BLE) {
            /* No need to keep serial port open anymore. */
            rigdfu_serial_close();
        }
        m_update_status = BOOTLOADER_UPDATING;
    }
    return bootloader_timeout_reset_ticks(m_timeout_connect_ticks);
}

/**@brief   Disable the bootloader timeout.
 */
void bootloader_timeout_stop(void)
{
    app_timer_stop(m_bootloader_timer_id);
    /* ignore error */
}

/**@brief Function for handling callbacks from fstorage module.
 */
static void fstorage_callback_handler(uint32_t address,
                                      uint8_t op_code,
                                      uint32_t result,
                                      void *p_data)
{
    // If we are in BOOTLOADER_SETTINGS_SAVING state and the store completes,
    // the settings have been saved and we're done.
    if ((m_update_status == BOOTLOADER_SETTINGS_SAVING) &&
        (op_code == FSTORAGE_STORE_OP_CODE))
    {
        m_update_status = BOOTLOADER_COMPLETE;
    }

    APP_ERROR_CHECK(result);
}


/**@brief   Function for waiting for events.
 *
 * @details This function will place the chip in low power mode while waiting for events from
 *          the SoftDevice or other peripherals. When interrupted by an event, it will call the
 *          @ref app_sched_execute function to process the received event. This function will return
 *          when the final state of the firmware update is reached OR when a tear down is in
 *          progress.
 */
static void wait_for_events(void)
{
    for (;;)
    {
        // Wait in low power state for any events.
        uint32_t err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);

        // Event received. Process it from the scheduler.
        app_sched_execute();

        if ((m_update_status == BOOTLOADER_COMPLETE) ||
            (m_update_status == BOOTLOADER_TIMEOUT)  ||
            (m_update_status == BOOTLOADER_RESET))
        {
            // When update has completed or a timeout/reset occured we will return.
            return;
        }
    }
}


bool bootloader_app_is_valid(void)
{
    const bootloader_settings_t *p_bootloader_settings;
    uint32_t *app = (uint32_t *)DFU_BANK_0_REGION_START;
    uint32_t sp = app[0];
    uint32_t reset = app[1];

    /* Bank 0 must be marked as either valid, or unknown (initial
       flash state).  This is to prevent running decrypted-but-
       unauthenticated data. */
    bootloader_util_settings_get(&p_bootloader_settings);
    if (p_bootloader_settings->bank_0 != BANK_VALID_APP &&
        p_bootloader_settings->bank_0 != BANK_UNKNOWN_00 &&
        p_bootloader_settings->bank_0 != BANK_UNKNOWN_FF)
        return false;

    /* Initial SP must be aligned */
    if ((sp & 3) != 0)
        return false;

    /* Initial SP must point within valid RAM (assume 64k limit) */
    if (sp < 0x20000000 || sp > (0x20000000 + 64 * 1024))
        return false;

    /* Reset vector must be thumb mode */
    if ((reset & 1) != 1)
        return false;

    /* Reset vector must point within application */
    if (reset < DFU_BANK_0_REGION_START ||
        reset >= (DFU_BANK_0_REGION_START + DFU_IMAGE_MAX_SIZE_BANKED))
        return false;

    /* Looks good! */
    return true;
}

static void bootloader_settings_save(bootloader_settings_t * p_settings)
{
    uint32_t err_code;

    err_code = fstorage_clear(FSTORAGE_BOOTLOADER,
                              BOOTLOADER_SETTINGS_ADDRESS,
                              sizeof(bootloader_settings_t));
    APP_ERROR_CHECK(err_code);

    err_code = fstorage_store(FSTORAGE_BOOTLOADER,
                              BOOTLOADER_SETTINGS_ADDRESS,
                              p_settings,
                              sizeof(bootloader_settings_t));
    APP_ERROR_CHECK(err_code);
}


void    bootloader_dfu_update_process(dfu_update_status_t update_status)
{
    static bootloader_settings_t  settings;
    const bootloader_settings_t * p_bootloader_settings;

    bootloader_util_settings_get(&p_bootloader_settings);

    if (update_status.status_code == DFU_UPDATE_APP_COMPLETE)
    {
        settings.bank_0_size = update_status.app_size;
        settings.bank_0      = BANK_VALID_APP;
        settings.bank_1      = BANK_INVALID_APP;

        m_update_status      = BOOTLOADER_SETTINGS_SAVING;
        bootloader_settings_save(&settings);
    }
    else if (update_status.status_code == DFU_UPDATE_SD_COMPLETE)
    {
        settings.bank_0_size    = update_status.sd_size +
                                  update_status.bl_size +
                                  update_status.app_size;
        settings.bank_0         = BANK_VALID_SD;
        settings.bank_1         = BANK_INVALID_APP;
        settings.sd_image_size  = update_status.sd_size;
        settings.bl_image_size  = update_status.bl_size;
        settings.app_image_size = update_status.app_size;
        settings.app_image_size = update_status.app_size;
        settings.sd_image_start = update_status.sd_image_start;

        m_update_status         = BOOTLOADER_SETTINGS_SAVING;
        bootloader_settings_save(&settings);

        m_need_reboot = true;
    }
    else if (update_status.status_code == DFU_UPDATE_BOOT_COMPLETE)
    {
        settings.bank_0         = p_bootloader_settings->bank_0;
        settings.bank_0_size    = p_bootloader_settings->bank_0_size;
        settings.bank_1         = BANK_VALID_BOOT;
        settings.sd_image_size  = update_status.sd_size;
        settings.bl_image_size  = update_status.bl_size;
        settings.app_image_size = update_status.app_size;

        m_update_status         = BOOTLOADER_SETTINGS_SAVING;
        bootloader_settings_save(&settings);

        m_need_reboot = true;
    }
    else if (update_status.status_code == DFU_UPDATE_SD_SWAPPED)
    {
        if (p_bootloader_settings->bank_0 == BANK_VALID_APP ||
            p_bootloader_settings->bank_0 == BANK_UNKNOWN_00 ||
            p_bootloader_settings->bank_0 == BANK_UNKNOWN_FF) {
            /* Bank 0 might still be valid, if we only updated the
               bootloader. */
            settings.bank_0         = p_bootloader_settings->bank_0;
            settings.bank_0_size    = p_bootloader_settings->bank_0_size;
        } else {
            settings.bank_0_size    = 0;
            settings.bank_0         = BANK_INVALID_APP;
        }
        settings.bank_1         = BANK_INVALID_APP;
        settings.sd_image_size  = 0;
        settings.bl_image_size  = 0;
        settings.app_image_size = 0;

        m_update_status         = BOOTLOADER_SETTINGS_SAVING;
        bootloader_settings_save(&settings);

        m_need_reboot = true;
    }
    else if (update_status.status_code == DFU_TIMEOUT)
    {
        // Timeout has occurred. Close the connection with the DFU Controller.
        if (m_transport == BLE)
            dfu_transport_close_ble();
        else
            dfu_transport_close_serial();

        m_update_status = BOOTLOADER_TIMEOUT;
    }
    else if (update_status.status_code == DFU_BANK_0_ERASED)
    {
        settings.bank_0_size = 0;
        settings.bank_0      = BANK_ERASED;
        settings.bank_1      = p_bootloader_settings->bank_1;

        bootloader_settings_save(&settings);
    }
    else if (update_status.status_code == DFU_BANK_1_ERASED)
    {
        settings.bank_0      = p_bootloader_settings->bank_0;
        settings.bank_0_size = p_bootloader_settings->bank_0_size;
        settings.bank_1      = BANK_ERASED;

        bootloader_settings_save(&settings);
    }
    else if (update_status.status_code == DFU_RESET)
    {
        // Reset requested. Close the connection with the DFU Controller.
        if (m_transport == BLE)
            dfu_transport_close_ble();
        else
            dfu_transport_close_serial();

        m_update_status = BOOTLOADER_RESET;

        m_need_reboot = true;
    }
    else
    {
        // No implementation needed.
    }
}


uint32_t bootloader_init(void)
{
    uint32_t                err_code;

    fstorage_init();
    err_code = fstorage_register(FSTORAGE_BOOTLOADER,
                                 fstorage_callback_handler);

    return err_code;
}

/* Switch from BLE to serial transport.  Only supported if we're
   still in the "init" phase, before we got a BLE connection. */
static void bootloader_transport_switch(void *p_event_data, uint16_t event_size)
{
    if (m_update_status == BOOTLOADER_INIT && m_transport == BLE) {
        /* Close BLE and switch to serial transport */
        dfu_transport_close_ble();
        m_transport = SERIAL;

        /* Reset timeout */
        bootloader_timeout_reset_on_first_connect();

        /* Start serial transport */
        dfu_transport_update_start_serial();
    }
}

/* Called whenever a byte is received from the UART (for initial host
   detection). Detects the pattern used to trigger serial DFU. */
static void bootloader_uart_rx(uint8_t x)
{
#ifdef RELEASE
    static uint8_t magic_pattern[4] = { 0xca, 0x9d, 0xc6, 0xa4 };
#else
    static uint8_t magic_pattern[4] = { 'a', 's', 'd', 'f' };
#endif
    static uint8_t offset = 0;

    if (x != magic_pattern[offset])
        offset = 0;
    if (x == magic_pattern[offset])
        offset++;

    if (offset == sizeof(magic_pattern)) {
        app_sched_event_put(NULL, 0, bootloader_transport_switch);
        offset = 0;
    }
}

bool bootloader_dfu_start(uint32_t initial_timeout_ticks,
                          uint32_t connect_timeout_ticks,
                          uint32_t dfu_timeout_ticks)
{
    uint32_t err_code;

    m_need_reboot = false;

    // Setup timeout timer.
    err_code = app_timer_create(&m_bootloader_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                bootloader_timeout_handler);
    if (err_code != NRF_SUCCESS)
        return false;

    // Number of ticks to use when resetting the timer in
    // various cases.
    m_timeout_connect_ticks = connect_timeout_ticks;
    m_timeout_dfu_ticks = dfu_timeout_ticks;

    // Initialize DFU, which may clear the swap area
    err_code = dfu_init(DFU_STATE_IDLE);
    if (err_code != NRF_SUCCESS)
        return false;

    // Start the initial timeout timer
    err_code = app_timer_start(m_bootloader_timer_id,
                               initial_timeout_ticks, NULL);
    if (err_code != NRF_SUCCESS)
        return false;

    // Start watching for a signal to switch to the serial transport
    rigdfu_serial_set_rx_handler(bootloader_uart_rx);

    // Start the BLE transport.
    err_code = dfu_transport_update_start_ble();
    
    wait_for_events();

    if (m_need_reboot)
        return false;

    // Timeout, or new app -- OK to load it
    return true;
}

/**@brief Launch the application now, by triggering a reset.
 */
void bootloader_app_start(void)
{
    sd_softdevice_disable();
    NRF_POWER->GPREGRET = BOOTLOADER_APP_START_MIN;
    NVIC_SystemReset();
}

/**@brief Call after reset to actually jump to the application
 */
void bootloader_launch_app_after_reset(void)
{
    /* Protect the softdevice and bootloader from being erased or
       written, using PROTENSET.  The bank addresses may change with
       an updated bootloader, so they must be calculated at runtime.

       Note that we may unintentionally protect the beginning/end
       of the app area, if they reside in the same protection
       block as the softdevice/bootloader.
    */
    uint64_t protect = 0ULL;
    uint32_t addr;
    for (addr = 0; addr < DFU_BANK_0_REGION_START; addr += 0x1000)
        protect |= (1ULL << (addr / 0x1000));
    for (addr = BOOTLOADER_REGION_START; addr < 0x40000; addr += 0x1000)
        protect |= (1ULL << (addr / 0x1000));
    NRF_MPU->PROTENSET1 = (protect >> 32);
    NRF_MPU->PROTENSET0 = (protect & 0xffffffffUL);

    /* Set up the softdevice vector table redirection */
    sd_mbr_command_t com = { SD_MBR_COMMAND_INIT_SD };
    sd_mbr_command(&com);
    sd_softdevice_vector_table_base_set(DFU_BANK_0_REGION_START);

    /* Jump to application */
#if defined(SDK12) || defined(SDK14)
    nrf_bootloader_app_start(DFU_BANK_0_REGION_START);
#else
    bootloader_util_app_start(DFU_BANK_0_REGION_START);
#endif
}

bool bootloader_dfu_sd_in_progress(void)
{
    const bootloader_settings_t * p_bootloader_settings;

    bootloader_util_settings_get(&p_bootloader_settings);

    if (p_bootloader_settings->bank_0 == BANK_VALID_SD ||
        p_bootloader_settings->bank_1 == BANK_VALID_BOOT)
    {
        return true;
    }

    return false;
}


uint32_t bootloader_dfu_sd_update_continue(void)
{
    uint32_t err_code;

    if ((dfu_sd_image_validate() == NRF_SUCCESS) &&
        (dfu_bl_image_validate() == NRF_SUCCESS))
    {
        return NRF_SUCCESS;
    }

    err_code = dfu_sd_image_swap();
    APP_ERROR_CHECK(err_code);

    err_code = dfu_sd_image_validate();
    APP_ERROR_CHECK(err_code);

    err_code = dfu_bl_image_swap();
    APP_ERROR_CHECK(err_code);

    return err_code;
}


uint32_t bootloader_dfu_sd_update_finalize(void)
{
    dfu_update_status_t update_status = {DFU_UPDATE_SD_SWAPPED, };

    bootloader_dfu_update_process(update_status);

    wait_for_events();

    return NRF_SUCCESS;
}


void bootloader_settings_get(bootloader_settings_t * const p_settings)
{
    const bootloader_settings_t *flash_settings;

    bootloader_util_settings_get(&flash_settings);
    uint32_t i;
    for (i = 0; i < sizeof(bootloader_settings_t); i++)
        ((uint8_t *)p_settings)[i] =
            ((const volatile uint8_t *)flash_settings)[i];
}

