#ifndef DFU_TRANSPORT_BLE
#define DFU_TRANSPORT_BLE

#include <stdint.h>

uint32_t dfu_transport_update_start_ble(void);
uint32_t dfu_transport_close_ble(void);

uint8_t get_client_rx_mtu(void);

#endif
