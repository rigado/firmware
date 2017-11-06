/** @file advertising_cfg.h
*
* @brief Advertising configuration
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __ADVERTISING_CFG_H__
#define __ADVERTISING_CFG_H__

#define APP_CFG_CONN_ADV_TIMEOUT        0                                   /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define BEACON_ADV_TIMEOUT        		4                                   /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define CONNECTABLE_ADV_INTERVAL        MSEC_TO_UNITS(2000, UNIT_0_625_MS)   /**< The advertising interval for connectable advertisement (100 ms). This value can vary between 100ms to 10.24s). */

#endif
