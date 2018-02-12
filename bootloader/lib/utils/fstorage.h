/* Simpler version of flash storage management. */

#ifndef FSTORAGE_H
#define FSTORAGE_H

#include <stdint.h>

#define FSTORAGE_STORE_OP_CODE  0x02
#define FSTORAGE_CLEAR_OP_CODE  0x04

#define FSTORAGE_QUEUE_LEN 16

/* Identifiers */
typedef enum {
    FSTORAGE_DFU,
    FSTORAGE_BOOTLOADER,
    FSTORAGE_max
} fstorage_id_t;

/* Callback type */
typedef void (*fstorage_cb_t)(uint32_t address, uint8_t op_code,
                              uint32_t result, void *p_data);

/* Initialize fstorage */
void fstorage_init(void);

/* Register callback handler. */
uint32_t fstorage_register(fstorage_id_t id, fstorage_cb_t cb);

/* Write data to flash and call the callback when it's finished. */
uint32_t fstorage_store(fstorage_id_t id, uint32_t dest, void *src, int len);

/* Clear flash and call the callback when it's finished.  "len" is rounded up
   to the nearest page size. */
uint32_t fstorage_clear(fstorage_id_t id, uint32_t dest, int len);

/* Handle system events */
void fstorage_sys_event_handler(uint32_t sys_evt);

#endif
