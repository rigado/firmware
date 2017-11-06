/** @file bootloader_info.c
*
* @brief This module provides functions to retrieve the installed bootloader
*        version information.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
* All rights reserved. 
*/

#ifdef NRF52
#define BOOTLOADER_START_ADDR       0x75000
#elif defined(NRF51)
#define BOOTLOADER_START_ADDR       0x3A800
#endif
#define BOOTLOADER_INFO_OFFSET      0x1000
#define BOOTLOADER_INFO_ADDR        (BOOTLOADER_START_ADDR + BOOTLOADER_INFO_OFFSET)
#define MAGIC_HEADER                0x465325D4
#define MAGIC_FOOTER                0x49B0784C

#include <stdint.h>
#include <string.h>

#include "nrf_error.h"

#include "rig_firmware_info.h"

#include "bootloader_info.h"

uint32_t bootloader_info_read(rig_firmware_info_t * p_info)
{
    const uint8_t * bl_info = (uint8_t*)BOOTLOADER_INFO_ADDR;
    uint32_t err = NRF_SUCCESS;
    
    if(!p_info)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    memcpy(p_info, bl_info, sizeof(rig_firmware_info_t));
    
    if(MAGIC_HEADER != p_info->magic_number_a || MAGIC_FOOTER != p_info->magic_number_b)
    {
        err = NRF_ERROR_INVALID_DATA;
    }
    
    return err;
}
