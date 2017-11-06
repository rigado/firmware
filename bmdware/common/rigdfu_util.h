/** @file rigdfu_util.c
*
* @brief This module provides utility functions for validating MAC addresses.
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef RIGDFU_UTIL_H
#define RIGDFU_UTIL_H

#include <stdint.h>
#include <stdbool.h>

/* Return x represented as hex character 0-f */
uint8_t hex_digit(uint8_t x);

/* Safe version of bcmp that doesn't leak (as much) timing information. */
int timingsafe_bcmp(const void *a, const void *b, int len);

/* Return true if all entries in buf are equal to val */
bool all_equal(const void *buf, uint8_t val, int len);

#endif
