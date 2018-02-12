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
 * @defgroup nrf_bootloader_types Types and definitions.
 * @{     
 *  
 * @ingroup nrf_bootloader
 * 
 * @brief Bootloader module type and definitions.
 */
 
#ifndef BOOTLOADER_TYPES_H__
#define BOOTLOADER_TYPES_H__

#include <stdint.h>

/* Magic values that can be written to GPREGRET before resetting: */
/* -- Reset into bootloader from application with DFU stubs. */
#define BOOTLOADER_DFU_START            0xB1
#define BOOTLOADER_DFU_START_W_UART     0xB2
/* -- Jump immediately to the application when the bootloader starts,
      if the application is valid.  Any value in the range
      BOOTLOADER_APP_START_MIN to BOOTLOADER_APP_START_MAX
      will trigger this. */
#define BOOTLOADER_APP_START_MIN 0xC0
#define BOOTLOADER_APP_START_MAX 0xCF
/* Before loading the app, GPREGRET will be ANDed with
   BOOTLOADER_APP_START_MASK so that subsequent resets don't skip
   the bootloader. */
#define BOOTLOADER_APP_START_MASK 0x0F

/**@brief DFU Bank state code, which indicates wether the bank contains: A valid image, invalid image, or an erased flash.
  */
typedef enum
{
    BANK_UNKNOWN_00  = 0x00,
    BANK_VALID_APP   = 0x01,
    BANK_VALID_SD    = 0xA5,
    BANK_VALID_BOOT  = 0xAA,
    BANK_ERASED      = 0xFD,
    BANK_INVALID_APP = 0xFE,
    BANK_UNKNOWN_FF  = 0xFF,
} bootloader_bank_code_t;

/**@brief Structure holding bootloader settings for application and bank data.
 */
typedef struct
{
    bootloader_bank_code_t bank_0;          /**< Variable to store if bank 0 contains a valid application. */
    bootloader_bank_code_t bank_1;          /**< Variable to store if bank 1 has been erased/prepared for new image. Bank 1 is only used in Banked Update scenario. */
    uint32_t               bank_0_size;     /**< Size of active image in bank0 if present, otherwise 0. */
    uint32_t               sd_image_size;   /**< Size of SoftDevice image in bank0 if bank_0 code is \ref BANK_VALID_SD. */
    uint32_t               bl_image_size;   /**< Size of Bootloader image in bank0 if bank_0 code is \ref BANK_VALID_SD. */
    uint32_t               app_image_size;  /**< Size of Application image in bank0 if bank_0 code is \ref BANK_VALID_SD. */
    uint32_t               sd_image_start;  /**< Location in flash where SoftDevice image is stored for SoftDevice update. */
} bootloader_settings_t;

#endif // BOOTLOADER_TYPES_H__ 

/**@} */
