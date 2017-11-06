/** @file at_proc.c
*
* @brief AT command input processing
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nrf_error.h"
#include "storage_intf.h"
#include "ringBuf.h"

#include "at_commands.h"
    
static bool m_at_command_is_ready;

static uint8_t new_ready_data[MAX_AT_DATA_LEN + MAX_AT_COMMAND_LEN];

void at_proc_set_cmd_ready(ringBuf_t * data, uint16_t len)
{
    m_at_command_is_ready = true;
    memset(new_ready_data, 0, sizeof(new_ready_data));
    ringBufRead(data, new_ready_data, len);
}

bool at_proc_is_cmd_ready(void)
{
    return m_at_command_is_ready;
}

void at_proc_process_command(void)
{
    uint32_t err_code;
    
    if(!m_at_command_is_ready)
    {
        return;
    }
    
    err_code = at_command_parse(new_ready_data);
    if(err_code == AT_RESULT_OK)
    {
        at_util_print_ok_response();
    }
    else if(err_code == AT_RESULT_UNKNOWN)
    {
        at_util_print_unknown_response();
    }
    else if(err_code == AT_RESULT_ERROR)
    {
        at_util_print_error_response();
    }
    else if(err_code == AT_RESULT_QUERY)
    {
        /* This error is used to denote when nothing should be printed because a response to a query was printed instead */
        /* This block is left here for reference */
    }
    else if(err_code == AT_RESULT_LOCKED)
    {
        at_util_print_locked_response();
    }
    
    m_at_command_is_ready = false;
}
