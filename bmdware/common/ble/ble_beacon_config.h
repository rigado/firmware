/** @file ble_beacon_config.h
*
* @brief This module manages the beacon configuration service.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef _BLE_BEACON_CONFIG_H_
#define _BLE_BEACON_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"

/* Control Point Command responses */
#define COMMAND_SUCCESS                 0x00
#define DEVICE_LOCKED                   0x01
#define DEVICE_COMMAND_INVALID_LEN      0x02
#define DEVICE_UNLOCK_FAILED            0x03
#define DEVICE_UPDATE_PIN_FAILED        0x04
#define DEVICE_COMMAND_INVALID_DATA     0x05
#define DEVICE_COMMAND_INVALID_STATE    0x06
#define DEVICE_COMMAND_INVALID_PARAM    0x07
#define DEVICE_COMMAND_INVALID_COMMAND  0x08

#define DEVICE_UART_TX_BUFFER_ALMOST_FULL   0x09
#define DEVICE_UART_TX_BUFFER_FULL          0x0a
#define DEVICE_UART_TX_BUFFER_AVAILABLE     0x0b

/**@brief Beacon Configuration Service event type. */
typedef enum
{
    BLE_BEACON_CONFIG_EVT_CONTROL_NOTIFICATION_ENABLED,                            /**< Beacon Configuration control notification disabled event. */
    BLE_BEACON_CONFIG_EVT_CONTROL_NOTIFICATION_DISABLED                            /**< Beacon Configuration control notification disabled event. */
} ble_beacon_config_evt_type_t;

/**@brief Beacon Configuration Service event. */
typedef struct
{
    ble_beacon_config_evt_type_t evt_type;                                  /**< Type of event. */
} ble_beacon_config_evt_t;

// Forward declaration of the ble_bas_t type. 
typedef struct ble_beacon_config_s ble_beacon_config_t;

/**@brief Beacon Configuration event handler type. */
typedef void (*ble_beacon_config_evt_handler_t) (ble_beacon_config_t * p_adc, ble_beacon_config_evt_t * p_evt);

/**@brief Beacon Configuration Service init structure. This contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_beacon_config_evt_handler_t         evt_handler;        /**< Event handler to be called for handling events in the band Service. */
    bool                          support_notification;         /**< TRUE if notification of Beacon Configuration Status is supported. */
    ble_srv_report_ref_t *        p_report_ref;                 /**< If not NULL, a Report Reference descriptor with the specified value will be added to the Beacon Configuration Status characteristic */
    
    ble_srv_cccd_security_mode_t  beacon_config_control_char_attr_md;     
                                                                /**< Initial security level for beacon config control characteristic attribute */
} ble_beacon_config_init_t;

//Major
//Minor
//UUID
//Interval
//Tx Power
//Enable

/**@brief Beacon Configuration Service structure. This contains various status information for the service. */
typedef struct ble_beacon_config_s
{
    ble_beacon_config_evt_handler_t         
                                    evt_handler;                    /**< Event handler to be called for handling events in the Beacon Configuration Service. */
    uint16_t                        service_handle;                 /**< Handle of Beacon Configuration Service (as provided by the BLE stack). */
    uint8_t                         uuid_type;                      /**< UUID type assigned for DFU Service by the BLE stack. */
    ble_gatts_char_handles_t        beacon_config_control_handles;  /**< Handles related to the Beacon Configuration Control characteristic. */
    ble_gatts_char_handles_t        beacon_config_uuid_handles;     /**< Handles related to the Beacon Configuration UUID characteristic. */
    ble_gatts_char_handles_t        beacon_config_major_handles;    /**< Handles related to the Beacon Configuration Major number characteristic. */
    ble_gatts_char_handles_t        beacon_config_minor_handles;    /**< Handles related to the Beacon Configuration Minor number characteristic. */
    ble_gatts_char_handles_t        beacon_config_interval_handles; /**< Handles related to the Beacon Configuration Interval characteristic. */
    ble_gatts_char_handles_t        beacon_config_tx_power_handles; /**< Handles related to the Beacon Configuration TX Power characteristic. */
    ble_gatts_char_handles_t        beacon_config_enable_handles;   /**< Handles related to the Beacon Configuration Enable characteristic. */
		ble_gatts_char_handles_t        beacon_config_connectable_tx_power_handles; 
																																		/**< Handles related to the Beacon Configuration TX Power characteristic. */
    
    uint16_t                        report_ref_handle;              /**< Handle of the Report Reference descriptor. */

    ble_uuid128_t                   beacon_uuid;                    /**< Value for Beacon UUID */
    uint16_t                        major;                          /**< Beacon Major number */
    uint16_t                        minor;                          /**< Beacon Minor number */
    uint16_t                        interval;                       /**< Beacon advertising interval */
    uint8_t                         beacon_tx_power;                /**< Beacon TX Power level (dbm) */
    uint8_t                         enable;                         /**< If 1, beacon is enabled, otherwise, disabled */
		uint8_t                         connectable_tx_power;            /**< Non-Beacon TX Power level (dbm) */
    uint16_t                        conn_handle;                    /**< Handle of the current connection (as provided by the BLE stack, is BLE_CONN_HANDLE_INVALID if not in a connection). */
    bool                            is_notification_supported;      /**< TRUE if notification of Beacon Configuration Status is supported. */
} ble_beacon_config_t;

extern const ble_uuid128_t ble_beacon_config_service_uuid128;                                         /**< Service UUID. */
//extern const ble_uuid128_t ble_beacon_config_ctrl_point_uuid128;                                      /**< Control point UUID. */

/**@brief Function for initializing the Beacon Configuration Service.
 *
 * @param[out]  p_bas       Beacon Configuration Service structure. This structure will have to be supplied by
 *                          the application. It will be initialized by this function, and will later
 *                          be used to identify this particular service instance.
 * @param[in]   p_bas_init  Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
uint32_t ble_beacon_config_init(ble_beacon_config_t * p_beacon_config, const ble_beacon_config_init_t * p_beacon_config_init);

/**@brief Function for acquiring the uuid type of the Beacon Configuration Service as provided by the stack.
 *
 * @return      UUID Type of Beacon Configuration Service as assigned by the stack.
 */
uint8_t ble_beacon_config_get_uuid_type( void );

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the Beacon Configuration Service.
 *
 * @param[in]   p_adc     Beacon Configuration Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
void ble_beacon_config_on_ble_evt(ble_beacon_config_t * p_adc, ble_evt_t * p_ble_evt);

uint32_t ble_beacon_config_send_notification(const ble_beacon_config_t * p_beacon_config, uint16_t value_handle, uint8_t * data, uint16_t length );

uint8_t ble_beacon_set_major(uint16_t val);
uint8_t ble_beacon_set_minor(uint16_t val);
uint8_t ble_beacon_set_uuid(uint8_t *data, uint16_t n);
uint8_t ble_beacon_set_beacon_tx_power(uint8_t val);
uint8_t ble_beacon_set_ad_interval(uint16_t val);
uint8_t ble_beacon_set_enable(uint8_t val);
uint8_t ble_beacon_set_connectable_tx_power(uint8_t val);

#endif

/** @} */
