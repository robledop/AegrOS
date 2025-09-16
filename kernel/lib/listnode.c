#include <kernel_heap.h>
#include <listnode.h>

list_node_t *list_node_new(void *payload)
{
    auto list_node = (list_node_t *)kzalloc(sizeof(list_node_t));
    if (!list_node) {
        return list_node;
    }

    list_node->prev    = nullptr;
    list_node->next    = nullptr;
    list_node->payload = payload;

    return list_node;
}
