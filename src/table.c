#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void table_init(Table* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_free(Table* table)
{
    array_free(Entry, table->entries, table->capacity);
    table_init(table);
}

static Entry* entry_find(Entry* entries, int capacity, ObjString* key)
{
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    while (true)
    {
        Entry* entry = &entries[index];

        if (entry->key == NULL)
        {
            if (value_is_nil(entry->value))
            {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key)
        {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool table_get(Table* table, ObjString* key, Value* out_value)
{
    if (table->count == 0) return false;

    Entry* entry = entry_find(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *out_value = entry->value;
    return true;
}

static void capacity_adjust(Table* table, int capacity)
{
    Entry* entries = mem_alloc(Entry, capacity);

    for (int i = 0; i < capacity; ++i)
    {
        entries[i].key = NULL;
        entries[i].value = value_make_nil();
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; ++i)
    {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = entry_find(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;

        table->count++;
    }

    array_free(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* table, ObjString* key, Value value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = capacity_grow(table->capacity);
        capacity_adjust(table, capacity);
    }

    Entry* entry = entry_find(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && value_is_nil(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool table_delete(Table* table, ObjString* key)
{
    if (table->count == 0) return false;

    // Find the entry.
    Entry* entry = entry_find(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = value_make_bool(true);

    return true;
}

void table_append(Table* from, Table* to)
{
    for (int i = 0; i < from->capacity; ++i)
    {
        Entry* entry = &from->entries[i];

        if (entry->key != NULL) table_set(to, entry->key, entry->value);
    }
}

ObjString* table_find_string(Table* table, const char* chars, int length,
                             uint32_t hash)
{
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    while (true)
    {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL)
        {
            // Stop if we find an empty non-tombstone entry.
            if (value_is_nil(entry->value)) return NULL;
        }
        else if (entry->key->length == length && entry->key->hash == hash &&
                 memcmp(entry->key->chars, chars, length) == 0)
        {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void gc_table_remove_white(Table* table)
{
    for (int i = 0; i < table->capacity; ++i)
    {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked)
            table_delete(table, entry->key);
    }
}

void gc_mark_table(Table* table)
{
    for (int i = 0; i < table->capacity; ++i)
    {
        Entry* entry = &table->entries[i];
        gc_mark_obj((Obj*)entry->key);
        gc_mark_value(entry->value);
    }
}
