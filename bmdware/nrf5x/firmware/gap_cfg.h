/** @file gap_cfg.h
*
* @brief GAP configuration data
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __GAP_CFG_H__
#define __GAP_CFG_H__

#define DEVICE_NAME                     "RigCom"

#define MIN_CONN_INTERVAL               6                                  /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               16                                  /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                   /**< slave latency. */
#define CONN_SUP_TIMEOUT                400                                 /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */

#define APP_TIMER_PRESCALER             0

#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)               /**< Time from the Connected event to first time sd_ble_gap_conn_param_update is called (100 milliseconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(500, APP_TIMER_PRESCALER)               /**< Time between each call to sd_ble_gap_conn_param_update after the first call (500 milliseconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT         3                                                      /**< Number of attempts before giving up the connection parameter negotiation. */

#endif
