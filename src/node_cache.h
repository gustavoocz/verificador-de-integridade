/*
 * node_cache.h — Interface do cache LRU de nós da árvore de hash (SO)
 *
 * Cada nó verificado é armazenado no cache para evitar recomputação.
 * Política de evicção: LRU (Least Recently Used).
 * Implementação prevista: hash table (open addressing) + lista encadeada LRU.
 *
 * A ser implementada em node_cache.c (E3).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#ifndef NODE_CACHE_H
#define NODE_CACHE_H

#include "sha256.h"
#include <stddef.h>
#include <stdint.h>

typedef struct node_cache_entry {
    uint64_t node_idx;
    uint8_t  hash[SHA256_DIGEST_SIZE];
    struct node_cache_entry *lru_prev;
    struct node_cache_entry *lru_next;
} node_cache_entry_t;

typedef struct {
    size_t              capacity;
    size_t              size;
    node_cache_entry_t *table;
    size_t              table_sz;
    node_cache_entry_t *lru_head;
    node_cache_entry_t *lru_tail;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
} node_cache_t;

int    node_cache_init(node_cache_t *cache, size_t capacity);
void   node_cache_destroy(node_cache_t *cache);

/* Retorna 1 (hit) ou 0 (miss); copia hash para `out` se não-NULL */
int    node_cache_get(node_cache_t *cache, uint64_t node_idx,
                      uint8_t out[SHA256_DIGEST_SIZE]);

void   node_cache_put(node_cache_t *cache, uint64_t node_idx,
                      const uint8_t hash[SHA256_DIGEST_SIZE]);

void   node_cache_invalidate(node_cache_t *cache, uint64_t node_idx);
void   node_cache_clear(node_cache_t *cache);
void   node_cache_print_stats(const node_cache_t *cache);
double node_cache_hit_ratio(const node_cache_t *cache);

#endif /* NODE_CACHE_H */
