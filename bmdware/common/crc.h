/** @file crc.c
*
* @brief This module provides a crc8 implementation
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __CRC_H
#define __CRC_H

#include <stdint.h>

uint8_t crc8(const uint8_t *data, uint16_t len);

#endif
