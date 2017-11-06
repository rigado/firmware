/** @file rigdfu.c
*
* @brief This module provides functions for accesing the mac address.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <nrf.h>
#include "string.h"
#include "rigdfu.h"
#include "rigdfu_util.h"

/* Get the FICR data */
void rigado_get_ficr(uint8_t *buf, bool invert)
{
    const uint8_t *mac = (const uint8_t *)&(NRF_FICR->DEVICEADDR[0]);
    memcpy(buf, mac, 6);
    if(invert)
    {
        for(uint8_t i = 0; i < 6; i++)
        {
            buf[i] = ~buf[i];
        }
    }
    buf[5] |= 0xC0;
}
    

/* Get the stored MAC address, little-endian (LSB first).  Returns
   true if it came from the data page, or false if it came from the
   FICR. */
bool rigado_get_mac_le(uint8_t *buf)
{
    bool uicr = true;
    const uint8_t *mac = RIGADO_MAC_UICR_ADDR;

    if (all_equal(mac, 0x00, 6) || all_equal(mac, 0xff, 6))
    {
        mac = (const uint8_t *)&(NRF_FICR->DEVICEADDR[0]);
        uicr = false;
    }
    memcpy(buf, mac, 6);

    /* If we pulled a random address from the FICR, the top
       two bits must be 11 per BT spec */
    if (uicr == false)
        buf[5] |= 0xc0;

    return uicr;
}

bool rigado_get_mac_uicr(uint8_t * buf)
{
    bool uicr = true;
    const uint8_t * mac = RIGADO_MAC_UICR_ADDR;

    if(all_equal(mac, 0x00, 6) || all_equal(mac, 0xff, 6))
    {
        mac = (const uint8_t *)&(NRF_FICR->DEVICEADDR[0]);
        uicr = false;
    }
    memcpy(buf, mac, 6);

    /* If we pulled a random address from the FICR, the top
       two bits must be 11 per BT spec */
    if(uicr == false)
        buf[5] |= 0xc0;

    return uicr;
}

bool rigado_get_mac(uint8_t *buf)
{
    uint8_t local_buf[6];
    bool uicr_or_data = false;
    if(rigado_get_mac_uicr(local_buf))
    {
        uicr_or_data = true;
    }
    else if(rigado_get_mac_le(local_buf))
    {
        uicr_or_data = true;
    }
    
    memcpy(buf, local_buf, 6);
    return uicr_or_data;
}

/* Return a pointer to a static formatted string version of the stored MAC */
const char *rigado_get_mac_string(void)
{
    static char macstr[6 * 3];
    uint8_t mac[6];
    int i;
    rigado_get_mac_le(mac);
    for (i = 0; i < 6; i++) {
        macstr[3 * i + 0] = hex_digit(mac[5 - i] >> 4);
        macstr[3 * i + 1] = hex_digit(mac[5 - i] & 0xf);
        macstr[3 * i + 2] = (i == 5) ? 0 : ':';
    }
    return macstr;
}

/* Store a pointer to the encryption key in *key.
   Returns true if the key is valid, false if it's empty
   (all 0 or all 1) */
bool rigado_get_key(const uint8_t **key)
{
    *key = RIGADO_DATA->dfu_key;
    if (all_equal(*key, 0x00, 16) || all_equal(*key, 0xff, 16))
        return false;
    return true;
}
