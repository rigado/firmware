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
 * @defgroup nrf_bootloader Bootloader API.
 * @{     
 *
 * @brief Bootloader module interface.
 */

#ifndef BOOTLOADER_H__
#define BOOTLOADER_H__

#include <stdbool.h>
#include <stdint.h>
#include "bootloader_types.h"
#include "dfu_types.h"

/**@brief Function for initializing the Bootloader.
 * 
 * @retval     NRF_SUCCESS If bootloader was succesfully initialized. 
 */
uint32_t bootloader_init(void);

/**@brief Function for validating application region in flash.
 *
 * @retval     true          If Application region is valid.
 * @retval     false         If Application region is not valid.
 */
bool bootloader_app_is_valid(void);

/**@brief Function for starting the Device Firmware Update.
 *
 * @param[in]  initial_timeout_ticks Initial timeout for first DFU contact
 * @param[in]  connect_timeout_ticks Timeout between connect and first DFU command
 * @param[in]  dfu_timeout_ticks     Timeout for subsequent peer DFU commands
 *
 * @retval     true    Try loading the app now (new app received, or timeout)
 *             false   Reboot now (error, or reboot required for SD/BL update)
 *
 */
bool bootloader_dfu_start(uint32_t initial_timeout_ticks,
                          uint32_t connect_timeout_ticks,
                          uint32_t dfu_timeout_ticks);

/**@brief Launch the application now, by triggering a reset.
 */
void bootloader_app_start(void);

/**@brief Call after reset to actually jump to the application
 */
void bootloader_launch_app_after_reset(void);

/**@brief Function for retrieving the bootloader settings.
 *
 * @param[out] p_settings    A copy of the current bootloader settings is returned in the structure
 *                           provided.
 */
void bootloader_settings_get(bootloader_settings_t * const p_settings);

/**@brief Function for processing DFU status update.
 *
 * @param[in]  update_status DFU update status.
 */
void bootloader_dfu_update_process(dfu_update_status_t update_status);

/**@brief Function getting state of SoftDevice update in progress.
 *        After a successfull SoftDevice transfer the system restarts in orderto disable SoftDevice
 *        and complete the update.
 *
 * @retval     true          A SoftDevice update is in progress. This indicates that second stage 
 *                           of a SoftDevice update procedure can be initiated.
 * @retval     false         No SoftDevice update is in progress.
 */
bool bootloader_dfu_sd_in_progress(void);

/**@brief Function for continuing the Device Firmware Update of a SoftDevice.
 * 
 * @retval     NRF_SUCCESS If the final stage of SoftDevice update was successful. 
 */
uint32_t bootloader_dfu_sd_update_continue(void);

/**@brief Function for finalizing the Device Firmware Update of a SoftDevice.
 * 
 * @retval     NRF_SUCCESS If the final stage of SoftDevice update was successful. 
 */
uint32_t bootloader_dfu_sd_update_finalize(void);

/**@brief   Function for restarting the bootloader timeout.
 *
 * @details This function will stop and restart the bootloader timeout timer.  This function
 *          should be called whenever a DFU packet is received from the peer.
 */
uint32_t bootloader_timeout_reset_on_first_connect(void);
uint32_t bootloader_timeout_reset(void);

/**@brief   Disable the bootloader timeout.
 */
void bootloader_timeout_stop(void);

#endif // BOOTLOADER_H__

/**@} */
