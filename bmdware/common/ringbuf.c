/** @file ringbuf.c
*
* @brief This module provides a ring buffer implementation
*
* @par
* COPYRIGHT NOTICE: (c) Rigado
*
* @description
* @par  Ringbuffer Overview
*       Always keep one slot open; don't queue or read if it will overflow
*       If read == write buffer is empty
*       If (write+1) == read buffer is full
*
* All rights reserved. */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "ringbuf.h"

#define ALMOST_FULL_THRESHOLD_PERCENT   (80)    

/* Helper functions */
static bool update_callback(ringBufEventCallback_t * list, 
    ringBufEventCallback_t callback, bool set);
static void incrementIdx(uint32_t bufSize, uint32_t* idx);
static void executeCallbacks(ringBuf_t* ringBuf, ringBufEvent_t event);

uint8_t ringBufInit(ringBuf_t* ringBuf, uint32_t elementSize, uint32_t elementCount, void* elementBuffer )
{
    if( ringBuf
        && elementBuffer
        && elementSize
        && elementCount )
    {
        memset(ringBuf, 0, sizeof(ringBuf_t));
        ringBuf->elementSize = elementSize;
        ringBuf->elementCount = elementCount;
        ringBuf->buffer = elementBuffer;

        ringBuf->writeIdx = 0;
        ringBuf->readIdx = 0;
        
        ringBuf->almostFullThreshold = 
            (uint32_t)((elementCount * ALMOST_FULL_THRESHOLD_PERCENT) / 100);

        return RINGBUF_SUCCESS;
    }
    return RINGBUF_ERROR;
}

uint8_t ringBufClear(ringBuf_t* ringBuf)
{
    if( ringBuf == NULL )
    {
        return RINGBUF_ERROR;
    }
    
    ringBuf->writeIdx = 0;
    ringBuf->readIdx = 0;
    
    executeCallbacks(ringBuf, RINGBUF_EVENT_EMPTY);
    
    return RINGBUF_SUCCESS;
}

/* status */
uint32_t ringBufTotalCapacity(ringBuf_t* ringBuf)
{
  return(ringBuf->elementCount);
}

uint32_t ringBufWaiting(ringBuf_t* ringBuf)
{
    uint32_t writeIdx;
    uint32_t readIdx;

    writeIdx = ringBuf->writeIdx;
    readIdx = ringBuf->readIdx;

    if(writeIdx >= readIdx)
    {
        return (writeIdx - readIdx);
    }
    else
    {
        return (ringBuf->elementCount - (readIdx-writeIdx));
    }
}

uint32_t ringBufUnused(ringBuf_t* ringBuf)
{
    /* leave one slot open so we can detect empty vs full */
    uint32_t free = (ringBuf->elementCount-1) - ringBufWaiting(ringBuf);
    return(free);
}



/* 1 byte */
uint8_t ringBufReadOne(ringBuf_t* ringBuf, void* elementOut)
{
    if( ringBuf == NULL
        || elementOut == NULL
        || ringBufWaiting(ringBuf) == 0
        )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t readOffset = ringBuf->readIdx * ringBuf->elementSize;
        memcpy(elementOut, &ringBuf->buffer[readOffset], ringBuf->elementSize);

        incrementIdx(ringBuf->elementCount, &(ringBuf->readIdx));
        if(ringBufWaiting(ringBuf) == 0)
        {
            executeCallbacks(ringBuf, RINGBUF_EVENT_EMPTY);
        }
        return RINGBUF_SUCCESS;
    }
}

uint8_t ringBufPeekOne(ringBuf_t* ringBuf, void* elementOut)
{
    if( ringBuf == NULL
        || elementOut == NULL
        || ringBufWaiting(ringBuf) == 0 )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t readOffset = ringBuf->readIdx * ringBuf->elementSize;
        memcpy(elementOut, &ringBuf->buffer[readOffset], ringBuf->elementSize);

        return RINGBUF_SUCCESS;
    }
}

uint8_t ringBufWriteOne(ringBuf_t* ringBuf, void* elementIn)
{
    if( ringBuf == NULL
        || elementIn == NULL
        || ringBufUnused(ringBuf) == 0 )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t writeOffset = ringBuf->writeIdx * ringBuf->elementSize;
        memcpy(&ringBuf->buffer[writeOffset],elementIn,ringBuf->elementSize);

        incrementIdx(ringBuf->elementCount, &(ringBuf->writeIdx));
        
        uint32_t waiting = ringBufWaiting(ringBuf);
        if(waiting == ringBuf->elementCount)
        {
            executeCallbacks(ringBuf, RINGBUF_EVENT_FULL);
        }
        else if(waiting > ringBuf->almostFullThreshold)
        {
            executeCallbacks(ringBuf, RINGBUF_EVENT_ALMOST_FULL);
        }
        
        return RINGBUF_SUCCESS;
    }
}

/* n-bytes */
uint8_t ringBufRead(ringBuf_t* ringBuf, void* elementsOut, uint32_t elementCount)
{
    if( ringBuf == NULL
        || elementsOut == NULL
        || elementCount == 0
        || ringBufWaiting(ringBuf) < elementCount )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t idx;
        uint8_t* ptr = elementsOut;

        for(idx = 0; idx<elementCount; idx++)
        {
            ringBufReadOne(ringBuf, ptr);
            ptr += ringBuf->elementSize;
        }
        return RINGBUF_SUCCESS;
    }
}

uint8_t ringBufPeek(ringBuf_t* ringBuf, void* elementsOut, uint32_t elementCount)
{
    if( ringBuf == NULL
        || elementsOut == NULL
        || elementCount == 0
        || ringBufWaiting(ringBuf) < elementCount )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t readOffset     = ringBuf->readIdx * ringBuf->elementSize;
        uint32_t writeOffset    = 0;
        uint8_t* p_data_out     = (uint8_t*)elementsOut;
        
        for(uint32_t i=0; i<elementCount; i++)
        {
            memcpy(&p_data_out[writeOffset], &ringBuf->buffer[readOffset], ringBuf->elementSize);
            
            writeOffset += ringBuf->elementSize;
            readOffset  += ringBuf->elementSize;
            
            //handle wrap
            if(readOffset >= (ringBuf->elementSize*ringBuf->elementCount))
            {
                readOffset = 0;
            }
        }
        
        return RINGBUF_SUCCESS;
    }
}


uint8_t ringBufWrite(ringBuf_t* ringBuf, void* elementsIn, uint32_t elementCount)
{
    if( ringBuf == NULL
        || elementsIn == NULL
        || elementCount == 0
        || ringBufUnused(ringBuf) < elementCount )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        uint32_t idx;
        uint8_t* ptr = elementsIn;

        for(idx = 0; idx<elementCount; idx++)
        {
            ringBufWriteOne(ringBuf, ptr);
            ptr += ringBuf->elementSize;
        }
        return RINGBUF_SUCCESS;
    }
}


/* discard */
uint8_t ringBufDiscard(ringBuf_t* ringBuf, uint32_t elementCount)
{
    if( ringBuf == NULL
        || elementCount == 0
        || ringBufUnused(ringBuf) < elementCount )
    {
        return RINGBUF_ERROR;
    }
    else
    {
        // just move the index
        for(uint32_t i=0; i<elementCount; i++)
        {
            incrementIdx(ringBuf->elementCount, &(ringBuf->readIdx));
        }
        
        if(ringBufWaiting(ringBuf) == 0)
        {
            executeCallbacks(ringBuf, RINGBUF_EVENT_EMPTY);
        }
        
        return RINGBUF_SUCCESS;
    }
}


/* events */
uint32_t ringBufRegisterEventCallback(ringBuf_t* ringBuf, 
    ringBufEvent_t event, ringBufEventCallback_t callback)
{
    if(ringBuf == NULL || event >= RINGBUF_EVENT_COUNT || callback == NULL)
    {
        return RINGBUF_ERROR;
    }
    
    ringBufEventCallback_t * cb_list = ringBuf->event_list[event];
    bool result = update_callback(cb_list, callback, true);
    
    return (result) ? RINGBUF_SUCCESS : RINGBUF_ERROR;
}

uint32_t ringBufUnregisterEventCallback(ringBuf_t* ringBuf, 
    ringBufEvent_t event, ringBufEventCallback_t callback)
{
    if(ringBuf == NULL || event >= RINGBUF_EVENT_COUNT || callback == NULL)
    {
        return RINGBUF_ERROR;
    }
    
    ringBufEventCallback_t * cb_list = ringBuf->event_list[event];
    bool result = update_callback(cb_list, callback, false);
    
    return (result) ? RINGBUF_SUCCESS : RINGBUF_ERROR;
}

/* helper functions */
static void executeCallbacks(ringBuf_t* ringBuf, ringBufEvent_t event)
{
    for(uint8_t i = 0; i < RINGBUF_MAX_CALLBACKS_PER_EVENT; i++)
    {
        ringBufEventCallback_t callback = ringBuf->event_list[event][i];
        if(callback != NULL)
        {
            callback(ringBuf, event);
        }
    }
}

static void incrementIdx(uint32_t bufSize, uint32_t* idx)
{
    *idx = (*idx) + 1;

    /* do we need to wrap? */
    if( *idx == bufSize )
    {
        *idx = 0;
    }
}

static bool update_callback(ringBufEventCallback_t * list, 
    ringBufEventCallback_t callback, bool set)
{
    ringBufEventCallback_t *end = &list[RINGBUF_MAX_CALLBACKS_PER_EVENT];
    ringBufEventCallback_t *cb = &list[0];
    
    while(cb != end)
    {
        if(set && *cb == NULL)
        {
            *cb = callback;
            return true;
        }
        else if(!set && *cb == callback)
        {
            *cb = NULL;
            return true;
        }
        cb++;
    }
    return false;
}


#if 0
uint32_t ringBufSelfTest(void)
{
  uint32_t errors = 0;
  ringBuf_t r;
  uint8_t rbuf[64];

  if( ringBufInit(&r, rbuf, sizeof(rbuf)) == RINGBUF_SUCCESS )
  {
    uint8_t buf1[64];
    uint8_t buf2[128];
    int i;

    // 1 slot always free
    if( ringBufBytesTotalSize(&r) != sizeof(rbuf) )
      errors++;

    // buffer is empty
    if( ringBufBytesWaiting(&r) != 0 )
      errors++;

    if( ringBufRead( &r, buf2, 2 ) != RINGBUF_ERROR )
      errors++;

    if( ringBufReadOne( &r, buf2 ) != RINGBUF_ERROR )
      errors++;

    if( ringBufPeekOne( &r, buf2 ) != RINGBUF_ERROR )
      errors++;

    // queue 3 bytes
    for(i=0;i<sizeof(buf1); i++)
    {
      buf1[i] = i;
    }

    if( ringBufWrite(&r, buf1, 63) == RINGBUF_ERROR )
      errors++;

    /* 3 bytes waiting */
    if( ringBufBytesWaiting(&r) != 63)
      errors++;

    /* peek */
    if( ringBufPeekOne(&r, &buf2[0]) == RINGBUF_ERROR || buf2[0] != 0 )
      errors++;

    /* read/verify */
    if( ringBufRead(&r, buf2, 63) == RINGBUF_ERROR || memcmp(buf1, buf2, 63) != 0 )
        errors++;

    for( i=0; i<32; i++)
    {
      memset(buf2, 0xff, sizeof(buf2));

      /* write 60 bytes */
      if( ringBufWrite(&r, buf1, 60) == RINGBUF_ERROR )
        errors++;

      /* check we queued */
      if( ringBufBytesWaiting(&r) != 60)
        errors++;

      /* peek one */
      if( ringBufPeekOne(&r, &buf2[0]) == RINGBUF_ERROR || buf2[0] != 0 )
        errors++;

      /* ensure peek didnt increment */
      if( ringBufBytesWaiting(&r) != 60)
        errors++;

      /* try to read more than is available */
      if( ringBufRead(&r, &buf2[1], 61) != RINGBUF_ERROR )
        errors++;

      /* read 60 bytes */
      /* read one */
      if( ringBufReadOne(&r, &buf2[0]) == RINGBUF_ERROR || buf2[0] != 0 )
        errors++;

      /* read 59 */
      if( ringBufRead(&r, &buf2[1], 59) == RINGBUF_ERROR || memcmp(buf1, buf2, 59) != 0 )
        errors++;

      /* write overflow */
      if( ringBufWrite(&r, buf1, 64) != RINGBUF_ERROR )
        errors++;
    }
  }
  else
  {
    //init error
    errors++;
  }


  return errors;
}

#endif
