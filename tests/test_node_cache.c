/*
 * test_node_cache.c — Testes unitários do cache LRU de nós
 *
 * Cobre: init/destroy, put/get (hit e miss), ordem LRU, evicção,
 * atualização de entrada existente, invalidate, clear e estatísticas.
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/node_cache.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ── Mini framework ────────────────────────────────────────────────────────── */
static int tests_run = 0, tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  [FAIL] linha %d: %s\n", __LINE__, (msg)); \
    } else { \
        tests_passed++; \
        printf("  [OK]   %s\n", (msg)); \
    } \
} while (0)

/* Gera hash sintético previsível para o nó n */
static void make_hash(uint64_t n, uint8_t out[SHA256_DIGEST_SIZE]) {
    memset(out, 0, SHA256_DIGEST_SIZE);
    /* Preenche com n repetido em bytes */
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
        out[i] = (uint8_t)(n + i);
}

/* ── Testes ────────────────────────────────────────────────────────────────── */

static void test_init_destroy(void) {
    printf("\n[TEST] init / destroy\n");

    node_cache_t c;
    ASSERT(node_cache_init(&c, 8) == 0,  "init retorna 0");
    ASSERT(c.capacity   == 8,            "capacity == 8");
    ASSERT(c.size       == 0,            "size inicial == 0");
    ASSERT(c.table      != NULL,         "table alocado");
    ASSERT(c.lru_head   == NULL,         "lru_head inicial == NULL");
    ASSERT(c.lru_tail   == NULL,         "lru_tail inicial == NULL");
    ASSERT(c.hits       == 0,            "hits inicial == 0");
    ASSERT(c.misses     == 0,            "misses inicial == 0");
    ASSERT(c.evictions  == 0,            "evictions inicial == 0");

    node_cache_destroy(&c);
    ASSERT(c.table == NULL, "table NULL apos destroy");

    /* init com capacidade 0 deve falhar */
    ASSERT(node_cache_init(NULL, 4) == -1, "init NULL retorna -1");
    ASSERT(node_cache_init(&c, 0)   == -1, "init capacity=0 retorna -1");
}

static void test_put_get_basic(void) {
    printf("\n[TEST] put / get basico\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h0[SHA256_DIGEST_SIZE], h1[SHA256_DIGEST_SIZE];
    uint8_t out[SHA256_DIGEST_SIZE];
    make_hash(10, h0);
    make_hash(20, h1);

    /* Miss antes de inserir */
    ASSERT(node_cache_get(&c, 10, out) == 0, "get antes de put = miss");
    ASSERT(c.misses == 1,                    "misses == 1");

    /* Put e hit */
    node_cache_put(&c, 10, h0);
    ASSERT(node_cache_get(&c, 10, out) == 1,                      "get apos put = hit");
    ASSERT(memcmp(out, h0, SHA256_DIGEST_SIZE) == 0,               "hash correto no hit");
    ASSERT(c.hits == 1,                                            "hits == 1");
    ASSERT(c.size == 1,                                            "size == 1");

    /* Segundo nó */
    node_cache_put(&c, 20, h1);
    ASSERT(node_cache_get(&c, 20, out) == 1,                 "get no2 = hit");
    ASSERT(memcmp(out, h1, SHA256_DIGEST_SIZE) == 0,         "hash no2 correto");
    ASSERT(c.size == 2,                                      "size == 2");

    node_cache_destroy(&c);
}

static void test_lru_order(void) {
    printf("\n[TEST] ordem LRU apos acessos\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h[SHA256_DIGEST_SIZE];

    /* Inserir A(0), B(1), C(2) — head deve ser C (mais recente) */
    make_hash(0, h); node_cache_put(&c, 0, h);
    make_hash(1, h); node_cache_put(&c, 1, h);
    make_hash(2, h); node_cache_put(&c, 2, h);

    /* Acessar A(0) → move para o topo */
    node_cache_get(&c, 0, NULL);

    /* head = 0 (MRU), tail = 1 (LRU) */
    ASSERT(c.lru_head != NULL && c.lru_head->node_idx == 0,
           "MRU = no acessado por ultimo (0)");
    ASSERT(c.lru_tail != NULL && c.lru_tail->node_idx == 1,
           "LRU = no menos recente (1)");

    node_cache_destroy(&c);
}

static void test_eviction(void) {
    printf("\n[TEST] evicção LRU ao ultrapassar capacidade\n");

    node_cache_t c;
    node_cache_init(&c, 3); /* capacidade 3 */

    uint8_t h[SHA256_DIGEST_SIZE], out[SHA256_DIGEST_SIZE];

    /* Inserir nós 10, 11, 12 (cache cheio) */
    make_hash(10, h); node_cache_put(&c, 10, h);
    make_hash(11, h); node_cache_put(&c, 11, h);
    make_hash(12, h); node_cache_put(&c, 12, h);
    ASSERT(c.size == 3,       "size == 3 (cheio)");
    ASSERT(c.evictions == 0,  "sem evicções ainda");

    /* Inserir nó 13 → deve despejar o LRU (10) */
    make_hash(13, h); node_cache_put(&c, 13, h);
    ASSERT(c.size == 3,       "size mantido em 3 apos evicção");
    ASSERT(c.evictions == 1,  "1 evicção");

    /* Nó 10 deve ter sido despejado */
    ASSERT(node_cache_get(&c, 10, out) == 0, "no 10 (LRU) foi despejado");
    /* Nós 11, 12, 13 ainda presentes */
    ASSERT(node_cache_get(&c, 11, out) == 1, "no 11 ainda presente");
    ASSERT(node_cache_get(&c, 12, out) == 1, "no 12 ainda presente");
    ASSERT(node_cache_get(&c, 13, out) == 1, "no 13 presente (novo)");

    node_cache_destroy(&c);
}

static void test_eviction_order(void) {
    printf("\n[TEST] evicção respeita ordem LRU com acessos intercalados\n");

    node_cache_t c;
    node_cache_init(&c, 3);

    uint8_t h[SHA256_DIGEST_SIZE], out[SHA256_DIGEST_SIZE];

    make_hash(1, h); node_cache_put(&c, 1, h);
    make_hash(2, h); node_cache_put(&c, 2, h);
    make_hash(3, h); node_cache_put(&c, 3, h);
    /* ordem LRU: 1(tail) - 2 - 3(head) */

    /* Acessar 1 → move para head */
    node_cache_get(&c, 1, NULL);
    /* ordem LRU: 2(tail) - 3 - 1(head) */

    /* Inserir 4 → despeja 2 (LRU) */
    make_hash(4, h); node_cache_put(&c, 4, h);

    ASSERT(node_cache_get(&c, 2, out) == 0, "no 2 (LRU) despejado");
    ASSERT(node_cache_get(&c, 1, out) == 1, "no 1 (acessado) permanece");
    ASSERT(node_cache_get(&c, 3, out) == 1, "no 3 permanece");
    ASSERT(node_cache_get(&c, 4, out) == 1, "no 4 (novo) presente");

    node_cache_destroy(&c);
}

static void test_update_existing(void) {
    printf("\n[TEST] put em no ja existente atualiza hash e promove ao topo\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h_old[SHA256_DIGEST_SIZE], h_new[SHA256_DIGEST_SIZE];
    uint8_t out[SHA256_DIGEST_SIZE];
    make_hash(100, h_old);
    make_hash(200, h_new);

    node_cache_put(&c, 5, h_old);
    node_cache_put(&c, 6, h_old);
    node_cache_put(&c, 5, h_new); /* atualizar nó 5 */

    ASSERT(c.size == 2, "size nao muda ao atualizar entrada existente");
    ASSERT(node_cache_get(&c, 5, out) == 1,
           "no 5 ainda no cache apos atualizacao");
    ASSERT(memcmp(out, h_new, SHA256_DIGEST_SIZE) == 0,
           "hash atualizado corretamente");
    /* nó 5 deve ser MRU */
    ASSERT(c.lru_head != NULL && c.lru_head->node_idx == 5,
           "no 5 e MRU apos atualizacao");

    node_cache_destroy(&c);
}

static void test_invalidate(void) {
    printf("\n[TEST] invalidate remove entrada do cache\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h[SHA256_DIGEST_SIZE], out[SHA256_DIGEST_SIZE];
    make_hash(7, h);

    node_cache_put(&c, 7, h);
    ASSERT(c.size == 1, "size == 1 antes de invalidate");

    node_cache_invalidate(&c, 7);
    ASSERT(c.size == 0,                       "size == 0 apos invalidate");
    ASSERT(node_cache_get(&c, 7, out) == 0,   "get apos invalidate = miss");

    /* Invalidar nó inexistente não deve quebrar nada */
    node_cache_invalidate(&c, 999);
    ASSERT(c.size == 0, "size inalterado ao invalidar no inexistente");

    node_cache_destroy(&c);
}

static void test_clear(void) {
    printf("\n[TEST] clear esvazia o cache e zera estatisticas\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h[SHA256_DIGEST_SIZE], out[SHA256_DIGEST_SIZE];
    make_hash(1, h); node_cache_put(&c, 1, h);
    make_hash(2, h); node_cache_put(&c, 2, h);
    node_cache_get(&c, 1, NULL);
    node_cache_get(&c, 99, NULL); /* miss */

    node_cache_clear(&c);

    ASSERT(c.size     == 0,    "size == 0 apos clear");
    ASSERT(c.hits     == 0,    "hits zerados");
    ASSERT(c.misses   == 0,    "misses zerados");
    ASSERT(c.evictions== 0,    "evictions zerados");
    ASSERT(c.lru_head == NULL, "lru_head == NULL");
    ASSERT(c.lru_tail == NULL, "lru_tail == NULL");
    ASSERT(node_cache_get(&c, 1, out) == 0, "no 1 nao existe apos clear");

    node_cache_destroy(&c);
}

static void test_stats(void) {
    printf("\n[TEST] estatisticas (hit_ratio)\n");

    node_cache_t c;
    node_cache_init(&c, 4);

    uint8_t h[SHA256_DIGEST_SIZE];
    make_hash(1, h); node_cache_put(&c, 1, h);
    make_hash(2, h); node_cache_put(&c, 2, h);

    node_cache_get(&c, 1, NULL); /* hit  */
    node_cache_get(&c, 2, NULL); /* hit  */
    node_cache_get(&c, 3, NULL); /* miss */
    node_cache_get(&c, 4, NULL); /* miss */

    ASSERT(c.hits   == 2, "hits == 2");
    ASSERT(c.misses == 2, "misses == 2");

    double ratio = node_cache_hit_ratio(&c);
    ASSERT(ratio > 0.49 && ratio < 0.51, "hit_ratio == 0.50");

    node_cache_destroy(&c);

    /* Cache vazio → ratio == 0.0 */
    node_cache_t empty;
    node_cache_init(&empty, 2);
    ASSERT(node_cache_hit_ratio(&empty) == 0.0, "hit_ratio == 0.0 sem acessos");
    node_cache_destroy(&empty);
}

static void test_null_safety(void) {
    printf("\n[TEST] seguranca contra NULL\n");

    /* Nenhuma dessas chamadas deve travar */
    node_cache_get(NULL, 0, NULL);
    node_cache_put(NULL, 0, NULL);
    node_cache_invalidate(NULL, 0);
    node_cache_clear(NULL);
    node_cache_destroy(NULL);
    node_cache_print_stats(NULL);

    double r = node_cache_hit_ratio(NULL);
    ASSERT(r == 0.0, "hit_ratio(NULL) == 0.0");

    ASSERT(1, "chamadas com NULL nao travaram");
}

static void test_many_items(void) {
    printf("\n[TEST] muitos itens — stress com evicções multiplas\n");

    const size_t CAP = 16;
    const int    N   = 64; /* 4× a capacidade */

    node_cache_t c;
    node_cache_init(&c, CAP);

    uint8_t h[SHA256_DIGEST_SIZE], out[SHA256_DIGEST_SIZE];

    /* Inserir N itens sequencialmente */
    for (int i = 0; i < N; i++) {
        make_hash((uint64_t)i, h);
        node_cache_put(&c, (uint64_t)i, h);
    }

    ASSERT(c.size == CAP,            "size == CAP apos N inserções");
    ASSERT(c.evictions == (uint64_t)(N - (int)CAP), "evictions == N - CAP");

    /* Últimos CAP itens devem estar presentes */
    int all_present = 1;
    for (int i = N - (int)CAP; i < N; i++) {
        make_hash((uint64_t)i, h);
        if (node_cache_get(&c, (uint64_t)i, out) != 1
            || memcmp(out, h, SHA256_DIGEST_SIZE) != 0) {
            all_present = 0;
        }
    }
    ASSERT(all_present, "ultimos CAP itens presentes e corretos");

    /* Primeiros (N - CAP) itens devem ter sido despejados */
    int all_evicted = 1;
    for (int i = 0; i < N - (int)CAP; i++) {
        if (node_cache_get(&c, (uint64_t)i, out) != 0)
            all_evicted = 0;
    }
    ASSERT(all_evicted, "primeiros itens foram despejados");

    node_cache_destroy(&c);
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("=== Testes: node_cache (LRU) ===\n");

    test_init_destroy();
    test_put_get_basic();
    test_lru_order();
    test_eviction();
    test_eviction_order();
    test_update_existing();
    test_invalidate();
    test_clear();
    test_stats();
    test_null_safety();
    test_many_items();

    printf("\nResultado: %d/%d testes passaram\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
