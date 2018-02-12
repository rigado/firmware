/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/**@file
 *
 * @defgroup nrf_dfu_types Types and definitions.
 * @{
 *
 * @ingroup nrf_dfu
 *
 * @brief Device Firmware Update module type and definitions.
 */

#ifndef DFU_TYPES_H__
#define DFU_TYPES_H__

#include <stdint.h>
#include "nrf_soc.h"
#include "app_util.h"
#include "rigdfu.h"

#define NRF_UICR_BOOT_START_ADDRESS     (NRF_UICR_BASE + 0x14)                                          /**< Register where the bootloader start address is stored in the UICR register. */

#if defined(NRF52)                                                  
#define NRF_UICR_MBR_PARAMS_PAGE_ADDRESS    (NRF_UICR_BASE + 0x18)      /**< Register where the mbr params page is stored in the UICR register. (Only in use in nRF52 MBR).*/
#endif        

#define NRF_UICR_CLENR0                 0xFFFFFFFF  /* No code region 0 */

#define CODE_REGION_1_START             SD_SIZE_GET(MBR_SIZE)

#ifdef SDK10
    #define DFU_3_1_0_BANK_1_LOCATION       0x28400
    #define DFU_3_0_0_BANK_1_LOCATION       0x29400
#endif

#define SOFTDEVICE_REGION_START         MBR_SIZE /* Start of softdevice. */
#ifdef NRF52
    #ifdef RELEASE
        #define BOOTLOADER_REGION_START         0x00075000 /* Start of bootloader; must be multiple of 0x800 */
    #else
        #define BOOTLOADER_REGION_START         0x00075000 /* Start of bootloader; must be multiple of 0x800 */
    #endif
    #define BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS 0x0007D000
    #define BOOTLOADER_SETTINGS_ADDRESS     0x0007E000 /**< Start of bootloader settings; must be multiple of 0x400 */
    #define RIGADO_ZEROS_ADDRESS            0x0007F000 /**< Word that must be zero */
    #define DFU_APP_DATA_RESERVED           0x3000  /**< Size of Application Data that must be preserved 
                                                         between application updates. This value must be a 
                                                         multiple of page size. Page size is 0x1000 (1024d) 
                                                         bytes, thus this value must be 0x0000, 0x1000, 0x2000, 
                                                         0x3000 etc. */
    #define CODE_PAGE_SIZE                  0x1000  /**< Size of a flash codepage. Used for size of the reserved 
                                                         flash space in the bootloader region. Will be runtime checked 
                                                         against NRF_UICR->CODEPAGESIZE to ensure the region is correct. */
#elif defined(NRF51)
    #ifdef RELEASE
        #define BOOTLOADER_REGION_START         0x0003A800 /**< Start of bootloader; must be multiple of 0x800 */
    #else
        #define BOOTLOADER_REGION_START         0x00038000 /**< Start of bootloader; must be multiple of 0x800 */
    #endif
    #define BOOTLOADER_SETTINGS_ADDRESS     0x0003F800 /**< Start of bootloader settings; must be multiple of 0x400 */
    #define RIGADO_ZEROS_ADDRESS            0x0003FC00 /**< Word that must be zero */
    #define DFU_APP_DATA_RESERVED           0x1000     /**< Size of Application Data that must be preserved 
                                                         between application updates. This value must be a 
                                                         multiple of page size. Page size is 0x400 (1024d) 
                                                         bytes, thus this value must be 0x0000, 0x0400, 0x0800, 
                                                         0x0C00 etc. */
    #define CODE_PAGE_SIZE                  0x400      /**< Size of a flash codepage. Used for size of the reserved 
                                                         flash space in the bootloader region. Will be runtime checked 
                                                         against NRF_UICR->CODEPAGESIZE to ensure the region is correct. */
#endif

#define DFU_REGION_TOTAL_SIZE           (BOOTLOADER_REGION_START - CODE_REGION_1_START)                 /**< Total size of the region between SD and Bootloader. */

#define DFU_IMAGE_MAX_SIZE_FULL         (DFU_REGION_TOTAL_SIZE - DFU_APP_DATA_RESERVED)                 /**< Maximum size of a application, excluding save data from the application. */
#define DFU_IMAGE_MAX_SIZE_BANKED       (DFU_IMAGE_MAX_SIZE_FULL / 2)                                   /**< Maximum size of a application, excluding save data from the application. */
#define DFU_BL_IMAGE_MAX_SIZE           (BOOTLOADER_SETTINGS_ADDRESS - BOOTLOADER_REGION_START)         /**< Maximum size of a bootloader, excluding save data from the current bootloader. */

#define DFU_BANK_0_REGION_START         CODE_REGION_1_START                                             /**< Bank 0 region start. */
#define DFU_BANK_1_REGION_START         (DFU_BANK_0_REGION_START + DFU_IMAGE_MAX_SIZE_BANKED)           /**< Bank 1 region start. */
#define EMPTY_FLASH_MASK                0xFFFFFFFF                                                      /**< Bit mask that defines an empty address in flash. */

/* Packet identifiers, used for data packet callbacks, and also by the
 * serial transport. */
#define INVALID_PACKET     0x00   /**< Invalid packet identifier. */
#define INIT_PACKET        0x01   /**< Packet identifier for initialization packet. */
#define START_PACKET       0x02   /**< Packet identifier for the Data Start Packet. */
#define DATA_PACKET        0x03   /**< Packet identifier for a Data Packet. */
#define STOP_DATA_PACKET   0x04   /**< Packet identifier for the Data Stop Packet */
#define CONFIG_PACKET      0x05   /**< Packet identifier for the Config packet */
#define PATCH_INIT_PACKET  0x06   /**< Packet identifier for the Patch Init packet */
#define PATCH_DATA_PACKET  0x07   /**< Packet identifier for the Patch Data packet */
#define RESTART_PACKET     0x08

// Safe guard to ensure during compile time that the DFU_APP_DATA_RESERVED is a multiple of page size.
STATIC_ASSERT((((DFU_APP_DATA_RESERVED) & (CODE_PAGE_SIZE - 1)) == 0x00));

/* Rigado data page must_be_zero fields must match up with
 * RIGADO_ZEROS_ADDRESS */
STATIC_ASSERT(offsetof(rigado_data_t, must_be_zero) + RIGADO_DATA_ADDRESS
              == RIGADO_ZEROS_ADDRESS);

/**@brief Structure holding a start packet containing image sizes. */
typedef struct
{
    uint32_t sd_image_size;      /* SoftDevice image size, or zero if none */
    uint32_t bl_image_size;      /* Bootloader image size, or zero if none */
    uint32_t app_image_size;     /* App image size, or zero if none */
} dfu_start_packet_t;
STATIC_ASSERT((sizeof(dfu_start_packet_t) % 4) == 0);

/**@brief Structure holding an init packet containing crypto stuff. */
typedef struct {
    uint8_t iv[16];              /* AES-128 EAX IV */
    uint8_t tag[16];             /* AES-128 EAX tag */
} dfu_init_packet_t;
STATIC_ASSERT((sizeof(dfu_init_packet_t) % 4) == 0);

typedef struct {
    uint32_t patch_size;         /* The length of the patch data */
    uint32_t patch_crc;          /* The CRC of the patched image */
    uint32_t orig_crc;           /* The CRC of the starting image */
} dfu_patch_init_packet_t;
STATIC_ASSERT((sizeof(dfu_patch_init_packet_t) % 4) == 0);

typedef struct {
    uint8_t data[20];
} dfu_patch_data_packet_t;
STATIC_ASSERT((sizeof(dfu_patch_data_packet_t) % 4) == 0);

typedef struct {
    uint8_t old_key[16];	    /* Existing key */
    uint8_t new_key[16];	    /* New key, updated if nonzero */
    uint8_t new_mac[6];	        /* New MAC, updated if nonzero */
    uint8_t zeros[10];          /* Reserved data, must be zero */
} dfu_config_data_t;
STATIC_ASSERT((sizeof(dfu_config_data_t) % 4) == 0);

/**@brief Structure holding a config packet containing MAC / KEY update info */
typedef struct {
    uint8_t hdr[12];//todo remove
    uint8_t iv[16];             /* AES-128 EAX IV */
    uint8_t tag[16];            /* AES-128 EAX tag */
    
    dfu_config_data_t config;
    
} dfu_config_packet_t;
STATIC_ASSERT((sizeof(dfu_config_packet_t) % 4) == 0);

/**@brief Structure holding a bootloader init/data packet received.
 */
typedef struct
{
    uint32_t packet_length;      /* Packet length, in words */
    uint32_t *p_data_packet;     /* Data words */
} dfu_data_packet_t;

/**@brief Structure for holding dfu update packet. Packet type indicate the type of packet.
 */
typedef struct
{
    uint32_t   packet_type;                                                                             /**< Packet type, used to identify the content of the received packet referenced by data packet. */
    union
    {
        dfu_data_packet_t    data_packet;                                                               /**< Used when packet type is INIT_PACKET or DATA_PACKET. Packet contains data received for init or data. */
        dfu_start_packet_t * start_packet;                                                              /**< Used when packet type is START_DATA_PACKET. Will contain information on software to be updtaed, i.e. SoftDevice, Bootloader and/or Application along with image sizes. */
    } params;
} dfu_update_packet_t;

/**@brief DFU status error codes.
*/
typedef enum
{
    DFU_UPDATE_APP_COMPLETE,                                                                            /**< Status update of application complete.*/
    DFU_UPDATE_SD_COMPLETE,                                                                             /**< Status update of SoftDevice update complete. Note that this solely indicates that a new SoftDevice has been received and stored in bank 0 and 1. */
    DFU_UPDATE_SD_SWAPPED,                                                                              /**< Status update of SoftDevice update complete. Note that this solely indicates that a new SoftDevice has been received and stored in bank 0 and 1. */
    DFU_UPDATE_BOOT_COMPLETE,                                                                           /**< Status update complete.*/
    DFU_BANK_0_ERASED,                                                                                  /**< Status bank 0 erased.*/
    DFU_BANK_1_ERASED,                                                                                  /**< Status bank 1 erased.*/
    DFU_TIMEOUT,                                                                                        /**< Status timeout.*/
    DFU_RESET                                                                                           /**< Status Reset to indicate current update procedure has been aborted and system should reset. */
} dfu_update_status_code_t;

/**@brief Structure holding DFU complete event.
*/
typedef struct
{
    dfu_update_status_code_t status_code;                                                               /**< Device Firmware Update status. */
    uint32_t                 sd_size;                                                                   /**< Size of the recieved SoftDevice. */
    uint32_t                 bl_size;                                                                   /**< Size of the recieved BootLoader. */
    uint32_t                 app_size;                                                                  /**< Size of the recieved Application. */
    uint32_t                 sd_image_start;                                                            /**< Location in flash where the received SoftDevice image is stored. */
} dfu_update_status_t;

/**@brief Update complete handler type. */
typedef void (*dfu_complete_handler_t)(dfu_update_status_t dfu_update_status);

#endif // DFU_TYPES_H__

/**@} */
