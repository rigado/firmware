#include <nrf.h>
#include <nrf_soc.h>
#include "nordic_common.h"
#include <tomcrypt.h>
#include "ltc_nrf.h"

#include "rigdfu_serial.h"

/* Bridge between LibTomCrypt and nRF51 SDK to use hardware AES cipher */

static nrf_ecb_hal_data_t ecb_data;

void ltc_nrf_ecb_setup(const unsigned char *key)
{
    memcpy(ecb_data.key, key, 16);
}

int ltc_nrf_ecb_encrypt(const unsigned char *pt,
                        unsigned char *ct)
{
    memcpy(ecb_data.cleartext, pt, 16);
    if (sd_ecb_block_encrypt(&ecb_data) != NRF_SUCCESS)
        return CRYPT_ERROR;
    memcpy(ct, ecb_data.ciphertext, 16);
    return CRYPT_OK;
}
