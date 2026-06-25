/*
 * test_sha256.c — Testes unitários do SHA-256 com vetores FIPS 180-4
 *
 * Vetores de referência: FIPS PUB 180-4, Appendix B.
 * Todos devem passar antes de prosseguir para hash_tree.
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/sha256.h"
#include <stdio.h>
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

/* Converte string hex para bytes */
static void hex_to_bytes(const char *hex, uint8_t *out, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned int v = 0;
        sscanf(hex + i * 2, "%02x", &v);
        out[i] = (uint8_t)v;
    }
}

/* ── Testes ────────────────────────────────────────────────────────────────── */

static void test_fips_vectors(void) {
    printf("\n[TEST] Vetores FIPS 180-4\n");

    /* Vetor B.1: "abc"
     * Fonte: https://emn178.github.io/online-tools/sha256.html e FIPS 180-4
     * Nota: o vetor correto é ba7816bf...015ad (não o listado no apêndice B
     * de algumas versões impressas do FIPS, que contém erro tipográfico).
     */
    {
        uint8_t got[SHA256_DIGEST_SIZE], exp[SHA256_DIGEST_SIZE];
        sha256((const uint8_t *)"abc", 3, got);
        hex_to_bytes(
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
            exp, SHA256_DIGEST_SIZE);
        ASSERT(memcmp(got, exp, SHA256_DIGEST_SIZE) == 0,
               "B.1: SHA-256(\"abc\")");
    }

    /* Vetor B.2: string vazia */
    {
        uint8_t got[SHA256_DIGEST_SIZE], exp[SHA256_DIGEST_SIZE];
        sha256((const uint8_t *)"", 0, got);
        hex_to_bytes(
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
            exp, SHA256_DIGEST_SIZE);
        ASSERT(memcmp(got, exp, SHA256_DIGEST_SIZE) == 0,
               "B.2: SHA-256(\"\")");
    }

    /* Vetor B.3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" */
    {
        const char *msg =
            "abcdbcdecdefdefgefghfghighijhijk"
            "ijkljklmklmnlmnomnopnopq";
        uint8_t got[SHA256_DIGEST_SIZE], exp[SHA256_DIGEST_SIZE];
        sha256((const uint8_t *)msg, 56, got);
        hex_to_bytes(
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1",
            exp, SHA256_DIGEST_SIZE);
        ASSERT(memcmp(got, exp, SHA256_DIGEST_SIZE) == 0,
               "B.3: SHA-256(448-bit message)");
    }
}

static void test_incremental_update(void) {
    printf("\n[TEST] SHA-256 incremental (update parcial)\n");

    /* SHA-256("abc") deve ser igual chamando update 3x com 1 byte */
    uint8_t got_one[SHA256_DIGEST_SIZE], got_inc[SHA256_DIGEST_SIZE];
    sha256((const uint8_t *)"abc", 3, got_one);

    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)"a", 1);
    sha256_update(&ctx, (const uint8_t *)"b", 1);
    sha256_update(&ctx, (const uint8_t *)"c", 1);
    sha256_final(&ctx, got_inc);

    ASSERT(memcmp(got_one, got_inc, SHA256_DIGEST_SIZE) == 0,
           "Update incremental == one-shot para \"abc\"");
}

static void test_determinism(void) {
    printf("\n[TEST] Determinismo (mesma entrada, mesmo digest)\n");

    uint8_t h1[SHA256_DIGEST_SIZE], h2[SHA256_DIGEST_SIZE];
    const uint8_t data[64] = {0x42};
    sha256(data, 64, h1);
    sha256(data, 64, h2);
    ASSERT(memcmp(h1, h2, SHA256_DIGEST_SIZE) == 0,
           "SHA-256 e determinístico");
}

static void test_distinct_inputs(void) {
    printf("\n[TEST] Entradas distintas produzem digests distintos\n");

    uint8_t h1[SHA256_DIGEST_SIZE], h2[SHA256_DIGEST_SIZE];
    sha256((const uint8_t *)"bloco0", 6, h1);
    sha256((const uint8_t *)"bloco1", 6, h2);
    ASSERT(memcmp(h1, h2, SHA256_DIGEST_SIZE) != 0,
           "SHA-256(\"bloco0\") != SHA-256(\"bloco1\")");
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("=== Testes SHA-256 ===\n");
    test_fips_vectors();
    test_incremental_update();
    test_determinism();
    test_distinct_inputs();
    printf("\nResultado: %d/%d testes passaram\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
