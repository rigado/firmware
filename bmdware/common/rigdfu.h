/** @file rigdfu.h
*
* @brief This module provides functions for accesing the mac address.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef RIGDFU_H
#define RIGDFU_H

#include <nrf.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Data page layout */
typedef struct
{
    uint32_t must_be_zero;   /* MUST be zero, otherwise the bootloader will
                                reboot when decrypting image data.  This is to
                                protect against an attacker erasing the key
                                by erasing the data page. */
    uint8_t reserved1[988];  /* reserved */
    uint8_t dfu_key[16];     /* DFU encryption key */
    uint8_t radio_mac[6];    /* assigned MAC address, LSB first */
    uint8_t reserved2[10];   /* reserved */
} rigado_data_t;

/* Rigado data page */
#define RIGADO_DATA_ADDRESS 0x3fc00
#define RIGADO_DATA ((const rigado_data_t *)RIGADO_DATA_ADDRESS)
#define RIGADO_MAC_UICR_ADDR ((uint8_t*)(NRF_UICR_BASE)+0x80)

/* The rigado data page must be one page in size; this checks it */
typedef char static_assert_failed[(sizeof(rigado_data_t) == 0x400) ? 1 : -1];

/* BLE identification */
#define RIGDFU_ID_MFG_MODEL_ID      "Rigado Secure DFU"
#define RIGDFU_ID_DEVICE_NAME       "__RigDfu"
#define RIGDFU_ID_MANUFACTURER_NAME "Rigado, LLC"

/* Get the FICR data */
void rigado_get_ficr(uint8_t *buf, bool invert);
    
/* Get the stored MAC address, little-endian (LSB first).  Returns
   true if it came from the data page, or false if it came from the
   FICR. */
bool rigado_get_mac_le(uint8_t *buf);

bool rigado_get_mac_uicr(uint8_t *buf);

bool rigado_get_mac(uint8_t *buf);

/* Return a pointer to a static formatted string version of the stored MAC */
const char *rigado_get_mac_string(void);

/* Store a pointer to the encryption key in *key.
   Returns true if the key is valid, false if it's empty
   (all 0 or all 1) */
bool rigado_get_key(const uint8_t **key);

#endif
