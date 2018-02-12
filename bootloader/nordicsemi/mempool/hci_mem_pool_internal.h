#ifndef MEM_POOL_INTERNAL_H__
#define MEM_POOL_INTERNAL_H__

/* HCI transport wants: */
#define TX_BUF_SIZE 32u
#define RX_BUF_SIZE 600u
#define RX_BUF_QUEUE_SIZE 2u

/* BLE transport wants: */
#define BLE_TX_BUF_SIZE 4u
#define BLE_RX_BUF_SIZE 128u
#define BLE_RX_BUF_QUEUE_SIZE 32u

/* Used by main.c to verify that we got the right copy of this file,
   since there are a few scattered around the SDK tree. */
#define MEM_POOL_CORRECT_INCLUDE

#endif
