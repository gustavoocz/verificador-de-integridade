/*
 * test_hash_tree.c — Testes unitários da Merkle tree
 *
 * Cobre: next_power_of_two, construção (1 bloco, 3 blocos),
 * persistência (save/load), verificação de bloco íntegro e
 * detecção de corrupção (teste de fogo).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
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

/* Cria um arquivo de imagem com N blocos de BSZ bytes, cada bloco preenchido
 * com o valor (byte)(bloco * 0x11 + 1) — padrão determinístico. */
static int create_image(const char *path, int n_blocks, uint32_t bsz) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint8_t *buf = (uint8_t *)malloc(bsz);
    if (!buf) { fclose(fp); return -1; }
    for (int b = 0; b < n_blocks; b++) {
        memset(buf, (unsigned char)(b * 0x11 + 1), bsz);
        fwrite(buf, 1, bsz, fp);
    }
    free(buf);
    fclose(fp);
    return 0;
}

/* Lê o bloco b de uma imagem para buf (BSZ bytes) */
static int read_block(const char *path, int b, uint32_t bsz, uint8_t *buf) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, (long)b * bsz, SEEK_SET);
    size_t n = fread(buf, 1, bsz, fp);
    fclose(fp);
    return (n == bsz) ? 0 : -1;
}

/* ── Teste 1: next_power_of_two ──────────────────────────────────────────── */
static void test_next_power_of_two(void) {
    printf("\n[TEST] next_power_of_two\n");
    ASSERT(next_power_of_two(0)    == 1,    "0 -> 1");
    ASSERT(next_power_of_two(1)    == 1,    "1 -> 1");
    ASSERT(next_power_of_two(2)    == 2,    "2 -> 2");
    ASSERT(next_power_of_two(3)    == 4,    "3 -> 4");
    ASSERT(next_power_of_two(4)    == 4,    "4 -> 4");
    ASSERT(next_power_of_two(5)    == 8,    "5 -> 8");
    ASSERT(next_power_of_two(8)    == 8,    "8 -> 8");
    ASSERT(next_power_of_two(9)    == 16,   "9 -> 16");
    ASSERT(next_power_of_two(1000) == 1024, "1000 -> 1024");
}

/* ── Teste 2: Construção com 1 bloco (folha == raiz) ─────────────────────── */
static void test_build_single_block(void) {
    printf("\n[TEST] hash_tree_build — 1 bloco\n");

    const char *img = "_test_1blk.img";
    create_image(img, 1, 512);

    hash_tree_t tree;
    int ret = hash_tree_build(&tree, img, 512);
    ASSERT(ret == 0,             "build retorna 0");
    ASSERT(tree.num_blocks == 1, "num_blocks == 1");
    ASSERT(tree.num_leaves == 1, "num_leaves == 1 (pot. 2)");
    ASSERT(tree.num_nodes  == 1, "num_nodes  == 1");
    ASSERT(tree.nodes      != NULL, "nodes alocado");

    /* Root hash deve ser sha256 do único bloco */
    uint8_t block[512];
    memset(block, 0x01, 512);
    uint8_t expected[SHA256_DIGEST_SIZE];
    sha256(block, 512, expected);
    ASSERT(memcmp(tree.root_hash, expected, SHA256_DIGEST_SIZE) == 0,
           "root_hash == sha256(bloco0)");

    hash_tree_free(&tree);
    remove(img);
}

/* ── Teste 3: Construção com 3 blocos (num_leaves=4, num_nodes=7) ─────────── */
static void test_build_three_blocks(void) {
    printf("\n[TEST] hash_tree_build — 3 blocos\n");

    const char *img = "_test_3blk.img";
    create_image(img, 3, 512);

    hash_tree_t tree;
    int ret = hash_tree_build(&tree, img, 512);
    ASSERT(ret == 0,             "build retorna 0");
    ASSERT(tree.num_blocks == 3, "num_blocks == 3");
    ASSERT(tree.num_leaves == 4, "num_leaves == 4 (prox. pot. 2)");
    ASSERT(tree.num_nodes  == 7, "num_nodes  == 7 (2*4-1)");

    /* Verificar que o nó 0 (raiz) == root_hash */
    uint8_t node0[SHA256_DIGEST_SIZE];
    hash_tree_get_node(&tree, 0, node0);
    ASSERT(memcmp(node0, tree.root_hash, SHA256_DIGEST_SIZE) == 0,
           "nodes[0] == root_hash");

    /* Verificar que nós internos são sha256 dos filhos */
    uint8_t l[SHA256_DIGEST_SIZE], r[SHA256_DIGEST_SIZE];
    uint8_t concat[SHA256_DIGEST_SIZE * 2];
    uint8_t expected[SHA256_DIGEST_SIZE];

    /* Nó 1 = sha256(nodes[3] || nodes[4]) */
    hash_tree_get_node(&tree, 3, l);
    hash_tree_get_node(&tree, 4, r);
    memcpy(concat,                      l, SHA256_DIGEST_SIZE);
    memcpy(concat + SHA256_DIGEST_SIZE, r, SHA256_DIGEST_SIZE);
    sha256(concat, SHA256_DIGEST_SIZE * 2, expected);
    hash_tree_get_node(&tree, 1, l);
    ASSERT(memcmp(l, expected, SHA256_DIGEST_SIZE) == 0,
           "no interno 1 = sha256(no3 || no4)");

    hash_tree_free(&tree);
    remove(img);
}

/* ── Teste 4: save e load — round trip ───────────────────────────────────── */
static void test_save_load(void) {
    printf("\n[TEST] hash_tree_save / hash_tree_load\n");

    const char *img = "_test_sl.img";
    const char *vrt = "_test_sl.verity";
    create_image(img, 3, 512);

    hash_tree_t t1, t2;
    hash_tree_build(&t1, img, 512);
    int ret = hash_tree_save(&t1, vrt);
    ASSERT(ret == 0, "save retorna 0");

    ret = hash_tree_load(&t2, vrt);
    ASSERT(ret == 0,                              "load retorna 0");
    ASSERT(t2.block_size == t1.block_size,        "block_size preservado");
    ASSERT(t2.num_blocks == t1.num_blocks,        "num_blocks preservado");
    ASSERT(t2.num_nodes  == t1.num_nodes,         "num_nodes preservado");
    ASSERT(memcmp(t1.root_hash, t2.root_hash,
                  SHA256_DIGEST_SIZE) == 0,       "root_hash identico apos round trip");
    ASSERT(memcmp(t1.nodes, t2.nodes,
                  (size_t)t1.num_nodes * SHA256_DIGEST_SIZE) == 0,
           "array de nos identico apos round trip");

    hash_tree_free(&t1);
    hash_tree_free(&t2);
    remove(img);
    remove(vrt);
}

/* ── Teste 5: load rejeita magic inválido ───────────────────────────────── */
static void test_load_bad_magic(void) {
    printf("\n[TEST] hash_tree_load — magic invalido\n");

    const char *vrt = "_test_bad.verity";
    FILE *fp = fopen(vrt, "wb");
    const char *junk = "INVALID!XXXXXXXX";
    fwrite(junk, 1, 16, fp);
    fclose(fp);

    hash_tree_t tree;
    int ret = hash_tree_load(&tree, vrt);
    ASSERT(ret == -1, "load retorna -1 para magic invalido");

    remove(vrt);
}

/* ── Teste 6: verify_block — blocos íntegros ─────────────────────────────── */
static void test_verify_intact(void) {
    printf("\n[TEST] hash_tree_verify_block — blocos integros\n");

    const char *img = "_test_vi.img";
    const char *vrt = "_test_vi.verity";
    const uint32_t bsz = 512;
    const int n = 4;
    create_image(img, n, bsz);

    hash_tree_t tree;
    hash_tree_build(&tree, img, bsz);
    hash_tree_save(&tree, vrt);

    uint8_t *buf = (uint8_t *)malloc(bsz);
    for (int b = 0; b < n; b++) {
        read_block(img, b, bsz, buf);
        int ret = hash_tree_verify_block(&tree, buf, (uint64_t)b);
        char msg[64];
        snprintf(msg, sizeof(msg), "bloco %d integro (verify == 0)", b);
        ASSERT(ret == 0, msg);
    }

    free(buf);
    hash_tree_free(&tree);
    remove(img);
    remove(vrt);
}

/* ── Teste 7: verify_block — TESTE DE FOGO (corrupção detectada) ──────────── */
static void test_fire_test(void) {
    printf("\n[TEST] TESTE DE FOGO — corrupcao detectada\n");

    const char *img = "_test_ff.img";
    const char *vrt = "_test_ff.verity";
    const uint32_t bsz = 512;
    const int n = 8;
    const int corrupted_block = 5; /* bloco a corromper */

    create_image(img, n, bsz);

    hash_tree_t tree;
    hash_tree_build(&tree, img, bsz);
    hash_tree_save(&tree, vrt);

    /* Ler blocos, corromper o bloco 5 e verificar */
    uint8_t *buf = (uint8_t *)malloc(bsz);

    /* Bloco 5 com 1 byte corrompido deve FALHAR */
    read_block(img, corrupted_block, bsz, buf);
    buf[42] ^= 0xFF; /* corrompendo byte 42 */
    int ret = hash_tree_verify_block(&tree, buf, (uint64_t)corrupted_block);
    ASSERT(ret == -1, "bloco corrompido detectado (retorna -1)");

    /* Todos os outros blocos com dados corretos devem PASSAR */
    int all_ok = 1;
    for (int b = 0; b < n; b++) {
        if (b == corrupted_block) continue;
        read_block(img, b, bsz, buf);
        if (hash_tree_verify_block(&tree, buf, (uint64_t)b) != 0) {
            all_ok = 0;
            fprintf(stderr, "  Bloco %d falhou inesperadamente\n", b);
        }
    }
    ASSERT(all_ok, "demais blocos permanecem legíveis apos corrupcao de 1 bloco");

    free(buf);
    hash_tree_free(&tree);
    remove(img);
    remove(vrt);
}

/* ── Teste 8: verify_block com árvore carregada do disco ─────────────────── */
static void test_verify_after_load(void) {
    printf("\n[TEST] verify_block com arvore carregada do disco\n");

    const char *img = "_test_val.img";
    const char *vrt = "_test_val.verity";
    const uint32_t bsz = 512;
    create_image(img, 6, bsz);

    /* Construir e salvar */
    hash_tree_t t_build;
    hash_tree_build(&t_build, img, bsz);
    hash_tree_save(&t_build, vrt);
    hash_tree_free(&t_build);

    /* Carregar do disco e verificar */
    hash_tree_t t_load;
    hash_tree_load(&t_load, vrt);

    uint8_t *buf = (uint8_t *)malloc(bsz);
    for (int b = 0; b < 6; b++) {
        read_block(img, b, bsz, buf);
        int ret = hash_tree_verify_block(&t_load, buf, (uint64_t)b);
        char msg[64];
        snprintf(msg, sizeof(msg), "bloco %d integro via arvore carregada", b);
        ASSERT(ret == 0, msg);
    }

    free(buf);
    hash_tree_free(&t_load);
    remove(img);
    remove(vrt);
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("=== Testes: hash_tree ===\n");

    test_next_power_of_two();
    test_build_single_block();
    test_build_three_blocks();
    test_save_load();
    test_load_bad_magic();
    test_verify_intact();
    test_fire_test();
    test_verify_after_load();

    printf("\nResultado: %d/%d testes passaram\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
