/*
 * node_cache.c — Cache LRU de nós da árvore de hash (componente de SO)
 *
 * Estrutura de dados:
 *   - Hash table com endereçamento aberto (linear probing) para lookup O(1)
 *   - Lista duplamente encadeada para política LRU:
 *       head = MRU (mais recentemente usado)
 *       tail = LRU (candidato à evicção)
 *   - Slots marcados com sentinels:
 *       SLOT_EMPTY   (UINT64_MAX)   — nunca usado
 *       SLOT_DELETED (UINT64_MAX-1) — tombstone pós-evicção
 *   - Fator de carga ≤ 0.5 (table_sz = 2 * capacity + 1)
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "node_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Sentinels ────────────────────────────────────────────────────────────── */
#define SLOT_EMPTY   (UINT64_MAX)
#define SLOT_DELETED (UINT64_MAX - 1ULL)

/* ── Hash function ────────────────────────────────────────────────────────── */
/* Fibonacci hashing — distribui bem índices sequenciais */
static size_t slot_hash(uint64_t key, size_t table_sz) {
    uint64_t h = key * 11400714819323198485ULL;
    return (size_t)(h % (uint64_t)table_sz);
}

/* ── LRU: operações na lista duplamente encadeada ─────────────────────────── */

static void lru_push_head(node_cache_t *c, node_cache_entry_t *e) {
    e->lru_prev = NULL;
    e->lru_next = c->lru_head;
    if (c->lru_head) c->lru_head->lru_prev = e;
    c->lru_head = e;
    if (!c->lru_tail) c->lru_tail = e;
}

static void lru_remove(node_cache_t *c, node_cache_entry_t *e) {
    if (e->lru_prev) e->lru_prev->lru_next = e->lru_next;
    else             c->lru_head            = e->lru_next;
    if (e->lru_next) e->lru_next->lru_prev = e->lru_prev;
    else             c->lru_tail            = e->lru_prev;
    e->lru_prev = NULL;
    e->lru_next = NULL;
}

static void lru_move_to_head(node_cache_t *c, node_cache_entry_t *e) {
    if (c->lru_head == e) return; /* já é MRU */
    lru_remove(c, e);
    lru_push_head(c, e);
}

/* ── Hash table: busca e inserção ─────────────────────────────────────────── */

/* Busca a entrada com node_idx; NULL se não encontrada */
static node_cache_entry_t *find_entry(node_cache_t *c, uint64_t node_idx) {
    size_t start = slot_hash(node_idx, c->table_sz);
    size_t i = start;
    do {
        node_cache_entry_t *e = &c->table[i];
        if (e->node_idx == SLOT_EMPTY)   return NULL; /* fim da cadeia */
        if (e->node_idx == node_idx)     return e;    /* encontrado    */
        /* SLOT_DELETED: continua a busca (tombstone) */
        i = (i + 1) % c->table_sz;
    } while (i != start);
    return NULL;
}

/* Retorna o primeiro slot livre (EMPTY ou DELETED) para inserção */
static node_cache_entry_t *find_insert_slot(node_cache_t *c, uint64_t node_idx) {
    size_t start = slot_hash(node_idx, c->table_sz);
    size_t i = start;
    node_cache_entry_t *first_deleted = NULL;
    do {
        node_cache_entry_t *e = &c->table[i];
        if (e->node_idx == SLOT_EMPTY)
            return first_deleted ? first_deleted : e;
        if (e->node_idx == SLOT_DELETED && !first_deleted)
            first_deleted = e;
        i = (i + 1) % c->table_sz;
    } while (i != start);
    return first_deleted; /* só tombstones disponíveis */
}

/* ── Evicção ──────────────────────────────────────────────────────────────── */

static void evict_lru(node_cache_t *c) {
    node_cache_entry_t *victim = c->lru_tail;
    if (!victim) return;
    lru_remove(c, victim);
    victim->node_idx = SLOT_DELETED; /* tombstone */
    c->size--;
    c->evictions++;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

int node_cache_init(node_cache_t *cache, size_t capacity) {
    if (!cache || capacity == 0) return -1;

    size_t table_sz = capacity * 2 + 1; /* fator de carga ≤ 0.5 */

    cache->capacity  = capacity;
    cache->size      = 0;
    cache->table_sz  = table_sz;
    cache->lru_head  = NULL;
    cache->lru_tail  = NULL;
    cache->hits      = 0;
    cache->misses    = 0;
    cache->evictions = 0;

    cache->table = (node_cache_entry_t *)malloc(
                       table_sz * sizeof(node_cache_entry_t));
    if (!cache->table) return -1;

    for (size_t i = 0; i < table_sz; i++) {
        cache->table[i].node_idx = SLOT_EMPTY;
        cache->table[i].lru_prev = NULL;
        cache->table[i].lru_next = NULL;
    }
    return 0;
}

void node_cache_destroy(node_cache_t *cache) {
    if (!cache) return;
    free(cache->table);
    memset(cache, 0, sizeof(*cache));
}

int node_cache_get(node_cache_t *cache, uint64_t node_idx,
                   uint8_t out[SHA256_DIGEST_SIZE]) {
    if (!cache || !cache->table) return 0;

    node_cache_entry_t *e = find_entry(cache, node_idx);
    if (!e) {
        cache->misses++;
        return 0; /* miss */
    }

    cache->hits++;
    lru_move_to_head(cache, e);
    if (out) memcpy(out, e->hash, SHA256_DIGEST_SIZE);
    return 1; /* hit */
}

void node_cache_put(node_cache_t *cache, uint64_t node_idx,
                    const uint8_t hash[SHA256_DIGEST_SIZE]) {
    if (!cache || !cache->table || !hash) return;

    /* Já existe? Atualizar hash e promover ao topo */
    node_cache_entry_t *e = find_entry(cache, node_idx);
    if (e) {
        memcpy(e->hash, hash, SHA256_DIGEST_SIZE);
        lru_move_to_head(cache, e);
        return;
    }

    /* Cache cheio: despejar LRU antes de inserir */
    if (cache->size >= cache->capacity)
        evict_lru(cache);

    /* Inserir em slot livre */
    e = find_insert_slot(cache, node_idx);
    if (!e) return; /* nunca deve acontecer com fator de carga ≤ 0.5 */

    e->node_idx = node_idx;
    memcpy(e->hash, hash, SHA256_DIGEST_SIZE);
    e->lru_prev = NULL;
    e->lru_next = NULL;

    lru_push_head(cache, e);
    cache->size++;
}

void node_cache_invalidate(node_cache_t *cache, uint64_t node_idx) {
    if (!cache || !cache->table) return;

    node_cache_entry_t *e = find_entry(cache, node_idx);
    if (!e) return;

    lru_remove(cache, e);
    e->node_idx = SLOT_DELETED;
    cache->size--;
}

void node_cache_clear(node_cache_t *cache) {
    if (!cache || !cache->table) return;

    for (size_t i = 0; i < cache->table_sz; i++) {
        cache->table[i].node_idx = SLOT_EMPTY;
        cache->table[i].lru_prev = NULL;
        cache->table[i].lru_next = NULL;
    }
    cache->lru_head  = NULL;
    cache->lru_tail  = NULL;
    cache->size      = 0;
    cache->hits      = 0;
    cache->misses    = 0;
    cache->evictions = 0;
}

void node_cache_print_stats(const node_cache_t *cache) {
    if (!cache) return;
    printf("=== Cache LRU de Nos ===\n");
    printf("Capacidade : %lu\n",  (unsigned long)cache->capacity);
    printf("Ocupacao   : %lu\n",  (unsigned long)cache->size);
    printf("Hits       : %lu\n",  (unsigned long)cache->hits);
    printf("Misses     : %lu\n",  (unsigned long)cache->misses);
    printf("Evictions  : %lu\n",  (unsigned long)cache->evictions);
    printf("Hit ratio  : %.1f%%\n", node_cache_hit_ratio(cache) * 100.0);
}

double node_cache_hit_ratio(const node_cache_t *cache) {
    if (!cache) return 0.0;
    uint64_t total = cache->hits + cache->misses;
    if (total == 0) return 0.0;
    return (double)cache->hits / (double)total;
}
