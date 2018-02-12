#include "rigdfu_util.h"

uint8_t hex_digit(uint8_t x)
{
    if (x < 10)
        return x + '0';
    return x - 10 + 'a';
}

/* Safe version of bcmp that doesn't leak (as much) timing information. */
int32_t timingsafe_bcmp(const void *a, const void *b, int len)
{
    const volatile uint8_t *pa = a;
    const volatile uint8_t *pb = b;
    uint8_t x = 0;
    while (len--)
        x |= *pa++ ^ *pb++;
    return (x != 0);
}

/* Return true if all entries in buf are equal to val */
bool all_equal(const void *buf, uint8_t val, int len)
{
    const uint8_t *b = buf;
    while (len--)
        if (*b++ != val)
            return false;
    return true;
}
