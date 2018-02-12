#ifndef HCI_MEM_POOL_BLE_H__
#define HCI_MEM_POOL_BLE_H__

#include <stdint.h>
#include "nrf_error.h"

uint32_t BLE_hci_mem_pool_open(void);
uint32_t BLE_hci_mem_pool_close(void);
uint32_t BLE_hci_mem_pool_tx_alloc(void ** pp_buffer);
uint32_t BLE_hci_mem_pool_tx_free(void);
uint32_t BLE_hci_mem_pool_rx_produce(uint32_t length, void ** pp_buffer);
uint32_t BLE_hci_mem_pool_rx_data_size_set(uint32_t length);
uint32_t BLE_hci_mem_pool_rx_extract(uint8_t ** pp_buffer, uint32_t * p_length);
uint32_t BLE_hci_mem_pool_rx_consume(uint8_t * p_buffer);

#endif
