#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nrf.h>
#include <nrf_soc.h>

#include "fstorage.h"

#define WRITE_SIZE   (0x1000)
#define PAGE_SIZE    (NRF_FICR->CODEPAGESIZE)

static fstorage_cb_t callbacks[FSTORAGE_max];

typedef struct {
    fstorage_id_t id;	 /* Caller ID */
    uint8_t op_code;	 /* Requested operation */
    uint32_t flash_addr; /* Flash address to write / clear */
    uint8_t *data;       /* Data that should be written */
    int32_t len;         /* Total bytes to write / clear */
    int32_t offset;	 /* Byte offset of next operation */
} cmd_queue_element_t;

static struct {
    uint8_t count;  /* how many commands remain to be processed */
    uint8_t index;  /* index of the element currently being processed */
    cmd_queue_element_t cmd[FSTORAGE_QUEUE_LEN];
} cmd_queue;

/* Process next thing in the command queue */
static void cmd_process(void)
{
    /* If nothing in queue, we're done */
    if (cmd_queue.count == 0)
        return;

    int32_t len;
    cmd_queue_element_t *cmd;
    cmd = &cmd_queue.cmd[cmd_queue.index];

    switch (cmd->op_code)
    {
    case FSTORAGE_STORE_OP_CODE:
        /* Write up to 1 page */
        len = cmd->len - cmd->offset;
        if (len > WRITE_SIZE)
            len = WRITE_SIZE;

        if (sd_flash_write((uint32_t *)(cmd->flash_addr + cmd->offset),
                           (uint32_t *)(cmd->data + cmd->offset),
                           len / sizeof(uint32_t)) == NRF_ERROR_BUSY)
            return;

        cmd->offset += len;
        break;

    case FSTORAGE_CLEAR_OP_CODE:
        /* Clear 1 page */
        if (sd_flash_page_erase((cmd->flash_addr + cmd->offset) / PAGE_SIZE)
            == NRF_ERROR_BUSY)
            return;

        cmd->offset += PAGE_SIZE;
        break;

    default:
        break;
    }
}

/* Add new entry to command queue */
static uint32_t cmd_enqueue(fstorage_id_t id, uint8_t op_code,
                            uint32_t flash_addr, uint8_t *data, int32_t len)
{
    if ((flash_addr & 3) || (((uint32_t)data) & 3))
        return NRF_ERROR_INVALID_ADDR;

    if (cmd_queue.count >= FSTORAGE_QUEUE_LEN)
        return NRF_ERROR_NO_MEM;

    uint8_t next = (cmd_queue.index + cmd_queue.count) % FSTORAGE_QUEUE_LEN;
    cmd_queue.cmd[next].id = id;
    cmd_queue.cmd[next].op_code = op_code;
    cmd_queue.cmd[next].flash_addr = flash_addr;
    cmd_queue.cmd[next].data = data;
    cmd_queue.cmd[next].len = len;
    cmd_queue.cmd[next].offset = 0;

    if (cmd_queue.count++ == 0)
        cmd_process();

    return NRF_SUCCESS;
}

/* Handle system events */
void fstorage_sys_event_handler(uint32_t sys_evt)
{
    if (cmd_queue.count == 0)
        return;

    cmd_queue_element_t *cmd;
    cmd = &cmd_queue.cmd[cmd_queue.index];
    uint32_t ret = NRF_ERROR_TIMEOUT;

    if (sys_evt == NRF_EVT_FLASH_OPERATION_SUCCESS)
    {
        if (cmd->offset < cmd->len) {
            /* Still more to do for this command. */
            cmd_process();
            return;
        }

        /* Success */
        ret = NRF_SUCCESS;
    }

    /* Notify the app */
    callbacks[cmd->id](cmd->flash_addr, cmd->op_code, ret, cmd->data);

    /* Remove the handled command and process the next one */
    cmd_queue.index = (cmd_queue.index + 1) % FSTORAGE_QUEUE_LEN;
    if (--cmd_queue.count)
        cmd_process();
}

/* Initialize fstorage */
void fstorage_init(void)
{
    memset(&cmd_queue, 0, sizeof(cmd_queue));
    memset(callbacks, 0, sizeof(callbacks));
}

/* Register callback handler. */
uint32_t fstorage_register(fstorage_id_t id, fstorage_cb_t cb)
{
    callbacks[id] = cb;
    return NRF_SUCCESS;
}

/* Write data to flash and call the callback when it's finished. */
uint32_t fstorage_store(fstorage_id_t id, uint32_t dest, void *src, int len)
{
    return cmd_enqueue(id, FSTORAGE_STORE_OP_CODE, dest, src, len);
}

/* Clear flash and call the callback when it's finished.  "len" is rounded up
   to the nearest page size. */
uint32_t fstorage_clear(fstorage_id_t id, uint32_t dest, int len)
{
    return cmd_enqueue(id, FSTORAGE_CLEAR_OP_CODE, dest, NULL, len);
}

