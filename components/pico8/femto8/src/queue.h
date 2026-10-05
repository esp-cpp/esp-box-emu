#ifndef QUEUE_H
#define QUEUE_H

#include <inttypes.h>
#include <stdbool.h>

typedef struct
{
    void *data_buf;
    void *front;
    void *back;
    const uint16_t elements_num_max;
    const uint16_t elements_size;
    uint16_t elements;
} queue_t;

void _queue_init(queue_t *queue);
bool queue_is_full(queue_t *queue);
bool queue_is_empty(queue_t *queue);
bool queue_add_front(queue_t *queue, void *data);
bool queue_add_back(queue_t *queue, void *data);
bool queue_get_front(queue_t *queue, void *data);
bool queue_get_back(queue_t *queue, void *data);

#endif
