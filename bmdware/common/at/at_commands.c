/** @file at_commands.c
*
* @brief AT Command main processing
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nrf.h"
#include "at_commands.h"
#include "simple_uart.h"
#include "uart.h"
#include <ctype.h>
#include <string.h>
#include "version.h"
#include "storage_intf.h"
#include "ble_beacon_config.h"
#include "ble_nus.h"
#include "lock.h"
#include "app_error.h"
#include "uart.h"
#include "ble_debug_assert_handler.h"
#include "timer.h"
#include "ringbuf.h"

#include "advertising.h"

#define AT_RESULT_OK        0
#define AT_RESULT_ERROR     1
#define AT_RESULT_LOCKED    2
#define AT_RESULT_UNKNOWN   3

#define AT_COMMAND_HEADER       "at$"
#define AT_COMMAND_HEADER_SZ    3
#define AT_COMMAND_QUERY        '?'
#define AT_COMMAND_QUERY_SZ     1

#define AT_UNLOCK_COMMAND_INDEX    (15u) 

#define ARRAY_CNT(array) ((sizeof(array)/sizeof(array[0])))

/* Command list pointers.  One extra so it's always NULL terminated. */
static const at_command_t *m_commands[MAX_AT_CMD_LISTS + 1] = { NULL };

/* This is set to true when the lock state changes from locked to unlocked.
   The system will automatically relock after the next command. */
static bool m_should_relock;

/* Defined in at_commands_misc.c */
void at_commands_misc_init(void);

/* Defined in at_commands_beacon.c */
void at_commands_beacon_init(void);

/* Defined in at_commands_uart.c */
void at_commands_uart_init(void);

/* Defined in at_commands_gpio.c */
void at_commands_gpio_init(void);

void at_commands_init(void)
{
    at_commands_misc_init();
    at_commands_beacon_init();
    at_commands_uart_init();
    at_commands_gpio_init();
}

/* Register a new list of commands.  By default, the global
   "p_default_commands" is registered.  More lists can be registered
   with this function, up to a max of CONSOLE_MAX_CMD_LISTS.
   Returns true on success. */
bool at_commands_register(const at_command_t *cmds)
{
    for (uint32_t i = 0; i < MAX_AT_CMD_LISTS; i++)
    {
        if (m_commands[i] == NULL) 
        {
            m_commands[i] = cmds;
            return true;
        }
    }

    return false;
}

uint32_t at_command_parse(uint8_t * line)
{
    bool is_query = false;
    uint32_t argc = 0;
    static char * strtok_save;
    const at_command_t **pp_command_list;
    const at_command_t *p_cmd;
    bool is_name_command = false;
    
    /* Defined static so it's in static ram and not the stack */
    static char *argv[MAX_AT_CMD_ARGS];
    
    if(line == NULL)
    {
        return AT_RESULT_ERROR;
    }
    
    /* Split line into arguments */
    argv[0] = strtok_r( (char*)line, " ", &strtok_save );
    if ( argv[0] == NULL ) 
    {
        return AT_RESULT_ERROR;
    }
    
    /* Convert command to lowercase */
    unsigned char * line_ptr = (unsigned char *)argv[0];
    for ( ; *line_ptr; ++line_ptr) *line_ptr = (uint8_t)tolower(*line_ptr);
    
    /* SPECIAL CASE - If this is the name command, then don't convert to lower case */
    if(strncmp( &argv[0][AT_COMMAND_HEADER_SZ], "name", 4 ) == 0)
    {
        is_name_command = true;
    }
    
    /* Process argument and update argument count (command counts as 1 argument) */
    for( argc = 1; argc < MAX_AT_CMD_ARGS; argc++ )
    {
        argv[argc] = strtok_r( NULL, " ", &strtok_save );
        if( argv[argc] == NULL )
            break;
        
        /* Convert everything to lower case if this is not the name command */
        if(!is_name_command)
        {
            unsigned char * line_ptr = (unsigned char*)argv[argc];
            for ( ; *line_ptr; ++line_ptr) *line_ptr = (uint8_t)tolower(*line_ptr);
        }
    }
    
    /* Handle special case of command AT (compares against first two characters of header) */
    if((strncmp(argv[0], AT_COMMAND_HEADER, AT_COMMAND_HEADER_SZ - 1) == 0) &&
        (strlen(argv[0]) == AT_COMMAND_HEADER_SZ - 1) &&
        ((argc - 1) == 0))
    {
        /* This is the special AT command */
        return AT_RESULT_OK;
    }
    
    /* Verify command has AT command header */
    if(strncmp(argv[0], AT_COMMAND_HEADER, AT_COMMAND_HEADER_SZ) != 0)
    {
        return AT_RESULT_ERROR;
    }
    
    /* Find command, return unknown if not found */
    bool found = false;
    for (pp_command_list = m_commands; !found && *pp_command_list; pp_command_list++) 
    {
        for (p_cmd = *pp_command_list; !found && p_cmd->command != NULL; p_cmd++) 
        {            
            if( strncmp( &argv[0][AT_COMMAND_HEADER_SZ], p_cmd->command, strlen(p_cmd->command) ) == 0 ) 
            {
                found = true;
                break;
            }
        
        }
    }
    
    if(!found)
    {
        return AT_RESULT_UNKNOWN;
    }
    
    /* Check if command is a query */
    uint8_t command_len = strlen(argv[0]);
    is_query = (argv[0][command_len - 1] == AT_COMMAND_QUERY);
    
    /* Verify correct number of parameters was provided. If query, count should be 0 */
    if((argc - 1) < p_cmd->min_args || (argc - 1) > p_cmd->max_args)
    {
        return AT_RESULT_ERROR;
    }
    
    if(is_query && ((argc - 1) != 0))
    {
        return AT_RESULT_ERROR;
    }
        
    /* Check to see if command can be executed base on locked status (all queries are allowed as long as command can be queried) */
    if(p_cmd->require_unlock && lock_is_locked() && !is_query)
    {
        return AT_RESULT_LOCKED;
    }
    
    bool lock_state = lock_is_locked();
    
    
    
    /* Execute command handler function */
    uint32_t result = (*p_cmd->handler)(argc, (char**)argv, is_query);
    
    if(m_should_relock)
    {
        lock_set();
        m_should_relock = false;
    }
    
    if(lock_state == true && (lock_is_locked() == false))
    {
        m_should_relock = true;
    }
    
    return result;
}
