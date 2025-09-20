// Simple hash table implemented in C.
#include <hash_table.h>
#include <kernel_heap.h>
#include <stdint.h>
#include <string.h>

#include "assert.h"

// Hash table entry (slot may be filled or empty).
typedef struct {
    int key; // key is nullptr if this slot is empty
    void *value;
} ht_entry;

// Hash table structure: create with ht_create, kfree with ht_destroy.
struct ht {
    ht_entry *entries; // hash slots
    size_t capacity;   // size of _entries array
    size_t length;     // number of items in hash table
};

#define INITIAL_CAPACITY 16 // must not be zero

hash_table_t *ht_create(void)
{
    // Allocate space for hash table struct.
    hash_table_t *table = kmalloc(sizeof(hash_table_t));
    if (table == nullptr) {
        return nullptr;
    }
    table->length   = 0;
    table->capacity = INITIAL_CAPACITY;

    // Allocate (zero'd) space for entry buckets.
    table->entries = kzalloc(table->capacity * sizeof(ht_entry));
    if (table->entries == nullptr) {
        kfree(table); // error, kfree table before we return!
        return nullptr;
    }
    return table;
}

void ht_destroy(hash_table_t *table)
{
    // First kfree allocated keys.
    // for (size_t i = 0; i < table->capacity; i++) {
    //     if (table->entries[i].key) {
    //         kfree((void *)table->entries[i].key);
    //     }
    // }

    // Then kfree entries array and table itself.
    kfree(table->entries);
    kfree(table);
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static uint64_t hash_key(const char *key)
{
    uint64_t hash = FNV_OFFSET;
    for (const char *p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

void *ht_get(hash_table_t *table, int key)
{
    // AND hash with capacity-1 to ensure it's within entries array.
    // uint64_t hash = hash_key(key);
    uint64_t hash = key;
    size_t index  = (size_t)(hash & (uint64_t)(table->capacity - 1));

    // Loop till we find an empty entry.
    while (table->entries[index].key != 0) {
        if (key == table->entries[index].key) {
            // Found key, return value.
            return table->entries[index].value;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= table->capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }
    return nullptr;
}

// Internal function to set an entry (without expanding table).
static int ht_set_entry(ht_entry *entries, size_t capacity, int key, void *value, size_t *plength)
{
    // AND hash with capacity-1 to ensure it's within entries array.
    // uint64_t hash = hash_key(key);
    uint64_t hash = key;
    size_t index  = (size_t)(hash & (uint64_t)(capacity - 1));

    // Loop till we find an empty entry.
    while (entries[index].key != 0) {
        // if (strcmp(key, entries[index].key) == 0) {
        if (key == entries[index].key) {
            // Found key (it already exists), update value.
            entries[index].value = value;
            return entries[index].key;
        }
        // Key wasn't in this slot, move to next (linear probing).
        index++;
        if (index >= capacity) {
            // At end of entries array, wrap around.
            index = 0;
        }
    }

    // Didn't find key, allocate+copy if needed, then insert it.
    if (plength != nullptr) {
        // key = strdup(key);
        if (key == 0) {
            return 0;
        }
        (*plength)++;
    }
    entries[index].key   = key;
    entries[index].value = value;
    return key;
}

// Expand hash table to twice its current size. Return true on success,
// false if out of memory.
static bool ht_expand(hash_table_t *table)
{
    // Allocate new entries array.
    size_t new_capacity = table->capacity * 2;
    if (new_capacity < table->capacity) {
        return false; // overflow (capacity would be too big)
    }
    ht_entry *new_entries = kzalloc(new_capacity * sizeof(ht_entry));
    if (new_entries == nullptr) {
        return false;
    }

    // Iterate entries, move all non-empty ones to new table's entries.
    for (size_t i = 0; i < table->capacity; i++) {
        ht_entry entry = table->entries[i];
        if (entry.key != 0) {
            ht_set_entry(new_entries, new_capacity, entry.key, entry.value, nullptr);
        }
    }

    // Free old entries array and update this table's details.
    kfree(table->entries);
    table->entries  = new_entries;
    table->capacity = new_capacity;
    return true;
}

int ht_set(hash_table_t *table, int key, void *value)
{
    ASSERT(value != nullptr);
    if (value == nullptr) {
        return 0;
    }

    // If length will exceed half of current capacity, expand it.
    if (table->length >= table->capacity / 2) {
        if (!ht_expand(table)) {
            return 0;
        }
    }

    // Set entry and update length.
    return ht_set_entry(table->entries, table->capacity, key, value, &table->length);
}

size_t ht_length(hash_table_t *table)
{
    return table->length;
}

hti ht_iterator(hash_table_t *table)
{
    hti it;
    it._table = table;
    it._index = 0;
    return it;
}

bool ht_next(hti *it)
{
    // Loop till we've hit end of entries array.
    hash_table_t *table = it->_table;
    while (it->_index < table->capacity) {
        size_t i = it->_index;
        it->_index++;
        if (table->entries[i].key != 0) {
            // Found next non-empty item, update iterator key and value.
            ht_entry entry = table->entries[i];
            it->key        = entry.key;
            it->value      = entry.value;
            return true;
        }
    }
    return false;
}