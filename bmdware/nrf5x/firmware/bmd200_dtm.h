#ifndef __BMD200_DTM_H
#define __BMD200_DTM_H

#include <stdint.h>
#include <stdbool.h>
#include <nrf51_bitfields.h>

#include "bmdware_gpio_def.h"

void bmd_dtm_init(void);
void bmd_dtm_deinit(void);
void bmd_dtm_proc_rx(uint8_t byte, bool init);

#endif
