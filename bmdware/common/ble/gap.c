/** @file gap.c
*
* @brief GAP setup commands
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <string.h>
#include <ctype.h>
#include "app_error.h"
#include "ble_gap.h"
#include "storage_intf.h"

#include "gap_cfg.h"
#include "gap.h"

/**@brief   Function for the GAP initialization.
 *
 * @details This function will setup all the necessary GAP (Generic Access Profile)
 *          parameters of the device. It also sets the permissions and appearance.
 */
void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;

    err_code = gap_update_device_name();
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

uint32_t gap_update_device_name(void)
{
    const default_app_settings_t * p_settings = storage_intf_get();
    const uint8_t * dev_name = p_settings->device_name;
    
    bool valid_name = gap_validate_name((const char*)dev_name);       
    
    if(!valid_name)
    {
        default_app_settings_t settings;
        memcpy(&settings, p_settings, sizeof(settings));
        memset(settings.device_name, 0, sizeof(settings.device_name));
        memcpy(settings.device_name, DEFAULT_DEVICE_NAME, strlen(DEFAULT_DEVICE_NAME));
        storage_intf_set(&settings);
        storage_intf_save();
    }
    
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&sec_mode);
    return sd_ble_gap_device_name_set(&sec_mode, dev_name, strlen((const char*)dev_name));
}

bool gap_validate_name(const char * p_name)
{
    bool valid_name = true;
    uint8_t temp[MAX_DEVICE_NAME_LEN];
    uint8_t temp_name[MAX_DEVICE_NAME_LEN + 1];
    
    if(NULL == p_name)
    {
        return false;
    }
    
    memset(temp, 0x00, sizeof(temp));
    
    //memcmp requires word alignment; copy to local buffer
    memcpy(temp_name, p_name, sizeof(temp_name));
    
    if(memcmp(temp_name, temp, sizeof(temp)) != 0)
    {
        memset(temp, 0xFF, sizeof(temp));
        if(memcmp(temp_name, temp, sizeof(temp)) != 0)
        {
            //Check characters
            uint32_t idx = 0;

            char ch;
            do
            {
                ch = p_name[idx];
                if(ch == 0)
                {
                    if(idx == 0)
                    {
                        valid_name = false;
                    }
                    break;
                }
                
                if(!isprint((unsigned char)ch))
                {
                    valid_name = false;
                    break;
                }
                idx++;
            } while(ch);
        }
        else
        {
            valid_name = false;
        }
    }
    else
    {
        valid_name = false;
    }
    
    return valid_name;
}
