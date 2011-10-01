/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_hash.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#ifndef LISO_HASH_H
#define LISO_HASH_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#undef get16bits
#if (defined(__GNUC__) && defined (__i386__)) || defined(__WATCOMC__) \
    || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined(get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8) \
        +(uint32_t)(((const uint8_t *)(d))[0]))
#endif

#define HASHSIZE        109
#define NUMHEADERS      64
#define MAXHEADERLEN    64
#define MAXVALUELEN     256

typedef struct pair
{
    char key[MAXHEADERLEN];
    char value[MAXVALUELEN];
    struct pair *next; /* need to check on this */
} pair;

typedef struct liso_hash
{
    pair *hash[HASHSIZE];
    char keys[NUMHEADERS][MAXHEADERLEN];
    int num_headers;
} liso_hash;

liso_hash headers_hash;

void init_hash(liso_hash *h);
int contains_key(liso_hash *h, char *key);
int contains_value(liso_hash *h, char *value);
char *get_value(liso_hash *h, char *key);
int hash_add(liso_hash *h, char *key, char*value);
int hash_remove(liso_hash *h, char *key);
int collapse(liso_hash *h);
void printPairs(liso_hash *h);

#endif
