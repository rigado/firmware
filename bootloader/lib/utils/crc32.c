#include "crc32.h"

uint32_t crc32(uint8_t *message, uint32_t len) {
   uint32_t i;
   int32_t j;
   
   uint32_t byte;
   uint32_t crc;
   uint32_t mask;

   i = 0;
   crc = 0xFFFFFFFF;
   for(i = 0; i < len; i++) {
      byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
   }
   
   return ~crc;
}
