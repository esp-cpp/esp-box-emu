/**
 * \file    queue.c
 *
 * \brief   A double-ended queue (deque). Elements can be added or removed from
 *          either the front or the back side.
 * \warning The current implementation is NOT interrupt safe. Make sure interrupts
 *          are disabled before access the QUEUE otherwise the program might yield
 *          unexpected results.
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "queue.h"

#define INCREASE_INDEX(queue, ptr) queue->ptr = (queue->ptr + queue->elements_size) >= (queue->data_buf + queue->elements_num_max * queue->elements_size) ? queue->data_buf : (queue->ptr + queue->elements_size)
#define DECREASE_INDEX(queue, ptr) queue->ptr = (queue->ptr - queue->elements_size) < queue->data_buf ? (queue->data_buf + (queue->elements_num_max - 1) * queue->elements_size) : (queue->ptr - queue->elements_size)

/**
 * Initializes - resets the queue.
 */
void _queue_init(queue_t *queue)
{
    memset((uint8_t *)queue->data_buf, 0, queue->elements_num_max * queue->elements_size);

    queue->back = queue->data_buf;
    queue->front = queue->data_buf;
    queue->elements = 0;
}

/**
 * Checks if queue is full.
 *
 * \returns true if queue is full.
 */
bool queue_is_full(queue_t *queue)
{
    return (queue->elements == queue->elements_num_max);
}

/**
 * Checks if queue is empty
 *
 * \returns true if queue is empty.
 */
bool queue_is_empty(queue_t *queue)
{
    return (queue->elements == 0);
}

/**
 * Adds one element to the front of the queue.
 *
 * \returns false if the queue is full.
 */
bool queue_add_front(queue_t *queue,
                     void *data)
{
    if (queue_is_full(queue))
    {
        return false;
    }

    if (queue_is_empty(queue) == false)
    {
        INCREASE_INDEX(queue, front);
    }

    memcpy((uint8_t *)queue->front, (uint8_t *)data, queue->elements_size);
    queue->elements++;
    return true;
}

/**
 * Adds one element to the back of the queue.
 *
 * \returns false if the queue is full.
 */
bool queue_add_back(queue_t *queue,
                    void *data)
{
    if (queue_is_full(queue))
    {
        return false;
    }

    if (queue_is_empty(queue) == false)
    {
        DECREASE_INDEX(queue, back);
    }

    memcpy((uint8_t *)queue->back, (uint8_t *)data, queue->elements_size);
    queue->elements++;
    return true;
}

/**
 * Reads one element from the front of the queue.
 *
 * \returns false if the queue is empty.
 */
bool queue_get_front(queue_t *queue,
                     void *data)
{
    if (queue_is_empty(queue))
    {
        return false;
    }

    memcpy((uint8_t *)data, (uint8_t *)queue->front, queue->elements_size);
    if (queue->front != queue->back)
    {
        DECREASE_INDEX(queue, front);
    }
    queue->elements--;
    return true;
}

/**
 * Reads one element from the back of the queue.
 *
 * \returns false if the queue is empty.
 */
bool queue_get_back(queue_t *queue,
                    void *data)
{
    if (queue_is_empty(queue))
    {
        return false;
    }

    memcpy((uint8_t *)data, (uint8_t *)queue->back, queue->elements_size);
    if (queue->front != queue->back)
    {
        INCREASE_INDEX(queue, back);
    }
    queue->elements--;
    return true;
}
