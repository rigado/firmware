/** @file bootloader_info.c
*
* @brief This module provides functions to retrieve the installed bootloader
*        version information.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
* All rights reserved. 
*/

#ifndef _BOOTLOADER_INFO_H_
#define _BOOTLOADER_INFO_H_

#include <stdint.h>
#include "rig_firmware_info.h"

#ifdef __cplusplus 
extern “C” { 
#endif

uint32_t bootloader_info_read(rig_firmware_info_t * p_info);

#ifdef __cplusplus 
}
#endif

#endif
