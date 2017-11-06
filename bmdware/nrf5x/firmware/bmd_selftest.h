#ifndef _BMD_SELFTEST_
#define _BMD_SELFTEST_

#include <stdint.h>

#define BMD_SELFTEST_CMD     0xAA55
#define BMD_GETMAC_CMD       0xAA56
#define BMD_GETVERSION_CMD   0xAA57
#define BMD_GETDEVICEID_CMD  0xAA58
#define BMD_GETHWID_CMD      0xAA59
#define BMD_GETPARTINFO_CMD  0xAA5A
#define BMD_ENABLE_RESET_CMD 0xAA5B
#define BMD_RESET_CMD        0xAA5C

#define BMD_SELFTEST_SUCCESS 0

uint32_t bmd_selftest(void);
void bmd_selftest_init_gpio(void);

#endif /* _BMD200_SELFTEST_ */
