#include <tomcrypt.h>
#include "ltc_nrf.h"

#include "rigdfu_serial.h"

#define printf rigdfu_serial_printf

void test_aes128eax(void)
{
    int err;
    int i;
    static eax_state eax;
//    static unsigned char pt[] = {
//        0x8B, 0x0A, 0x79, 0x30, 0x6C, 0x9C, 0xE7, 0xED,
//        0x99, 0xDA, 0xE4, 0xF8, 0x7F, 0x8D, 0xD6, 0x16,
//        0x36
//    };
    static unsigned char ct[] = {
        0x02, 0x08, 0x3e, 0x39, 0x79, 0xda, 0x01, 0x48,
        0x12, 0xf5, 0x9f, 0x11, 0xd5, 0x26, 0x30, 0xda,
        0x30
    };
    static unsigned char pt[sizeof(ct)];
    static unsigned char header[] = {
        0x65, 0xD2, 0x01, 0x79, 0x90, 0xD6, 0x25, 0x28
    };
    static unsigned char nonce[16] = {
        0x1A, 0x8C, 0x98, 0xDC, 0xD7, 0x3D, 0x38, 0x39,
        0x3B, 0x2B, 0xF1, 0x56, 0x9D, 0xEE, 0xFC, 0x19
    };
    static unsigned char key[16] = {
        0x7C, 0x77, 0xD6, 0xE8, 0x13, 0xBE, 0xD5, 0xAC,
        0x98, 0xBA, 0xA4, 0x17, 0x47, 0x7A, 0x2E, 0x7D
    };

    static unsigned char tag[16];
    static unsigned long taglen;

    /* initialize context */
    if ((err = eax_init(&eax, /* context */
                        0, /* cipher id */
                        key,
                        sizeof(key),
                        nonce, /* the nonce */
                        sizeof(nonce), /* nonce is 16 bytes */
                        header,
                        sizeof(header))) != CRYPT_OK) {
        printf("Error eax_init: %d\n", err);
        return;
    }

    /* now decrypt data, say in a loop or whatever */
    if ((err = eax_decrypt(&eax, /* eax context */
                           ct, /* ciphertext (source) */
                           pt, /* plaintext (destination) */
                           sizeof(ct) /* size of plaintext */
             )) != CRYPT_OK) {
        printf("Error eax_encrypt: %d\n", err);
        return;
    }

    /* finish message and get authentication tag */
    taglen = sizeof(tag);
    if ((err = eax_done(&eax,      /* eax context */
                        tag,      /* where to put tag */
                        &taglen       /* length of tag space */
             )) != CRYPT_OK) {
        printf("Error eax_done: %d\n", err);
        return;
    }

    printf("got a tag of len %ld\n", taglen);
    printf("\nMSG:     ");
    for (i = 0; i < sizeof(pt); i++)
        printf("%02x", pt[i]);
    printf("\nKEY:     ");
    for (i = 0; i < sizeof(key); i++)
        printf("%02x", key[i]);
    printf("\nNONCE:   ");
    for (i = 0; i < sizeof(nonce); i++)
        printf("%02x", nonce[i]);
    printf("\nHEADER:  ");
    for (i = 0; i < sizeof(header); i++)
        printf("%02x", header[i]);
    printf("\nCIPHER:  ");
    for (i = 0; i < sizeof(ct); i++)
        printf("%02x", ct[i]);
    printf("\nTAG:     ");
    for (i = 0; i < taglen; i++)
        printf("%02x", tag[i]);
    printf("\n");

    /* should be:
MSG:     8b0a79306c9ce7ed99dae4f87f8dd61636
KEY:     7c77d6e813bed5ac98baa417477a2e7d
NONCE:   1a8c98dcd73d38393b2bf1569deefc19
HEADER:  65d2017990d62528
CIPHER:  02083e3979da014812f59f11d52630da30
TAG:     137327d10649b0aa6e1c181db617d7f2
    */

}
