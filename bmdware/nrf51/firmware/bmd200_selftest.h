#ifndef __BMD200_SELFTEST
#define __BMD200_SELFTEST

#include <stdint.h>

#define BMD200_SELFTEST_CMD     0xAA55
#define BMD200_GETMAC_CMD       0xAA56
#define BMD200_GETVERSION_CMD   0xAA57
#define BMD200_GETDEVICEID_CMD  0xAA58
#define BMD200_GETHWID_CMD      0xAA59
#define BMD200_GETPARTINFO_CMD  0xAA5A
#define BMD200_SELFTEST_SUCCESS 0

uint32_t bmd200_selftest(void);
void bmd200_selftest_init_gpio(void);

#endif /* __BMD200_SELFTEST */
