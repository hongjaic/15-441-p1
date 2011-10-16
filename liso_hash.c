/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_hash.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "liso_hash.h"

void free_header_link(pair *ptr);
void remove_key_from_keyset(liso_hash *h, char *key);
int link_contains_value(pair *ptr, char *value);
int link_contains_key(pair *ptr, char *key);
uint32_t super_fast_hash(const char *data, int len);
void string_tolower(char *str);

void init_hash(liso_hash *h)
{
    int i;
    for (i = 0; i < HASHSIZE; i++)
    {
        (h->hash)[i] = NULL;
    }

    memset(h->keys, 0, NUMHEADERS*MAXHEADERLEN);
    h->num_headers = 0;
}

int contains_key(liso_hash *h, char *key)
{
    int hash_index;

    if (h->num_headers == 0)
        return 0;

    hash_index = super_fast_hash(key, (int)strlen(key))%HASHSIZE;

    if ((h->hash)[hash_index] == NULL)
        return 0;


    return 1;
}

int contains_value(liso_hash *h, char *value)
{
    int i;
    
    if (h->num_headers == 0)
        return 0;

    for (i = 0; i < HASHSIZE; i++)
    {
        if(link_contains_value((h->hash)[i], value))
        {
            return 1;
        }
    }

    return 0;
}

char *get_value(liso_hash *h, char *key)
{
    int hash_index;
    pair *curr;

    if ((h->num_headers) == 0)
        return NULL;

    hash_index = super_fast_hash(key, (int)strlen(key))%HASHSIZE;

    curr = (h->hash)[hash_index];

    while (curr != NULL)
    {
        if (strcmp(key, curr->key) == 0)
        {
            return curr->value;
        }
        
        curr = curr->next;
    }

    return NULL;
}

int hash_add(liso_hash *h, char *key, char *value)
{
    int hash_index = -1;
    pair *dom = NULL;

    string_tolower(key);

    hash_index = super_fast_hash(key, (int)strlen(key))%HASHSIZE;

    pair *new = malloc(sizeof(pair));
    strcpy(new->key, key);
    strcpy(new->value, value);
    new->next = NULL;


    if ((dom = (h->hash)[hash_index]) == NULL)
    {

        (h->hash)[hash_index] = new;
    } 
    else 
    {
        new->next = dom;
        (h->hash)[hash_index] = new;
    }

    strcpy((h->keys)[h->num_headers], key);
    h->num_headers++;

    return 1;
}

int hash_remove(liso_hash *h, char *key)
{
    int hash_index = super_fast_hash(key, (int)strlen(key))%HASHSIZE;
    pair *iter = NULL;
    pair *prev = NULL;

    if (h->num_headers == 0)
        return 0;

    if ((iter = (h->hash)[hash_index]) == NULL)
    {
        return 0;
    }

    while (iter != NULL)
    {
        if (strcmp(key, iter->key) == 0)
        {
            if (prev == NULL)
            {
                (h->hash)[hash_index] = iter->next;
                free(iter);
            }
            else
            {
                prev->next = iter->next;
                free(iter);
            }

            h->num_headers--;
            remove_key_from_keyset(h, key);

            return 1;
        }
        prev = iter;
        iter = iter->next;
    }
        
    return 0;
}

int collapse(liso_hash *h)
{
    if (h->num_headers == 0)
        return 0;

    int i;
    for (i = 0; i < HASHSIZE; i++)
    {
        free_header_link((h->hash)[i]);
    }

    init_hash(h);
    return 1;
}

void printPairs(liso_hash *h)
{
    int i;

    if (h == NULL)
    {
        return;
    }

    if (h->num_headers == 0)
    {
        return;
    }

    for (i = 0; i < h->num_headers; i++)
    {
        printf("%s: %s\n", (h->keys)[i], get_value(h, (h->keys)[i]));
    }
}

void free_header_link(pair *ptr)
{
    pair *prev = NULL;
    pair *curr = ptr;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;
        free(prev);
    }
}

int link_contains_value(pair *ptr, char *value)
{
    pair *curr = ptr;

    while (curr  != NULL)
    {
        if (strcmp(value, curr->value) == 0)
        {
            return 1;
        }

        curr = curr->next;
    }

    return 0;
}

int link_contains_key(pair *ptr, char *key)
{
    pair *curr = ptr;

    while (curr != NULL)
    {
        if (strcmp(key, curr->key) == 0)
        {
            return 1;
        }
        curr = curr->next;
    }

    return 0;
}

void remove_key_from_keyset(liso_hash *h, char *key)
{
    int i;
    for (i = 0; i < HASHSIZE; i++)
    {
        if (strcmp(key, (h->keys)[i]) == 0)
        {
            memset((h->keys)[i], 0, MAXHEADERLEN);
        }
    }
}

uint32_t super_fast_hash(const char * data, int len)
{
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;
    
    for (; len > 0; len--)
    {
        hash += get16bits(data);
        tmp = (get16bits(data+2) << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        data += 2*sizeof(uint16_t);
        hash += hash >> 11;
    }

    switch (rem)
    {
        case 3: hash += get16bits(data);
                hash ^= hash << 16;
                hash ^= data[sizeof(uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits(data);
                hash ^= hash << 11;
                hash += hash << 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 11;
    }

    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 16;

    return hash;
}

void string_tolower(char *str)
{
    int i;
    int len;

    if (str == NULL)
        return;

    len = strlen(str);

    for (i = 0; i < len; i++)
    {
        *(str + i) = tolower(*(str + i));
    }
}
