#ifndef RIGADO_H
#define RIGADO_H

#include <nrf.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "ble_gap.h"

/* Data page layout */
typedef struct
{
    uint32_t must_be_zero;   /* MUST be zero, otherwise the bootloader will
                                reboot when decrypting image data.  This is to
                                protect against an attacker erasing the key
                                by erasing the data page. */
#ifdef NRF52    
    uint8_t reserved1[4060];  /* reserved */
#elif defined(NRF51)
    uint8_t reserved[988];   /* reserved */
#endif
    uint8_t dfu_key[16];     /* DFU encryption key */
    uint8_t radio_mac[6];    /* assigned MAC address, LSB first */
    uint8_t reserved2[10];   /* reserved */
} rigado_data_t;

/* Rigado data page */
#ifdef NRF52
    #define RIGADO_DATA_ADDRESS 0x7F000
#elif defined(NRF51)
    #define RIGADO_DATA_ADDRESS 0x3FC00
#endif

#define RIGADO_DATA ((const rigado_data_t *)RIGADO_DATA_ADDRESS)
#define RIGADO_MAC_UICR_ADDR ((uint8_t*)(NRF_UICR_BASE)+0x80)

/* The rigado data page must be one page in size; this checks it */
/* EPS - Figure out how to fix this!! */
//typedef long static_assert_failed[(sizeof(rigado_data_t) == 0x1000) ? 1 : -1];

/* BLE identification */
#define RIGDFU_ID_MFG_MODEL_ID      "Rigado Secure DFU"
#define RIGDFU_ID_DEVICE_NAME       "RigDfu"
#define RIGDFU_ID_MANUFACTURER_NAME "Rigado"


/* Get the stored MAC address, little-endian (LSB first).  Returns
   true if it came from the UICR, or false if it came from the FICR.
*/
bool rigado_get_mac_uicr(uint8_t * buf);

/* Return a pointer to a static formatted string version of the stored MAC */
const char *rigado_get_mac_string(void);

/* invert bits to prevent caching on some devices */
void rigado_invert_mac_bits(ble_gap_addr_t * mac_addr);

/* Store a pointer to the encryption key in *key.
   Returns true if the key is valid, false if it's empty
   (all 0 or all 1) */
bool rigado_get_key(const uint8_t **key);

/* Fills a null-terminated string with hardware information up to len bytes;
   returns the number of bytes used */
uint8_t rigado_get_hardware_info(uint8_t * hw_info, uint8_t len);

#endif
