/** @file rig_firmware_info.c
*
* @brief This module creates and stores, in the ROM image, information
*        about the firmware image.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
* All rights reserved. 
*/

#ifndef _RIG_FIRMWARE_INFO_H_
#define _RIG_FIRMWARE_INFO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    
typedef enum version_type_e
{
    VERSION_TYPE_RELEASE = 1,
    VERSION_TYPE_DEBUG
} version_type_t;

typedef enum softdevice_support_e
{
    SOFTDEVICE_SUPPORT_S110 = 1,
    SOFTDEVICE_SUPPORT_S120,
    SOFTDEVICE_SUPPORT_S130,
    SOFTDEVICE_SUPPORT_S132,
    SOFTDEVICE_SUPPORT_RESERVED2,
    SOFTDEVICE_SUPPORT_RESERVED3,
    SOFTDEVICE_SUPPORT_RESERVED4,
    SOFTDEVICE_SUPPORT_RESERVED5
} softdevice_support_t;

typedef enum hardware_support_e
{
    HARDWARE_SUPPORT_NRF51 = 1,
    HARDWARE_SUPPORT_NRF52,
    HARDWARE_SUPPORT_RESERVED1,
    HARDWARE_SUPPORT_RESERVED2,
    HARDWARE_SUPPORT_RESERVED3,
    HARDWARE_SUPPORT_RESERVED4,
    HARDWARE_SUPPORT_RESERVED5
} hardware_support_t;

#pragma pack(1)
typedef struct rig_firmware_info_s
{
    uint32_t magic_number_a;        //4
    uint32_t size;                  //4
    uint8_t version_major;          //1
    uint8_t version_minor;          //1
    uint8_t version_rev;            //1
    uint32_t build_number;          //4
    version_type_t version_type;    //1
    softdevice_support_t sd_support;//1
    hardware_support_t hw_support;  //1
    uint16_t protocol_version;      //2
    uint32_t magic_number_b;        //4
                                    //24
} rig_firmware_info_t;
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif
