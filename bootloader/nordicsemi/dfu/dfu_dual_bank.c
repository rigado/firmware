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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "dfu.h"
#include "dfu_types.h"
#include "dfu_bank_internal.h"
#include "nrf.h"
#ifdef SDK10
    #include "nrf51.h"
    #include "nrf51_bitfields.h"
#else
    #include "nrf_soc.h"
#endif
#include "app_util.h"
#include "nrf_sdm.h"
#include "app_error.h"
#include "nrf_error.h"
#include "ble_flash.h"
#include "app_error.h"
#include "nordic_common.h"
#include "bootloader.h"
#include "bootloader_types.h"
#include "crc16.h"
#include "crc32.h"
#include "fstorage.h"
#include "nrf_gpio.h"
#include "nrf_mbr.h"
#include "patcher.h"

#include <tomcrypt.h>
#include "rigdfu.h"
#include "rigdfu_util.h"
#include "rigdfu_serial.h"

#include "ble_dfu.h"


static bool                         m_should_check_readback_protect = false;

//disable
#define rigdfu_serial_printf(...)

static dfu_state_t                  m_dfu_state;                /**< Current DFU state. */
static uint32_t                     m_image_size;               /**< Size of the image that will be transmitted. */

static dfu_callback_t               m_data_pkt_cb;              /**< Callback from DFU Bank module for notification of asynchronous operation such as flash prepare. */
static dfu_bank_func_t              m_functions;                /**< Structure holding operations for the selected update process. */

/* Fstorage */
static uint32_t target_base_address;

/** Packets */

static dfu_start_packet_t m_start_packet;
static uint16_t m_start_packet_length;

static dfu_init_packet_t m_init_packet;
static uint16_t m_init_packet_length;

static dfu_config_packet_t m_config_packet;
static uint16_t m_config_packet_length;

static dfu_patch_init_packet_t m_patch_init_packet;
static uint16_t m_patch_init_packet_length;

/** Data decryption and authentication */
static eax_state m_eax;
static bool m_decrypt;

/** Shared memory section: Used by config and patch */
static uint8_t m_shared_mem[sizeof(rigado_data_t)] __attribute__((aligned (4)));
static bool m_shared_mem_in_use;

static uint8_t m_patch_buffer[4096] __attribute__((aligned (4)));

/** State varible to denote if a patch is in progress */
//TODO: Remove?
static bool m_is_patching;

/* Prepare for decryption */
uint32_t decrypt_prepare(void)
{
    const uint8_t *key;
    if (rigado_get_key(&key) == false) {
        /* There's no key -- don't decrypt */
        m_decrypt = false;
    } else {
        /* Set up decryption.  Use the start packet contents as the header. */
        if (eax_init(&m_eax, 0, /* context, cipher id */
                     key, 16, /* key */
                     m_init_packet.iv, 16, /* nonce */
                     (uint8_t *)&m_start_packet,
                     sizeof(m_start_packet) /* header */
                ) != CRYPT_OK) {
            return NRF_ERROR_NOT_SUPPORTED;
        }

        /* Ensure that, if we're writing to the application location,
           the existing application has been marked invalid.  Otherwise,
           a subsequent boot could try to execute code that was not
           properly validated. */
        if (target_base_address == DFU_BANK_0_REGION_START)
        {
            bootloader_settings_t bootloader_settings;
            bootloader_settings_get(&bootloader_settings);
            if ((bootloader_settings.bank_0 != BANK_ERASED))
                return NRF_ERROR_INVALID_STATE;
        }

        m_decrypt = true;
    }

    if(!m_decrypt)
    {
        /* If the image is encrypted and encryption is not enabled, then the udpate will fail.
           Report Error to host. */
        dfu_init_packet_t zero_init_packet;
        memset(&zero_init_packet, 0, sizeof(zero_init_packet));
        if(memcmp(&m_init_packet, &zero_init_packet, sizeof(dfu_init_packet_t)) != 0)
        {
            return NRF_ERROR_INVALID_STATE;
        }
    }
    
    if(m_decrypt)
    {
        /* If the image should be encrypted and the iv + tag is 0, then the update will fail
           since this is an unencrypted image. */ 
        dfu_init_packet_t zero_init_packet;
        memset(&zero_init_packet, 0, sizeof(zero_init_packet));
        if(memcmp(&m_init_packet, &zero_init_packet, sizeof(dfu_init_packet_t)) == 0)
        {
            return NRF_ERROR_INVALID_STATE;
        }
    }
    
    m_dfu_state = DFU_STATE_INIT_PKT_DONE;
    return NRF_SUCCESS;
}

/* Decrypt a data packet in-place */
uint32_t decrypt_data(uint8_t *data, int len)
{
    if (eax_decrypt(&m_eax, data, data, len) != CRYPT_OK)
        return NRF_ERROR_INVALID_STATE;
    return NRF_SUCCESS;
}

/* Finish decryption and validate key */
uint32_t decrypt_validate(void)
{
    static uint8_t tag[16];
    unsigned long taglen = sizeof(tag);

    /* Always succeed if we weren't decrypting */
    if (!m_decrypt)
        return NRF_SUCCESS;

    /* Finalize EAX */
    if (eax_done(&m_eax, tag, &taglen) != CRYPT_OK ||
        taglen != 16)
        return NRF_ERROR_INVALID_DATA;

    /* Check tag */
    if (timingsafe_bcmp(tag, m_init_packet.tag, 16) != 0)
        return NRF_ERROR_INVALID_DATA;

    /* Check must_be_zero field */
    if (RIGADO_DATA->must_be_zero != 0)
    {
        /* Readback protection is designed to prevent reading or
           writing of code, but still allows erasing of individual
           pages.  If this value is nonzero, that indicates that the
           page has been erased, and our key was erased along with it.
           Don't allow code updates to be pushed in this case, as
           new unsigned code could read out the old signed code.

           Note that readback protection doesn't really work anyway,
           for other reasons.
        */
        return NRF_ERROR_INVALID_DATA;
    }

    /* Matched! */
    return NRF_SUCCESS;
}

static uint32_t store_data(uint8_t * data, uint32_t len)
{
    uint32_t err_code;
    
    err_code = fstorage_store(FSTORAGE_DFU,
                              target_base_address + m_data_received,
                              data, len);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    m_data_received += len;
    
    return NRF_SUCCESS;
}

static uint32_t patch_prepare()
{
    bootloader_settings_t bootloader_settings;
    bootloader_settings_get(&bootloader_settings);
    
    //check to make sure current app is valid
    if(bootloader_settings.bank_0 != BANK_VALID_APP)
    {
        return NRF_ERROR_INVALID_STATE;
    }
    
    uint32_t crc = crc32((uint8_t*)DFU_BANK_0_REGION_START, bootloader_settings.bank_0_size);
    if(crc != m_patch_init_packet.orig_crc)
    {
        return NRF_ERROR_INVALID_DATA;
    }

    m_is_patching = true;    
    if((IS_UPDATING_SD(m_start_packet) || IS_UPDATING_BL(m_start_packet))) 
    {
        // Only support patching of application
        return NRF_ERROR_NOT_SUPPORTED;
    }
    
    m_dfu_state = DFU_STATE_RX_PATCH_PKT;
    
    patch_init_t init;
    memset(&init, 0, sizeof(init));
    
    m_shared_mem_in_use = true;
    init.new_buf_ptr = m_patch_buffer;
    init.new_buf_size = sizeof(m_patch_buffer);
    init.new_size = m_start_packet.app_image_size;
    init.old_ptr = (uint8_t*)DFU_BANK_0_REGION_START;
    init.old_size = bootloader_settings.bank_0_size;
    init.store_func = store_data;
    patcher_init(&init);
    
    return NRF_SUCCESS;
}

static uint32_t validate_config(void)
{
    const uint8_t *key;
    if (rigado_get_key(&key))
    {
        int err;
        
        //init 
        err = eax_init(&m_eax, 0,               /* context, cipher id */
                         key, 16,               /* key */
                         m_config_packet.iv, 16,/* nonce */
                         m_config_packet.hdr, sizeof(m_config_packet.hdr)                /* no header */
                         );
        
        if(err != CRYPT_OK)
            return NRF_ERROR_INVALID_DATA;
        
        //decrypt
        err = eax_decrypt(&m_eax, (uint8_t*)&m_config_packet.config, (uint8_t*)&m_config_packet.config, sizeof(m_config_packet.config));
        
        if(err != CRYPT_OK)
            return NRF_ERROR_INVALID_DATA;
        
        //validate
        unsigned long taglen = 16;
        err = eax_done(&m_eax, m_config_packet.tag, &taglen);
        
        if(err != CRYPT_OK || taglen != 16)
            return NRF_ERROR_INVALID_DATA;
        
        /* If a key was set, verify that the host provided the
           existing key in the old_key field. */
        if (timingsafe_bcmp(key, m_config_packet.config.old_key, 16) != 0)
            return NRF_ERROR_INVALID_DATA;        
    }

    //nothing to decrypt, or we decrypted and the key matched
    return NRF_SUCCESS;
}


/* Reconfigure the MAC address and/or key */
uint32_t configure(void)
{
    //decrypt if needed and validate the oldkey
    uint32_t err = validate_config();
    
    if(err == NRF_SUCCESS)
    {
        if (!all_equal(m_config_packet.config.zeros, 0x00,
                       sizeof(m_config_packet.config.zeros)))
            return NRF_ERROR_INVALID_DATA;

        /* Copy the entire old data page.  Must be static so that we
           don't overflow the stack, and because fstorage_store needs
           it to persist.  Also needs to be aligned for fstorage_store! */
        //static rigado_data_t data __attribute__((aligned (4)));
        m_shared_mem_in_use = true;
        memcpy(&m_shared_mem, RIGADO_DATA, sizeof(m_shared_mem));
        rigado_data_t * rig_data = (rigado_data_t*)m_shared_mem;
                       
        /* If there is no key, clear out the entire struture before writing the
         * new key. */
        if(all_equal(rig_data->dfu_key, 0x00, 16) || all_equal(rig_data->dfu_key, 0xff, 16))
        {
            memset(rig_data, 0x00, sizeof(m_shared_mem));
        }
                       
        /* copy in new key */
        if (!all_equal(m_config_packet.config.new_key, 0x00, 16))
        {
            memcpy(rig_data->dfu_key, m_config_packet.config.new_key, 16);
        }
        
        /* copy in MAC */
        if (!all_equal(m_config_packet.config.new_mac, 0x00, 6)) 
        {
            /* MAC is stored little endian in the data page */
            for (int i = 0; i < 6; i++)
                rig_data->radio_mac[5 - i] = m_config_packet.config.new_mac[i];
        }

        /* Enqueue two operations: clear the old data page, and store the
           new data page. */
        fstorage_clear(FSTORAGE_DFU, (uint32_t)RIGADO_DATA, sizeof(rigado_data_t));
        fstorage_store(FSTORAGE_DFU, (uint32_t)RIGADO_DATA, rig_data, sizeof(rigado_data_t));

        /* All set.  The fstorage callback handler will trigger a response
           packet to the host when the store completes. */
        m_dfu_state = DFU_STATE_CONFIGURING;
        
        return NRF_SUCCESS;
    }
    
    return err;
}

/**@brief Function for handling callbacks from fstorage module.
 */
static void fstorage_callback_handler(uint32_t address,
                                      uint8_t op_code,
                                      uint32_t result,
                                      void *p_data)
{
    if (m_data_pkt_cb != NULL)
    {
        switch (op_code)
        {
            case FSTORAGE_STORE_OP_CODE:
                if (m_dfu_state == DFU_STATE_RX_DATA_PKT) {
                    m_data_pkt_cb(DATA_PACKET, result, (uint8_t *)p_data);
                }
                else if (m_dfu_state == DFU_STATE_CONFIGURING) {
                    m_shared_mem_in_use = false;
                    m_should_check_readback_protect = true;

                    m_data_pkt_cb(CONFIG_PACKET, result, (uint8_t *)p_data);
                }
                else if (m_dfu_state == DFU_STATE_RX_PATCH_PKT) {
                    m_data_pkt_cb(PATCH_DATA_PACKET, result, (uint8_t *)p_data);
                }
                break;

            case FSTORAGE_CLEAR_OP_CODE:
                if (m_dfu_state == DFU_STATE_PREPARING &&
                    address == DFU_BANK_0_REGION_START)
                {
                    dfu_update_status_t update_status = {DFU_BANK_0_ERASED, };
                    bootloader_dfu_update_process(update_status);

                    m_dfu_state = DFU_STATE_RDY;
                    m_data_pkt_cb(START_PACKET, result, (uint8_t *)p_data);
                }
                else if(m_dfu_state == DFU_STATE_RESTART)
                {                    
                    m_dfu_state = DFU_STATE_IDLE;
                    m_data_pkt_cb(RESTART_PACKET, result, (uint8_t *)p_data);
                }
                break;

            default:
                break;
        }
    }
    APP_ERROR_CHECK(result);
}

/**@brief   Function for calculating storage offset for receiving SoftDevice image.
 *
 * @details When a new SoftDevice is received it will be temporary stored in flash before moved to
 *          address 0x0. In order to succesfully validate transfer and relocation it is important
 *          that temporary image and final installed image does not ovwerlap hence an offset must
 *          be calculated in case new image is larger than currently installed SoftDevice.
 */
uint32_t offset_calculate(uint32_t sd_image_size)
{
    uint32_t offset = 0;
    
    if (m_start_packet.sd_image_size > DFU_BANK_0_REGION_START)
    {
        uint32_t page_mask = (CODE_PAGE_SIZE - 1);
        uint32_t diff = m_start_packet.sd_image_size - DFU_BANK_0_REGION_START;
        
        offset = diff & ~page_mask;
        
        // Align offset to next page if image size is not page sized.
        if ((diff & page_mask) > 0)
        {
            offset += CODE_PAGE_SIZE;
        }
        
        //account for MBR page
        offset += CODE_PAGE_SIZE;
    }

    
    return offset;
}

/**@brief   Function for preparing of flash before receiving SoftDevice image.
 *
 * @details This function will erase current application area to ensure sufficient amount of
 *          storage for the SoftDevice image. Upon erase complete a callback will be done.
 *          See \ref dfu_bank_prepare_t for further details.
 */
static void dfu_prepare_func_app_erase(uint32_t image_size)
{
    uint32_t err_code;

    target_base_address = DFU_BANK_0_REGION_START;

    // Current application must be cleared for the SoftDevice
    m_dfu_state = DFU_STATE_PREPARING;
    err_code = fstorage_clear(FSTORAGE_DFU,
                              DFU_BANK_0_REGION_START, m_image_size);
    APP_ERROR_CHECK(err_code);
}

/**@brief   Function for preparing before receiving application or bootloader image.
 *
 * @details As swap area is prepared during init then this function only update current state and\
 *          issue a callback.
 */
static void dfu_prepare_func(uint32_t image_size)
{
    target_base_address = DFU_BANK_1_REGION_START;

    m_dfu_state = DFU_STATE_RDY;
    
    if(m_is_patching) 
    {
        bootloader_settings_t bootloader_settings;
        bootloader_settings_get(&bootloader_settings);
    }

    if (m_data_pkt_cb != NULL)
    {
        // Flash has already been prepared. Issue callback immediately.
        m_data_pkt_cb(START_PACKET, NRF_SUCCESS, NULL);
    }
}

/**@brief Function for activating received SoftDevice image.
 *
 *  @note This function will not move the SoftDevice image.
 *        The bootloader settings will be marked as SoftDevice update complete and the swapping of
 *        current SoftDevice will occur after system reset.
 *
 * @return NRF_SUCCESS on success.
 */
static uint32_t dfu_activate_sd(void)
{
    dfu_update_status_t update_status;

    update_status.status_code    = DFU_UPDATE_SD_COMPLETE;
    update_status.sd_image_start = DFU_BANK_0_REGION_START 
        + offset_calculate(m_start_packet.sd_image_size);
    update_status.sd_size        = m_start_packet.sd_image_size;
    update_status.bl_size        = m_start_packet.bl_image_size;
    update_status.app_size       = m_start_packet.app_image_size;

    bootloader_dfu_update_process(update_status);

    return NRF_SUCCESS;
}


/**@brief Function for activating received Application image.
 *
 *  @details This function will move the received application image fram swap (bank 1) to
 *           application area (bank 0).
 *
 * @return NRF_SUCCESS on success. Error code otherwise.
 */
static uint32_t dfu_activate_app(void)
{
    uint32_t err_code;

    // Erase BANK 0.
    err_code = fstorage_clear(FSTORAGE_DFU,
                              DFU_BANK_0_REGION_START,
                              m_start_packet.app_image_size);
    APP_ERROR_CHECK(err_code);

    err_code = fstorage_store(FSTORAGE_DFU,
                              DFU_BANK_0_REGION_START,
                              (uint8_t *)DFU_BANK_1_REGION_START,
                              m_start_packet.app_image_size);

    if (err_code == NRF_SUCCESS)
    {
        dfu_update_status_t update_status;

        update_status.status_code = DFU_UPDATE_APP_COMPLETE;
        update_status.app_size    = m_start_packet.app_image_size;

        bootloader_dfu_update_process(update_status);
    }

    return err_code;
}


/**@brief Function for activating received Bootloader image.
 *
 *  @note This function will not move the bootloader image.
 *        The bootloader settings will be marked as Bootloader update complete and the swapping of
 *        current bootloader will occur after system reset.
 *
 * @return NRF_SUCCESS on success.
 */
static uint32_t dfu_activate_bl(void)
{
    dfu_update_status_t update_status;

    update_status.status_code = DFU_UPDATE_BOOT_COMPLETE;
    update_status.sd_size     = m_start_packet.sd_image_size;
    update_status.bl_size     = m_start_packet.bl_image_size;
    update_status.app_size    = m_start_packet.app_image_size;

    bootloader_dfu_update_process(update_status);

    return NRF_SUCCESS;
}

uint32_t dfu_init(dfu_state_t new_state)
{
    uint32_t                err_code;
    bootloader_settings_t   bootloader_settings;
    dfu_update_status_t     update_status;

    err_code = fstorage_register(FSTORAGE_DFU, fstorage_callback_handler);

    m_dfu_state = new_state;
    
    /* Check / clear swap area */
    bootloader_settings_get(&bootloader_settings);
    if ((bootloader_settings.bank_1 != BANK_ERASED) ||
        (*(uint32_t *)DFU_BANK_1_REGION_START != EMPTY_FLASH_MASK))
    {
        err_code = fstorage_clear(FSTORAGE_DFU,
                                  DFU_BANK_1_REGION_START,
                                  DFU_IMAGE_MAX_SIZE_BANKED);
        if (err_code != NRF_SUCCESS)
        {
            m_dfu_state = DFU_STATE_INIT_ERROR;
            return err_code;
        }

        update_status.status_code = DFU_BANK_1_ERASED;
        bootloader_dfu_update_process(update_status);
    }
    else if(m_dfu_state == DFU_STATE_RESTART)
    {
        m_dfu_state = DFU_STATE_IDLE;
        m_data_pkt_cb(RESTART_PACKET, NRF_SUCCESS, NULL);
    }

    m_data_received = 0;
    m_is_patching   = false;

    return NRF_SUCCESS;
}

bool dfu_should_check_readback_protect(void)
{
    return m_should_check_readback_protect;
}


void dfu_register_callback(dfu_callback_t callback_handler)
{
    m_data_pkt_cb = callback_handler;
}

static uint32_t dfu_receive_packet_helper(dfu_update_packet_t *p_packet,
                                          void *destination,
                                          int expected_length,
                                          uint16_t *current_length)
{
    int p_len = p_packet->params.data_packet.packet_length *
        sizeof(uint32_t);

    /* If too much data, return an error */
    if ((*current_length + p_len) > expected_length)
        return NRF_ERROR_DATA_SIZE;

    /* Copy data */
    memcpy(((uint8_t *)destination) + *current_length,
           p_packet->params.data_packet.p_data_packet,
           p_len);
    *current_length += p_len;

    /* Do we still need more? */
    if (*current_length != expected_length)
        return NRF_ERROR_INVALID_LENGTH;

    return NRF_SUCCESS;
}

uint32_t dfu_start_pkt_handle(dfu_update_packet_t * p_packet)
{
    uint32_t err_code;

    if (m_dfu_state == DFU_STATE_IDLE) {
        m_start_packet_length = 0;
        m_dfu_state = DFU_STATE_RX_START_PKT;
    }

    if (m_dfu_state != DFU_STATE_RX_START_PKT)
        return NRF_ERROR_INVALID_STATE;

    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;

    err_code = dfu_receive_packet_helper(p_packet, &m_start_packet,
                                         sizeof(m_start_packet),
                                         &m_start_packet_length);
    if (err_code != NRF_SUCCESS)
        return err_code;

    /* Got the full start packet; validate and configure */

    if (IS_UPDATING_APP(m_start_packet) &&
        (IS_UPDATING_SD(m_start_packet) || IS_UPDATING_BL(m_start_packet)))
    {
        // App update is only supported independently.
        return NRF_ERROR_NOT_SUPPORTED;
    }

    if (!(IS_WORD_SIZED(m_start_packet.sd_image_size) &&
          IS_WORD_SIZED(m_start_packet.bl_image_size) &&
          IS_WORD_SIZED(m_start_packet.app_image_size)))
    {
        // Image_sizes are not a multiple of 4 (word size).
        return NRF_ERROR_NOT_SUPPORTED;
    } 
    
    m_image_size = m_start_packet.sd_image_size + m_start_packet.bl_image_size +
                   m_start_packet.app_image_size;

    if (m_start_packet.bl_image_size > DFU_BL_IMAGE_MAX_SIZE)
    {
        return NRF_ERROR_DATA_SIZE;
    }

    if (IS_UPDATING_SD(m_start_packet))
    {
        if (m_image_size > (DFU_IMAGE_MAX_SIZE_FULL - CODE_PAGE_SIZE))
        {
            return NRF_ERROR_DATA_SIZE;
        }
        //m_sd_im
        m_functions.prepare  = dfu_prepare_func_app_erase;
        m_functions.activate = dfu_activate_sd;
    }
    else
    {
        if (m_image_size > DFU_IMAGE_MAX_SIZE_BANKED)
        {
            return NRF_ERROR_DATA_SIZE;
        }

        m_functions.prepare = dfu_prepare_func;
        if (IS_UPDATING_BL(m_start_packet))
        {
            m_functions.activate = dfu_activate_bl;
        }
        else
        {
            m_functions.activate = dfu_activate_app;
//            if(IS_PATCHING(m_start_packet))
//            {
//                m_is_patching = true;
//            }
        }
    }

    /* Prepare for flash download */
    m_functions.prepare(m_image_size);
    return NRF_SUCCESS;
}


uint32_t dfu_data_pkt_handle(dfu_update_packet_t * p_packet)
{
    uint32_t   data_length;
    uint32_t   err_code;
    uint8_t  * p_data;

    if (p_packet == NULL)
    {
        return NRF_ERROR_NULL;
    }

    // Check pointer alignment.
    if (!is_word_aligned(p_packet->params.data_packet.p_data_packet))
    {
        // The p_data_packet is not word aligned address.
        return NRF_ERROR_INVALID_ADDR;
    }
    
    /* If the softdevice is being updated, then we may need to offset where
     * the softdevice update is stored incase the new softdevice is larger than
     * the current softdevice. */
    if(m_start_packet.sd_image_size > 0 && target_base_address == DFU_BANK_0_REGION_START)
    {
        target_base_address += offset_calculate(m_start_packet.sd_image_size);
    }

    switch (m_dfu_state)
    {
        case DFU_STATE_INIT_PKT_DONE:
            m_dfu_state = DFU_STATE_RX_DATA_PKT;
            //fall through - if not performing a patch
        case DFU_STATE_RX_DATA_PKT:
            
            data_length = p_packet->params.data_packet.packet_length * sizeof(uint32_t);

            if ((m_data_received + data_length) > m_image_size)
            {
                // The caller is trying to write more bytes into the flash than the size provided to
                // the dfu_image_size_set function. This is treated as a serious error condition and
                // an unrecoverable one. Hence point the variable mp_app_write_address to the top of
                // the flash area. This will ensure that all future application data packet writes
                // will be blocked because of the above check.
                m_data_received = 0xFFFFFFFF;

                return NRF_ERROR_DATA_SIZE;
            }

            // Valid peer activity detected. Hence restart the DFU timer.
            err_code = bootloader_timeout_reset();
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }

            p_data = (uint8_t *)p_packet->params.data_packet.p_data_packet;

            if (m_decrypt) {
                err_code = decrypt_data(p_data, data_length);
                if (err_code != NRF_SUCCESS)
                    return err_code;
            }

            //TODO: Insert patching code here??
            //Need to determine if system is sending a patch file or full firmware image
            err_code = fstorage_store(FSTORAGE_DFU,
                                      target_base_address + m_data_received,
                                      p_data, data_length);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }

            m_data_received += data_length;

            if (m_data_received != m_image_size)
            {
                // The entire image is not received yet. More data is expected.
                err_code = NRF_ERROR_INVALID_LENGTH;
            }
            else
            {
                // The entire image has been received. Return NRF_SUCCESS.
                err_code = NRF_SUCCESS;
            }
            break;
        default:
            err_code = NRF_ERROR_INVALID_STATE;
            break;
    }

    return err_code;
}

uint32_t dfu_init_pkt_handle(dfu_update_packet_t * p_packet)
{
    uint32_t err_code;

    if (m_dfu_state == DFU_STATE_RDY) {
        m_init_packet_length = 0;
        m_dfu_state = DFU_STATE_RX_INIT_PKT;
    }

    if (m_dfu_state != DFU_STATE_RX_INIT_PKT ||
        IMAGE_WRITE_IN_PROGRESS())
        return NRF_ERROR_INVALID_STATE;

    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;

    err_code = dfu_receive_packet_helper(p_packet, &m_init_packet,
                                         sizeof(m_init_packet),
                                         &m_init_packet_length);
    if (err_code != NRF_SUCCESS)
        return err_code;

    /* Have the full packet, process it */
    return decrypt_prepare();
}

uint32_t dfu_patch_init_pkt_handle(dfu_update_packet_t * p_packet)
{
    uint32_t err_code;
    if (m_dfu_state == DFU_STATE_INIT_PKT_DONE) {
        m_patch_init_packet_length = 0;
        m_dfu_state = DFU_STATE_RX_PATCH_INIT_PKT;
    }
    
    if (m_dfu_state != DFU_STATE_RX_PATCH_INIT_PKT ||
        IMAGE_WRITE_IN_PROGRESS())
        return NRF_ERROR_INVALID_STATE;
    
    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;
    
     err_code = dfu_receive_packet_helper(p_packet, &m_patch_init_packet,
                                         sizeof(m_patch_init_packet),
                                         &m_patch_init_packet_length);
    if (err_code != NRF_SUCCESS)
        return err_code;
    
    /* Have the full packet, process it */
    return patch_prepare();
}

int32_t dfu_patch_data_pkt_handle(uint8_t * p_data, uint32_t length)
{
    uint32_t err_code;
    int32_t status;
    
    if(m_dfu_state != DFU_STATE_RX_PATCH_PKT) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;
    
    /* When the packet is null, attempt to process more data */
    if(p_data != NULL)
    {
        if (m_decrypt) {
            err_code = decrypt_data(p_data, length);
            if (err_code != NRF_SUCCESS)
                return err_code;
        }
        status = patcher_add_data(p_data, length);
        if(status == PATCHER_INPUT_FULL)
        {
            return status;
        }
    }

    status = patcher_patch();
    return status;
}

uint32_t dfu_config_pkt_handle(dfu_update_packet_t * p_packet)
{
    uint32_t err_code;

    if (m_dfu_state == DFU_STATE_IDLE) {
        m_config_packet_length = 0;
        m_dfu_state = DFU_STATE_RX_CONFIG_PKT;
    }

    if (m_dfu_state != DFU_STATE_RX_CONFIG_PKT)
        return NRF_ERROR_INVALID_STATE;

    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;

    err_code = dfu_receive_packet_helper(p_packet, &m_config_packet,
                                         sizeof(m_config_packet),
                                         &m_config_packet_length);
    if (err_code != NRF_SUCCESS)
        return err_code;

    /* Have the full packet, process it */
    return configure();
}

uint32_t dfu_image_validate()
{
    uint32_t err_code;
    bool validate_patch = false;

    if (!(m_dfu_state == DFU_STATE_RX_DATA_PKT || m_dfu_state == DFU_STATE_RX_PATCH_PKT))
        return NRF_ERROR_INVALID_STATE;

    if(m_dfu_state == DFU_STATE_RX_PATCH_PKT)
        validate_patch = true;
    
    m_dfu_state = DFU_STATE_VALIDATE;

    if (m_data_received != m_image_size)
    {
        // Image not yet fully transfered by the peer or the
        // peer has attempted to write too much data.
        return NRF_ERROR_INVALID_STATE;
    }

    err_code = bootloader_timeout_reset();
    if (err_code != NRF_SUCCESS)
        return err_code;

    // Finish EAX cipher, and check that the tag in m_init_packet matches,
    // if we were decrypting.
    err_code = decrypt_validate();
    if (err_code != NRF_SUCCESS)
        return NRF_ERROR_INVALID_DATA;
    
    if(validate_patch)
    {
        uint32_t crc = crc32((uint8_t*)DFU_BANK_1_REGION_START, m_image_size);
        if(crc != m_patch_init_packet.patch_crc)
        {
            return NRF_ERROR_INVALID_DATA;
        }
    }
    
    m_dfu_state = DFU_STATE_WAIT_4_ACTIVATE;
    return NRF_SUCCESS;
}


uint32_t dfu_image_activate()
{
    uint32_t err_code;

    switch (m_dfu_state)
    {
        case DFU_STATE_WAIT_4_ACTIVATE:

            // Stop the DFU Timer because the peer activity need not be monitored any longer.
            bootloader_timeout_stop();

            err_code = m_functions.activate();
            break;

        default:
            err_code = NRF_ERROR_INVALID_STATE;
            break;
    }

    return err_code;
}


void dfu_reset(void)
{
    dfu_update_status_t update_status;

    update_status.status_code = DFU_RESET;

    bootloader_dfu_update_process(update_status);
}


static uint32_t dfu_compare_block(uint32_t * ptr1, uint32_t * ptr2, uint32_t len)
{
    sd_mbr_command_t sd_mbr_cmd;

    sd_mbr_cmd.command             = SD_MBR_COMMAND_COMPARE;
    sd_mbr_cmd.params.compare.ptr1 = ptr1;
    sd_mbr_cmd.params.compare.ptr2 = ptr2;
    sd_mbr_cmd.params.compare.len  = len / sizeof(uint32_t);

    return sd_mbr_command(&sd_mbr_cmd);
}


static uint32_t dfu_copy_sd(uint32_t * src, uint32_t * dst, uint32_t len)
{
    sd_mbr_command_t sd_mbr_cmd;

    sd_mbr_cmd.command            = SD_MBR_COMMAND_COPY_SD;
    sd_mbr_cmd.params.copy_sd.src = src;
    sd_mbr_cmd.params.copy_sd.dst = dst;
    sd_mbr_cmd.params.copy_sd.len = len / sizeof(uint32_t);

    return sd_mbr_command(&sd_mbr_cmd);
}


static uint32_t dfu_sd_img_block_swap(uint32_t * src, 
                                      uint32_t * dst, 
                                      uint32_t len, 
                                      uint32_t block_size)
{
    // It is neccesarry to swap the new SoftDevice in 3 rounds to ensure correct copy of data
    // and verifucation of data in case power reset occurs during write to flash. 
    // To ensure the robustness of swapping the images are compared backwards till start of
    // image swap. If the back is identical everything is swapped.
    uint32_t err_code = dfu_compare_block(src, dst, len);
    if (err_code == NRF_SUCCESS)
    {
        return err_code;
    }

    if ((uint32_t)dst > SOFTDEVICE_REGION_START)
    {
        err_code = dfu_sd_img_block_swap((uint32_t *)((uint32_t)src - block_size), 
                                         (uint32_t *)((uint32_t)dst - block_size), 
                                         block_size, 
                                         block_size);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    err_code = dfu_copy_sd(src, dst, len);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    return dfu_compare_block(src, dst, len);
}


uint32_t dfu_sd_image_swap(void)
{
    bootloader_settings_t boot_settings;

    bootloader_settings_get(&boot_settings);

    if (boot_settings.sd_image_size == 0)
    {
        return NRF_SUCCESS;
    }
    
    if ((SOFTDEVICE_REGION_START + boot_settings.sd_image_size) > boot_settings.sd_image_start)
    {
        uint32_t err_code;
        uint32_t sd_start        = SOFTDEVICE_REGION_START;
        uint32_t block_size      = (boot_settings.sd_image_start - sd_start) / 2;
        uint32_t image_end       = boot_settings.sd_image_start + boot_settings.sd_image_size;

        uint32_t img_block_start = boot_settings.sd_image_start + 2 * block_size;
        uint32_t sd_block_start  = sd_start + 2 * block_size;

        if (SD_SIZE_GET(MBR_SIZE) < boot_settings.sd_image_size)
        {
            // This will clear a page thus ensuring the old image is invalidated before swapping.
            err_code = dfu_copy_sd((uint32_t *)(sd_start + block_size), 
                                   (uint32_t *)(sd_start + block_size), 
                                   sizeof(uint32_t));
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }

            err_code = dfu_copy_sd((uint32_t *)sd_start, (uint32_t *)sd_start, sizeof(uint32_t));
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }
        
        return dfu_sd_img_block_swap((uint32_t *)img_block_start, 
                                     (uint32_t *)sd_block_start, 
                                     image_end - img_block_start, 
                                     block_size);
    }
    else
    {
        if (boot_settings.sd_image_size != 0)
        {
            return dfu_copy_sd((uint32_t *)boot_settings.sd_image_start,
                               (uint32_t *)SOFTDEVICE_REGION_START, 
                               boot_settings.sd_image_size);
        }
    }

    return NRF_SUCCESS;
}


uint32_t dfu_bl_image_swap(void)
{
    bootloader_settings_t bootloader_settings;
    sd_mbr_command_t      sd_mbr_cmd;

    bootloader_settings_get(&bootloader_settings);

    if (bootloader_settings.bl_image_size != 0)
    {
        uint32_t bl_image_start = (bootloader_settings.sd_image_size == 0) ?
                                  DFU_BANK_1_REGION_START :
                                  bootloader_settings.sd_image_start + 
                                  bootloader_settings.sd_image_size;

        sd_mbr_cmd.command               = SD_MBR_COMMAND_COPY_BL;
        sd_mbr_cmd.params.copy_bl.bl_src = (uint32_t *)(bl_image_start);
        sd_mbr_cmd.params.copy_bl.bl_len = bootloader_settings.bl_image_size / sizeof(uint32_t);
        
        return sd_mbr_command(&sd_mbr_cmd);
    }
    return NRF_SUCCESS;
}


uint32_t dfu_bl_image_validate(void)
{
    bootloader_settings_t bootloader_settings;
    sd_mbr_command_t      sd_mbr_cmd;

    bootloader_settings_get(&bootloader_settings);

    if (bootloader_settings.bl_image_size == 0)
    {
        return NRF_SUCCESS;
    }
    
    /* First, check normal location when BANK 1 address was not changed */
    uint32_t bl_image_start = (bootloader_settings.sd_image_size == 0) ?
                              DFU_BANK_1_REGION_START :
                              bootloader_settings.sd_image_start +
                              bootloader_settings.sd_image_size;

    sd_mbr_cmd.command             = SD_MBR_COMMAND_COMPARE;
    sd_mbr_cmd.params.compare.ptr1 = (uint32_t *)BOOTLOADER_REGION_START;
    sd_mbr_cmd.params.compare.ptr2 = (uint32_t *)(bl_image_start);
    sd_mbr_cmd.params.compare.len  = bootloader_settings.bl_image_size / sizeof(uint32_t);

    uint32_t err_code = sd_mbr_command(&sd_mbr_cmd);
    if(err_code == NRF_SUCCESS)
    {
        return err_code;
    }
    
    /* Workaround Checks for when the BANK 1 address changed.  Verions 3.0.0 and Versions 3.1.0 had incorrect
       BANK 1 starting locations.  In this case, if the above validation check fails, try the 
       other possible BANK 1 starting locations.  It's possible the bootloader is valid but that the update
       was performed using a different start address.  When the bootloader resets after an update, it is running
       the new bootloader code.  If this new bootloader code has a different start address for BANK 1, then the
       validation check after update will fail!  The below code attempts to verify the running code against previous
       known BANK 1 start addresses. If the BANK 1 address is changed in future versions, this validation code
       will need to be updated. 
    */
#ifdef SDK10
    uint32_t validation_alternative_addresses[] = { DFU_3_0_0_BANK_1_LOCATION, DFU_3_1_0_BANK_1_LOCATION };
    for(uint8_t i = 0; i < sizeof(validation_alternative_addresses)/(sizeof(validation_alternative_addresses[0])); i++)
    {
        bl_image_start                   = validation_alternative_addresses[i];
        sd_mbr_cmd.command               = SD_MBR_COMMAND_COMPARE;
        sd_mbr_cmd.params.compare.ptr1 = (uint32_t *)BOOTLOADER_REGION_START;
        sd_mbr_cmd.params.compare.ptr2 = (uint32_t *)(bl_image_start);
        sd_mbr_cmd.params.compare.len  = bootloader_settings.bl_image_size / sizeof(uint32_t);
        err_code = sd_mbr_command(&sd_mbr_cmd);
        if(err_code == NRF_SUCCESS)
        {
            return NRF_SUCCESS;
        }
    }
#endif
    
    return err_code;
}


uint32_t dfu_sd_image_validate(void)
{
    bootloader_settings_t bootloader_settings;
    sd_mbr_command_t      sd_mbr_cmd;

    bootloader_settings_get(&bootloader_settings);

    if (bootloader_settings.sd_image_size == 0)
    {
        return NRF_SUCCESS;
    }
    
    if ((SOFTDEVICE_REGION_START + bootloader_settings.sd_image_size) > bootloader_settings.sd_image_start)
    {
        uint32_t sd_start        = SOFTDEVICE_REGION_START;
        uint32_t block_size      = (bootloader_settings.sd_image_start - sd_start) / 2;
        uint32_t image_end       = bootloader_settings.sd_image_start + 
                                   bootloader_settings.sd_image_size;

        uint32_t img_block_start = bootloader_settings.sd_image_start + 2 * block_size;
        uint32_t sd_block_start  = sd_start + 2 * block_size;
        
        if (SD_SIZE_GET(MBR_SIZE) < bootloader_settings.sd_image_size)
        {
            return NRF_ERROR_NULL;
        }

        return dfu_sd_img_block_swap((uint32_t *)img_block_start, 
                                     (uint32_t *)sd_block_start, 
                                     image_end - img_block_start, 
                                     block_size);
    }
    
    sd_mbr_cmd.command             = SD_MBR_COMMAND_COMPARE;
    sd_mbr_cmd.params.compare.ptr1 = (uint32_t *)SOFTDEVICE_REGION_START;
    sd_mbr_cmd.params.compare.ptr2 = (uint32_t *)bootloader_settings.sd_image_start;
    sd_mbr_cmd.params.compare.len  = bootloader_settings.sd_image_size / sizeof(uint32_t);

    return sd_mbr_command(&sd_mbr_cmd);
}

uint8_t * dfu_get_shared_mem(void)
{
    if(m_shared_mem_in_use)
    {
        return NULL;
    }
    
    m_shared_mem_in_use = true;
    return m_shared_mem;
}

void dfu_release_shared_mem(void)
{
    m_shared_mem_in_use = false;
}
