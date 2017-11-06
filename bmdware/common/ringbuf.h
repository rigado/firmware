/** @file ringbuf.h
*
* @brief This module provides a ring buffer implementation
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* All rights reserved. */

#ifndef __RINGBUF_H
#define __RINGBUF_H

#include <stdint.h>
#include <string.h>

#define RINGBUF_SUCCESS 1
#define RINGBUF_ERROR   0

typedef enum
{
    RINGBUF_EVENT_ALMOST_FULL,
    RINGBUF_EVENT_FULL,
    RINGBUF_EVENT_EMPTY,
    
    RINGBUF_EVENT_COUNT
} ringBufEvent_t;

#define RINGBUF_MAX_CALLBACKS_PER_EVENT         5

typedef struct ringBuf_s ringBuf_t;
typedef void (*ringBufEventCallback_t)(ringBuf_t * ringBuf, ringBufEvent_t event);

/* ringbuf structure */
struct ringBuf_s
{
    /* element size and count */
    uint32_t elementSize;
    uint32_t elementCount;

    /* pointer to buffer */
    uint8_t* buffer;

    /* read/write index */
    uint32_t writeIdx;
    uint32_t readIdx;
    
    /* almost full count */
    uint32_t almostFullThreshold;
    
    /* event callbacks */
    ringBufEventCallback_t event_list[RINGBUF_EVENT_COUNT][RINGBUF_MAX_CALLBACKS_PER_EVENT];
};

/* init */
uint8_t ringBufInit(ringBuf_t* ringBuf, uint32_t elementSize, uint32_t elementCount, void* elementBuffer);
uint8_t ringBufClear(ringBuf_t* ringBuf);

/* event registration */
uint32_t ringBufRegisterEventCallback(ringBuf_t* ringBuf, 
    ringBufEvent_t event, ringBufEventCallback_t callback);
uint32_t ringBufUnregisterEventCallback(ringBuf_t* ringBuf, 
    ringBufEvent_t event, ringBufEventCallback_t callback);

/* status */
uint32_t ringBufTotalCapacity(ringBuf_t* ringBuf);
uint32_t ringBufWaiting(ringBuf_t* ringBuf);
uint32_t ringBufUnused(ringBuf_t* ringBuf);

/* 1 byte */
uint8_t ringBufReadOne(ringBuf_t* ringBuf, void* elementOut);
uint8_t ringBufPeekOne(ringBuf_t* ringBuf, void* elementOut);
uint8_t ringBufWriteOne(ringBuf_t* ringBuf, void* elementIn);

/* n-bytes */
uint8_t ringBufRead(ringBuf_t* ringBuf, void* elementsOut, uint32_t elementCount);
uint8_t ringBufPeek(ringBuf_t* ringBuf, void* elementsIn, uint32_t elementCount);
uint8_t ringBufWrite(ringBuf_t* ringBuf, void* elementsIn, uint32_t elementCount);
uint8_t ringBufDiscard(ringBuf_t* ringBuf, uint32_t elementCount);

/* self test */
uint32_t ringBufSelfTest(void);

#endif /* __RINGBUF_H */
