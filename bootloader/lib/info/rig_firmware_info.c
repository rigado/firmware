/** @file rig_firmware_info.c
*
* @brief This module creates and stores, in the ROM image, information
*        about the firmware image.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
* All rights reserved. 
*/

#include "dfu_types.h"
#include "version.h"

#include "rig_firmware_info.h"

const rig_firmware_info_t bl_info __attribute((at(BOOTLOADER_REGION_START + 0x1000))) =
{
    0x465325D4,
    sizeof(rig_firmware_info_t),
    FIRMWARE_MAJOR_VERSION,
    FIRMWARE_MINOR_VERSION,
    FIRMWARE_BUILD_NUMBER,
    BUILD_VERSION_NUMBER,
    #ifdef RELEASE
    VERSION_TYPE_RELEASE,
    #else
    VERSION_TYPE_DEBUG,
    #endif
    #ifdef S120
    SOFTDEVICE_SUPPORT_S120,
    #elif defined(S130)
    SOFTDEVICE_SUPPORT_S130,
    #elif defined(S110)
    SOFTDEVICE_SUPPORT_S110,
    #elif defined(S132)
        #if defined(SDK12)
            SOFTDEVICE_SUPPORT_S132_3_0,
        #else
            SOFTDEVICE_SUPPORT_S132,
        #endif
    #elif defined(S332)
    SOFTDEVICE_SUPPORT_S332,
    #else
    #error "Unknown Softdevice Support!"
    #endif
    #ifdef NRF51
    HARDWARE_SUPPORT_NRF51,
    #else
    HARDWARE_SUPPORT_NRF52,
    #endif
    API_PROTOCOL_VERSION,
    0x49B0784C
};
