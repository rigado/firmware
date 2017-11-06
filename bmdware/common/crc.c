/** @file crc.c
*
* @brief This module provides a crc8 implementation
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */
#include "crc.h"

//CRC-8 - based on the CRC8 formulas by Dallas/Maxim
//code released under the terms of the GNU GPL 3.0 license
uint8_t crc8(const uint8_t *data, uint16_t len) 
{
  uint8_t crc = 0x00;
	
  while (len--) {
    uint8_t extract = *data++;
    for (uint8_t tempI = 8; tempI; tempI--) {
      uint8_t sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }

  return crc;
}

