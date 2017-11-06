/** @file advertising.c
*
* @brief This module initializes all advertising data for connectable and beacon 
*        advertisements
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __ADVERTISING_H__
#define __ADVERTISING_H__

#include <stdint.h>

#define ADV_MODE_BEACON                 0
#define ADV_MODE_CONNECTABLE            1

void advertising_init(void);
void advertising_restart(void);

/* Initialize advertising for beacon mode */
void advertising_init_beacon(void);

/* Initialize advertising for non-beacon mode (configuration mode) */
void advertising_init_non_beacon(void);

void advertising_start(void);
void advertising_stop_beacon(void);
void advertising_stop_connectable_adv(void);

#endif
