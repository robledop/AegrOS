#pragma once

typedef struct list_node {
    void *payload;
    struct list_node *prev;
    struct list_node *next;
} list_node_t;

list_node_t *list_node_new(void *payload);