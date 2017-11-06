/** @file gap.h
*
* @brief GAP setup commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __GAP_INIT_H__
#define __GAP_INIT_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_DEVICE_NAME_LEN     (8)
#define DEFAULT_DEVICE_NAME     "RigCom"

void gap_params_init(void);
uint32_t gap_update_device_name(void);
bool gap_validate_name(const char * p_name);

#endif
