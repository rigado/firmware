/** @file rig_firmware_info.c
*
* @brief This module creates and stores, in the ROM image, information
*        about the firmware image.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
* All rights reserved. 
*/

#include "version.h"

#include "rig_firmware_info.h"

#if defined( __CC_ARM )
const rig_firmware_info_t bl_info __attribute((at(APP_START_ADDRESS + 0x1000))) =
#elif defined(__GNUC__)
const rig_firmware_info_t bl_info __attribute((section(".version_info"))) =
#endif
{
    0x97C69FAE,
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
    #elif defined(S132)
    SOFTDEVICE_SUPPORT_S132,
    #else
    SOFTDEVICE_SUPPORT_S110,
    #endif
    #ifdef NRF51
    HARDWARE_SUPPORT_NRF51,
    #else
    HARDWARE_SUPPORT_NRF52,
    #endif
    API_PROTOCOL_VERSION,
    0x11DE2759
};
