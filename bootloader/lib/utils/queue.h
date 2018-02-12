#ifndef QUEUE_H
#define QUEUE_H

/* Simple FIFO queue that supports put and get operations.
   The plain version of each function will disable/reenable interrupts;
   the versions prefixed by _ leave interrupts alone. */

#include <stdint.h>

struct queue {
	volatile int len;
	volatile uint8_t *data;
	volatile int start;
	volatile int items;
};

/* Declare a buffer, and a queue that uses that buffer. */
#define DECLARE_QUEUE(name, size)                                       \
    static uint8_t __queuebuf_##name[size];                             \
    struct queue name = { .data = __queuebuf_##name, .len = size }

/* Same as DECLARE_QUEUE, but the queue is static. */
#define DECLARE_STATIC_QUEUE(name, size)                                \
    static uint8_t __queuebuf_##name[size];                             \
    static struct queue name = { .data = __queuebuf_##name, .len = size }

/* Push one byte on the end of the queue.  Returns 1 on success, 0 otherwise. */
int queue_push(struct queue *q, uint8_t v);
int _queue_push(struct queue *q, uint8_t v);

/* Get the next byte from the beginning of the queue, or -1 if it's empty */
int queue_pop(struct queue *q);
int _queue_pop(struct queue *q);

#endif
