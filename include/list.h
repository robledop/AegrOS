#pragma once
#include <listnode.h>

typedef struct list {
    unsigned int count;
    list_node_t *root_node;
} list_t;

list_t *list_new();
int list_add(list_t *list, void *payload);
void *list_get_at(list_t *list, unsigned int index);
void *list_remove_at(list_t *list, unsigned int index);
