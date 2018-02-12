#include <nrf.h>
#include "string.h"
#include "rigdfu.h"
#include "rigdfu_util.h"
#include "nrf_ic_info.h"


#define HW_NRF52        "nRF52/"
#define HW_NRF51        "nRF51/"
#define HW_QFN          "QFN/"
#define HW_CSP          "CSP/"
#define HW_RAM_16       "16"
#define HW_RAM_32       "32"
#define HW_RAM_64       "64"
#define HW_FLASH_128    "128/"
#define HW_FLASH_256    "256/"
#define HW_FLASH_512    "512/"

#define GET_RAM(x)      ((x == 16) ? HW_RAM_16 : (x == 32) ? HW_RAM_32 : (x == 64) ? HW_RAM_64 : "??")
#define GET_FLASH(x)    ((x == 128) ? HW_FLASH_128 : (x == 256) ? HW_FLASH_256 : (x == 512) ? HW_FLASH_512 : "???")


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

void rigado_invert_mac_bits(ble_gap_addr_t * mac_addr)
{
    for(uint8_t i = 0; i < BLE_GAP_ADDR_LEN; i++)
    {
        mac_addr->addr[i] = ~(mac_addr->addr[i]);
    }
    /* For random public static address, high two bits must be set */
    mac_addr->addr[5] |= 0xc0;
}

/* Return a pointer to a static formatted string version of the stored MAC */
const char *rigado_get_mac_string(void)
{
    static char macstr[6 * 3];
    ble_gap_addr_t mac_addr;
    int i;
    rigado_get_mac_uicr(mac_addr.addr);

    rigado_invert_mac_bits(&mac_addr);

    for (i = 0; i < 6; i++) {
        macstr[3 * i + 0] = hex_digit(mac_addr.addr[5 - i] >> 4);
        macstr[3 * i + 1] = hex_digit(mac_addr.addr[5 - i] & 0xf);
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

uint8_t rigado_get_hardware_info(uint8_t * hw_info, uint8_t len)
{
    uint8_t offset = 0;
    uint8_t temp = 0;
    
    memset(hw_info, 0x00, len);
    
#ifdef NRF51
    nrf_ic_info_t info;
    nrf_ic_info_get(&info);
    offset += strlen(HW_NRF51);
    memcpy(hw_info, HW_NRF51, offset);
#elif defined(NRF52)
    offset += strlen(HW_NRF52);
    memcpy(hw_info, HW_NRF52, offset);
#endif
    
#ifdef NRF51
    char * flash = GET_FLASH(info.flash_size);
    char * ram = GET_RAM(info.ram_size);
#elif defined(NRF52)
    char * flash = GET_FLASH(NRF_FICR->INFO.FLASH);
    char * ram = GET_RAM(NRF_FICR->INFO.RAM);
#else
    #error "Unknown chip type!"
#endif

    temp = strlen(flash);
    memcpy(&hw_info[offset], flash, temp);
    offset += temp;
    
    temp = strlen(ram);
    memcpy(&hw_info[offset], ram, temp);
    offset += temp;
    
    return offset;
}
