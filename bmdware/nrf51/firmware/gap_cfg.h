#ifndef __GAP_CFG_H__
#define __GAP_CFG_H__

#define DEVICE_NAME                     "RigCom"

#define MIN_CONN_INTERVAL               16                                  /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               60                                  /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                   /**< slave latency. */
#define CONN_SUP_TIMEOUT                400                                 /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */

#endif
