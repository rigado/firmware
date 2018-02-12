#ifndef _PATCHER_H_
#define _PATCHER_H_

#include <stdint.h>
#include <stddef.h>
#include "bspatch.h"

#define PATCHER_COMPLETE    4
#define PATCHER_FLASHING    3
#define PATCHER_NEED_MORE   2
#define PATCHER_INPUT_FULL  1
#define PATCHER_SUCCESS     0
#define PATCHER_FAIL        (-1)

typedef struct patch_init_struct {
    int32_t old_size;
    uint8_t * old_ptr;
    int32_t new_size;
    uint8_t * new_buf_ptr;
    uint32_t new_buf_size;
    store_data_fptr store_func;
} patch_init_t;

int32_t patcher_init(patch_init_t * init_data);
int32_t patcher_add_data(uint8_t * data, uint32_t length);
uint32_t patcher_get_bytes_received(void);
int32_t patcher_patch(void);
#endif
