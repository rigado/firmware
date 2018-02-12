/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file
 * @defgroup flashwrite_example_main main.c
 * @{
 * @ingroup flashwrite_example
 *
 * @brief This file contains the source code for a sample application using the Flash Write Application.
 *a
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rigdfu2_nrf52_s132_rel_3.2.3.45.bin.h"

#include "nrf.h"

/**@brief DFU Bank state code, which indicates wether the bank contains: A valid image, invalid image, or an erased flash.
  */
typedef enum
{
    BANK_VALID_APP   = 0x01,
    BANK_VALID_SD    = 0xA5,
    BANK_VALID_BOOT  = 0xAA,
    BANK_ERASED      = 0xFE,
    BANK_INVALID_APP = 0xFF,
} bootloader_bank_code_t;

/**@brief Structure holding bootloader settings for application and bank data.
 */
typedef struct
{
    bootloader_bank_code_t bank_0;          /**< Variable to store if bank 0 contains a valid application. */
    bootloader_bank_code_t bank_1;          /**< Variable to store if bank 1 has been erased/prepared for new image. Bank 1 is only used in Banked Update scenario. */
    uint32_t               bank_0_size;     /**< Size of active image in bank0 if present, otherwise 0. */
    uint32_t               sd_image_size;   /**< Size of SoftDevice image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               bl_image_size;   /**< Size of Bootloader image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               app_image_size;  /**< Size of Application image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               sd_image_start;  /**< Location in flash where SoftDevice image is stored for SoftDevice update. */
} bootloader_settings_t;

typedef struct version_s
{
    uint8_t major;
    uint8_t minor;
    uint8_t rev;
} version_t;

static const version_t update_version_list[] = {
    { 3, 2, 0 },
    { 3, 2, 1 },
    { 3, 2, 2 }
};    
#define UPDATE_VERSION_COUNT    (3)

/** @brief Function for erasing a page in flash.
 *
 * @param page_address Address of the first word in the page to be erased.
 */
static void flash_page_erase(uint32_t * page_address)
{
    // Turn on flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Erase page:
    NRF_NVMC->ERASEPAGE = (uint32_t)page_address;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}


/** @brief Function for filling a page in flash with a value.
 *
 * @param[in] address Address of the first word in the page to be filled.
 * @param[in] value Value to be written to flash.
 */
static void flash_word_write(uint32_t * address, uint32_t value)
{
    // Turn on flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    *address = value;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}

#define UICR_MBR_ADDRESS            (0x10001018)
#define FLASH_MBR_ADDRESS           (0x7D000)
#define FLASH_BL_SETTINGS_ADDRESS   (0x7E000)
#define BANK_1_ADDRESS              (0x47000)
#define BL_VERSION_IN_FLASH         (0x76008)

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t * addr;

    //fix UICR
    if(*((uint32_t*)UICR_MBR_ADDRESS) != FLASH_MBR_ADDRESS)
    {
        uint32_t mbr_address = FLASH_MBR_ADDRESS;
        flash_word_write((uint32_t*)UICR_MBR_ADDRESS, mbr_address);
    }
    
    volatile uint8_t * bl_version = (uint8_t*)(BL_VERSION_IN_FLASH);
    if(bl_version[0] == 3 && bl_version[1] == 2 && bl_version[2] == 3)
    {
        NRF_POWER->GPREGRET = 0xB1; //todo: use define
        NVIC_SystemReset();
    }
    
    bool should_update = false;
    for(uint8_t i = 0; i < UPDATE_VERSION_COUNT; i++)
    {
        if(memcmp(&update_version_list[i], (uint8_t*)bl_version, sizeof(version_t)) == 0)
        {
            should_update = true;
            break;
        }
    }
    
    if(!should_update)
    {
        NRF_POWER->GPREGRET = 0xB1;
        NVIC_SystemReset();
    }
    
    //write version 3.2.3 to bank 1
    uint32_t num_words = (sizeof(rigdfu2_nrf52_s132_rel_3_2_3_45_bin) / sizeof(uint32_t));
    uint32_t * new_bl_image = (uint32_t*)(rigdfu2_nrf52_s132_rel_3_2_3_45_bin);
    uint32_t * bank_addr = (uint32_t*)(BANK_1_ADDRESS);
    
    uint32_t page_count = (sizeof(rigdfu2_nrf52_s132_rel_3_2_3_45_bin) / 0x1000);
    if((page_count & 0x3FF) > 0)
    {
        page_count++;
    }
    
    //ensure bank 1 is erased for the image
    uint32_t page_addr = BANK_1_ADDRESS;
    for(uint32_t i = 0; i < page_count; i++)
    {
        flash_page_erase((uint32_t*)page_addr);
        page_addr += 0x1000;
    }
    
    //write bootloader data
    for(uint32_t i = 0; i < num_words; i++, bank_addr++)
    {
        flash_word_write(bank_addr, new_bl_image[i]);
    }
    
    //copy in the current settings and clear the settings area
    bootloader_settings_t settings;
    memset(&settings, 0, sizeof(bootloader_settings_t));
    flash_page_erase((uint32_t*)FLASH_BL_SETTINGS_ADDRESS);
    
    //set bank 0 to an invalid app so that this app will no longer run
    settings.bank_0 = BANK_INVALID_APP;
    settings.bank_0_size = 0;
    
    //set bank 1 to have a valid boot loader update
    settings.bank_1 = BANK_VALID_BOOT;
    settings.bl_image_size = sizeof(rigdfu2_nrf52_s132_rel_3_2_3_45_bin);
    
    //write the update settings back to flash
    uint32_t * p_settings = (uint32_t*)&settings;
    num_words = sizeof(bootloader_settings_t) / 4;
    addr = (uint32_t*)(FLASH_BL_SETTINGS_ADDRESS);
    for(uint32_t i = 0; i < num_words; i++, addr++)
    {
        flash_word_write(addr, p_settings[i]);
    }

    //tell the current bootloader to run and reset which will then update
    //the 3.2.x bootloader to 3.2.3
    NVIC_SystemReset();
}


/** @} */
