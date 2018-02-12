#ifndef DFU_TRANSPORT_SERIAL
#define DFU_TRANSPORT_SERIAL

#include <stdint.h>

/* pkt_len byte + op_id */
#define SERIAL_FRAME_HDR_SZ 2

/* 0xAA occurs only immediately before a frame */
#define SERIAL_FRAME_MARKER 0xAA

/* In a frame:
   0xAB is sent over the wire as 0xAB 0xAB
   0xAA is sent over the wire as 0xAB 0xAC */
#define SERIAL_FRAME_ESCAPE 0xAB
#define SERIAL_FRAME_ESCAPE_ESCAPE 0xAB
#define SERIAL_FRAME_ESCAPE_MARKER 0xAC

typedef struct serial_frame {
    uint8_t pad[2];
    uint8_t packet_len;    /* Total packet length, including this byte */
    uint8_t opcode;        /* Opcode, from SERIAL_OP constants below */
    uint8_t data[253];     /* Data, up to 253 bytes */
} serial_frame_t __attribute__((aligned(4)));


typedef enum
{
    SERIAL_OP_START_DFU = 1,
    SERIAL_OP_INITIALIZE_DFU = 2,
    SERIAL_OP_RECEIVE_FIRMWARE_IMAGE = 3,
    SERIAL_OP_VALIDATE_FIRMWARE_IMAGE = 4,
    SERIAL_OP_ACTIVATE_FIRMWARE_AND_RESET = 5,
    SERIAL_OP_SYSTEM_RESET = 6,
    SERIAL_OP_CONFIG = 9,
    SERIAL_OP_INITIALIZE_PATCH = 10,
    SERIAL_OP_RECEIVE_PATCH_IMAGE = 11,
    SERIAL_OP_PROTOCOL_VER = 12,
    SERIAL_OP_RESPONSE = 16,
} serial_dfu_op_t;

/**@brief   DFU Response value type.
 */
typedef enum
{
    SERIAL_DFU_RESP_VAL_SUCCESS = 1,                                       /**< Success.*/
    SERIAL_DFU_RESP_VAL_INVALID_STATE,                                     /**< Invalid state.*/
    SERIAL_DFU_RESP_VAL_NOT_SUPPORTED,                                     /**< Operation not supported.*/
    SERIAL_DFU_RESP_VAL_DATA_SIZE,                                         /**< Data size exceeds limit.*/
    SERIAL_DFU_RESP_VAL_CRC_ERROR,                                         /**< CRC Error.*/
    SERIAL_DFU_RESP_VAL_OPER_FAILED,                                        /**< Operation failed.*/
    SERIAL_DFU_RESP_VAL_OK_MORE_DATA_EXP,
} serial_dfu_resp_val_t;


uint32_t dfu_transport_update_start_serial(void);
uint32_t dfu_transport_close_serial(void);

#endif
