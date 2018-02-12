#ifndef LTC_NRF_H
#define LTC_NRF_H

#include <tomcrypt.h>

void ltc_nrf_ecb_setup(const unsigned char *key);
int ltc_nrf_ecb_encrypt(const unsigned char *pt,
                        unsigned char *ct);

#endif
