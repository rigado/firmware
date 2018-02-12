#include <nrf.h>
#include "queue.h"

/* Push one byte on the end of the queue.  Returns 1 on success, 0 otherwise. */
int queue_push(struct queue *q, uint8_t v)
{
    __disable_irq();
    int ret = _queue_push(q, v);
    __enable_irq();
    return ret;
}

/* Push one byte on the end of the queue.  Returns 1 on success, 0 otherwise. */
int _queue_push(struct queue *q, uint8_t v)
{
    if (q->items >= q->len)
        return 0;
    q->data[(q->start + q->items) % q->len] = v;
    q->items++;
    return 1;
}

/* Get the next byte from the beginning of the queue, or -1 if it's empty */
int queue_pop(struct queue *q)
{
    __disable_irq();
    int ret = _queue_pop(q);
    __enable_irq();
    return ret;
}

/* Get the next byte from the beginning of the queue, or -1 if it's empty */
int _queue_pop(struct queue *q)
{
    if (q->items <= 0)
        return -1;
    int ret = q->data[q->start];
    q->start = (q->start + 1) % q->len;
    q->items--;
    return ret;
}
