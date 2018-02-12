#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>

/** @brief Calculate a CRC32 using a reverse polynomial 0xEDB88320.
 *
 *  @param[in] data The data to CRC
 *  @param[in] len The length of the data
 *
 *  @return CRC32 of data with length len
 */
uint32_t crc32(uint8_t * data, uint32_t len);

#endif /* _CRC32_H_ */
