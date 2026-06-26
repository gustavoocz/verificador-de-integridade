/*
 * test_verity.c — Testes unitários da camada de leitura verificada
 *
 * Cobre: open/close, leitura de blocos íntegros, detecção de corrupção,
 * persistência da verificação (outros blocos OK), benefício do cache LRU
 * (hash_computations cai na segunda leitura) e segurança contra NULL.
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/verity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Cria imagem com N blocos de BSZ bytes; bloco b = byte (b * 0x11 + 1) */
static int create_image(const char *path, int n_blocks, uint32_t bsz) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint8_t *buf = (uint8_t *)malloc(bsz);
    if (!buf) { fclose(fp); return -1; }
    for (int b = 0; b < n_blocks; b++) {
        memset(buf, (uint8_t)(b * 0x11 + 1), bsz);
        fwrite(buf, 1, bsz, fp);
    }
    free(buf);
    fclose(fp);
    return 0;
}

/* Corrompe 1 byte na imagem (offset absoluto) */
static void corrupt_byte(const char *path, long offset, uint8_t val) {
    FILE *fp = fopen(path, "r+b");
    if (!fp) return;
    fseek(fp, offset, SEEK_SET);
    fputc(val, fp);
    fclose(fp);
}

/* ── Fixture comum ─────────────────────────────────────────────────────────── */
#define IMG "_tv_test.img"
#define VRT "_tv_test.verity"
#define N_BLOCKS 8
#define BSZ      512

static int setup(void) {
    if (create_image(IMG, N_BLOCKS, BSZ) != 0) return -1;
    hash_tree_t t;
    if (hash_tree_build(&t, IMG, BSZ) != 0) return -1;
    int r = hash_tree_save(&t, VRT);
    hash_tree_free(&t);
    return r;
}

static void teardown(void) {
    remove(IMG);
    remove(VRT);
}

/* ── Testes ────────────────────────────────────────────────────────────────── */

static void test_open_close(void) {
    printf("\n[TEST] verity_open / verity_close\n");
    setup();

    verity_ctx_t ctx;
    int ret = verity_open(&ctx, IMG, VRT, 0);
    ASSERT(ret == 0,                     "open retorna 0");
    ASSERT(ctx.image_fp  != NULL,        "image_fp aberto");
    ASSERT(ctx.tree.nodes != NULL,       "arvore carregada");
    ASSERT(ctx.cache_enabled == 0,       "cache desabilitado (capacity=0)");
    ASSERT(ctx.reads_ok     == 0,        "reads_ok inicial == 0");
    ASSERT(ctx.reads_failed == 0,        "reads_failed inicial == 0");
    ASSERT(ctx.hash_computations == 0,   "hash_computations inicial == 0");

    verity_close(&ctx);
    ASSERT(ctx.image_fp == NULL, "image_fp fechado apos close");
    ASSERT(ctx.tree.nodes == NULL, "arvore liberada apos close");

    /* Open com .verity inexistente deve falhar */
    ret = verity_open(&ctx, IMG, "_nao_existe.verity", 0);
    ASSERT(ret == -1, "open com .verity invalido retorna -1");

    /* Open com imagem inexistente deve falhar */
    ret = verity_open(&ctx, "_nao_existe.img", VRT, 0);
    ASSERT(ret == -1, "open com imagem invalida retorna -1");

    teardown();
}

static void test_read_intact_no_cache(void) {
    printf("\n[TEST] verity_read — blocos integros sem cache\n");
    setup();

    verity_ctx_t ctx;
    verity_open(&ctx, IMG, VRT, 0);

    uint8_t buf[BSZ];
    int all_ok = 1;
    for (int b = 0; b < N_BLOCKS; b++) {
        if (verity_read(&ctx, buf, (uint64_t)b) != 0) all_ok = 0;
    }
    ASSERT(all_ok, "todos os blocos integros retornam 0");
    ASSERT(ctx.reads_ok     == N_BLOCKS, "reads_ok == N_BLOCKS");
    ASSERT(ctx.reads_failed == 0,        "reads_failed == 0");
    /* Sem cache: 1 hash por bloco (apenas a folha) */
    ASSERT(ctx.hash_computations == (uint64_t)N_BLOCKS,
           "hash_computations == N_BLOCKS (so folha, sem cache)");

    verity_close(&ctx);
    teardown();
}

static void test_read_corrupted_no_cache(void) {
    printf("\n[TEST] verity_read — corrupcao detectada sem cache\n");
    setup();

    /* Corromper byte 42 do bloco 3 */
    corrupt_byte(IMG, (long)(3 * BSZ + 42), 0xFF);

    verity_ctx_t ctx;
    verity_open(&ctx, IMG, VRT, 0);

    uint8_t buf[BSZ];

    /* Bloco 3 deve falhar */
    ASSERT(verity_read(&ctx, buf, 3) == -1, "bloco 3 corrompido detectado");
    ASSERT(ctx.reads_failed == 1,           "reads_failed == 1");

    /* Demais blocos devem passar */
    int others_ok = 1;
    for (int b = 0; b < N_BLOCKS; b++) {
        if (b == 3) continue;
        if (verity_read(&ctx, buf, (uint64_t)b) != 0) others_ok = 0;
    }
    ASSERT(others_ok, "demais blocos permanecem integros");
    ASSERT(ctx.reads_ok == (uint64_t)(N_BLOCKS - 1),
           "reads_ok == N_BLOCKS - 1");

    verity_close(&ctx);
    teardown();
}

static void test_read_intact_with_cache(void) {
    printf("\n[TEST] verity_read — blocos integros com cache\n");
    setup();

    verity_ctx_t ctx;
    /* cache com capacidade para todos os nós da árvore de 8 blocos (15 nós) */
    verity_open(&ctx, IMG, VRT, 32);
    ASSERT(ctx.cache_enabled == 1, "cache habilitado");

    uint8_t buf[BSZ];

    /* Primeira leitura de todos os blocos → popula o cache */
    for (int b = 0; b < N_BLOCKS; b++)
        verity_read(&ctx, buf, (uint64_t)b);

    uint64_t comp_1st = ctx.hash_computations;
    ctx.reads_ok = 0;
    ctx.hash_computations = 0;

    /* Segunda leitura dos mesmos blocos → deve fazer menos hashes */
    for (int b = 0; b < N_BLOCKS; b++)
        verity_read(&ctx, buf, (uint64_t)b);

    uint64_t comp_2nd = ctx.hash_computations;

    ASSERT(ctx.reads_ok == (uint64_t)N_BLOCKS, "todos OK na 2a leitura");
    ASSERT(comp_2nd < comp_1st,
           "2a leitura faz menos hashes que a 1a (cache ativo)");

    printf("    1a leitura: %lu hashes | 2a leitura: %lu hashes\n",
           (unsigned long)comp_1st, (unsigned long)comp_2nd);

    verity_close(&ctx);
    teardown();
}

static void test_cache_stops_at_ancestor(void) {
    printf("\n[TEST] cache — parada antecipada em no ancestral\n");

    /* Imagem pequena: 2 blocos → arvore com 3 nos (raiz, folha0, folha1) */
    const char *img = "_tv_small.img";
    const char *vrt = "_tv_small.verity";
    create_image(img, 2, BSZ);
    hash_tree_t t;
    hash_tree_build(&t, img, BSZ);
    hash_tree_save(&t, vrt);
    hash_tree_free(&t);

    verity_ctx_t ctx;
    verity_open(&ctx, img, vrt, 8);

    uint8_t buf[BSZ];

    /* Ler bloco 0 → percorre: folha(1 hash) + raiz(1 hash) = 2 */
    verity_read(&ctx, buf, 0);
    uint64_t after_first = ctx.hash_computations;
    /* Deve ter calculado: 1 (folha0) + 1 (raiz) = 2 hashes */
    ASSERT(after_first == 2, "1a leitura: 2 hashes (folha + raiz)");

    /* Ler bloco 1 → raiz já está no cache → 1 (folha1) + 1 (raiz tentativa) = 2,
     * mas para ao hit no cache da raiz. Ainda 2 hashes, porém para mais cedo */
    verity_read(&ctx, buf, 1);
    uint64_t after_second = ctx.hash_computations;
    /* 2 (primeiro) + 2 (segundo, que faz folha + computa pai mas pára no cache) */
    ASSERT(after_second == 4, "2a leitura (outro bloco): 2 hashes, para no cache");

    ASSERT(ctx.reads_ok == 2, "ambos os blocos verificados OK");
    ASSERT(ctx.cache.hits >= 1, "cache teve ao menos 1 hit");

    verity_close(&ctx);
    remove(img);
    remove(vrt);
}

static void test_corrupted_with_cache(void) {
    printf("\n[TEST] corrupcao detectada mesmo com cache habilitado\n");
    setup();

    corrupt_byte(IMG, (long)(5 * BSZ + 10), 0xAB);

    verity_ctx_t ctx;
    verity_open(&ctx, IMG, VRT, 32);

    uint8_t buf[BSZ];
    ASSERT(verity_read(&ctx, buf, 5) == -1, "bloco 5 corrompido detectado com cache");

    /* Outros blocos passam normalmente */
    int ok = 1;
    for (int b = 0; b < N_BLOCKS; b++) {
        if (b == 5) continue;
        if (verity_read(&ctx, buf, (uint64_t)b) != 0) ok = 0;
    }
    ASSERT(ok, "demais blocos OK apos corrupcao do bloco 5");

    verity_close(&ctx);
    teardown();
}

static void test_null_safety(void) {
    printf("\n[TEST] seguranca contra NULL\n");
    uint8_t buf[BSZ];
    /* Nenhuma dessas deve travar */
    verity_close(NULL);
    verity_print_stats(NULL);
    int r = verity_read(NULL, buf, 0);
    ASSERT(r == -1, "verity_read(NULL,...) retorna -1");
    r = verity_open(NULL, "a", "b", 0);
    ASSERT(r == -1, "verity_open(NULL,...) retorna -1");
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("=== Testes: verity (camada de leitura verificada) ===\n");

    test_open_close();
    test_read_intact_no_cache();
    test_read_corrupted_no_cache();
    test_read_intact_with_cache();
    test_cache_stops_at_ancestor();
    test_corrupted_with_cache();
    test_null_safety();

    printf("\nResultado: %d/%d testes passaram\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
