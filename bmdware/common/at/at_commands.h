/** @file at_commands.h
*
* @brief AT Commands main processing
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __AT_COMMAND_H__
#define __AT_COMMAND_H__

#include <stdint.h>
#include <stdbool.h>
#include "ringbuf.h"

#define MAX_AT_COMMAND_LEN      20
#define MAX_AT_DATA_LEN         70
#define NUM_AT_COMMANDS         30
#define MAX_AT_CMD_ARGS         10

#define AT_RESULT_OK            0
#define AT_RESULT_ERROR         1
#define AT_RESULT_LOCKED        2
#define AT_RESULT_UNKNOWN       3
#define AT_RESULT_QUERY         4

#define EIGHT_BIT_STR_LEN           (2)
#define SIXTEEN_BIT_STR_LEN         (4)

typedef struct at_command_s
{
    char * command;
    
    uint8_t min_args;
    uint8_t max_args;
    bool require_unlock;
    
    uint32_t (*handler)(uint8_t argc, char ** argv, bool query);
    
} at_command_t;

uint16_t  process_at_command(uint8_t *data, uint8_t len);

bool at_cmd_is_ready(void);
void at_cmd_set_ready(ringBuf_t * data, uint16_t len);

extern const char * const * at_command_list;

/* Global list of implemented console commands, defined in at_commands_misc.c. */
extern const at_command_t cmds_default[];

/* Initialize all command lists */
void at_commands_init(void);

/* Register a new list of commands.  By default, the global
   "cmds_default" is registered.  More lists can be registered
   with this function, up to a max of MAX_AT_CMD_LISTS.
   Returns true on success. */
#define MAX_AT_CMD_LISTS 10
bool at_commands_register(const at_command_t *cmds);


/* AT command processing functions */
void at_proc_set_cmd_ready(ringBuf_t * data, uint16_t len);
bool at_proc_is_cmd_ready(void);
void at_proc_process_command(void);
  
/* Parsing function */
uint32_t at_command_parse(uint8_t * line);

/* Utility Functions */
uint32_t at_util_save_stored_data(void);
void at_util_print_ok_response(void);
void at_util_print_error_response(void);
void at_util_print_unknown_response(void);
void at_util_print_locked_response(void);
uint32_t at_util_uart_put_string(const uint8_t * const str);
uint32_t at_util_uart_put_bytes(const uint8_t * const bytes, uint32_t len);
void at_util_hex_str_to_array(const char * in_str, uint8_t * out_bytes, uint32_t out_bytes_len);
bool at_util_validate_hex_str(const char * in_str, uint32_t in_str_len);
bool at_util_validate_input_str(const uint8_t * in_str, uint32_t req_str_len);
uint32_t at_util_set_stroage_val(const uint8_t * value, uint8_t val_size, uint32_t storage_offset, uint32_t * converted_value);
bool at_util_validate_power(int8_t power);

#endif
