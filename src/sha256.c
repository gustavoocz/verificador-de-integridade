/*
 * sha256.c — Implementação portável de SHA-256 (FIPS PUB 180-4)
 *
 * Sem dependências externas. Vetores de referência: FIPS 180-4 Appendix B.
 * Versão defensiva: sem macros complexos, funções static inline para
 * garantir comportamento correto em gcc antigo (4.9.x).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "sha256.h"
#include <string.h>

/* ── Constantes K ─────────────────────────────────────────────────────────── */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Valores iniciais H */
static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/* ── Funções auxiliares inline ────────────────────────────────────────────── */

static inline uint32_t rotr32(uint32_t x, unsigned int n) {
    return (x >> n) | (x << (32u - n));
}

static inline uint32_t ch(uint32_t e, uint32_t f, uint32_t g) {
    return (e & f) ^ ((~e) & g);
}

static inline uint32_t maj(uint32_t a, uint32_t b, uint32_t c) {
    return (a & b) ^ (a & c) ^ (b & c);
}

static inline uint32_t ep0(uint32_t a) {
    return rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
}

static inline uint32_t ep1(uint32_t e) {
    return rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
}

static inline uint32_t sig0(uint32_t x) {
    return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}

static inline uint32_t sig1(uint32_t x) {
    return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v      );
}

static inline void write_be64(uint8_t *p, uint64_t v) {
    write_be32(p,     (uint32_t)(v >> 32));
    write_be32(p + 4, (uint32_t)(v      ));
}

/* ── Núcleo: processa um bloco de 64 bytes ────────────────────────────────── */
static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = read_be32(block + i * 4);
    for (i = 16; i < 64; i++)
        w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];

    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + ep1(e) + ch(e, f, g) + K[i] + w[i];
        t2 = ep0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

void sha256_init(sha256_ctx_t *ctx) {
    memcpy(ctx->state, H0, sizeof(H0));
    ctx->bit_count = 0;
    ctx->buf_len   = 0;
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t available, to_copy;

    ctx->bit_count += (uint64_t)len * 8;

    while (len > 0) {
        available = 64 - ctx->buf_len;
        to_copy   = (len < available) ? len : available;
        memcpy(ctx->buffer + ctx->buf_len, data, to_copy);
        ctx->buf_len += to_copy;
        data         += to_copy;
        len          -= to_copy;

        if (ctx->buf_len == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buf_len = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t out[SHA256_DIGEST_SIZE]) {
    int i;

    /* Append bit '1' */
    ctx->buffer[ctx->buf_len++] = 0x80;

    /* Se não couber o campo de comprimento (8 bytes) neste bloco */
    if (ctx->buf_len > 56) {
        memset(ctx->buffer + ctx->buf_len, 0, 64 - ctx->buf_len);
        sha256_transform(ctx, ctx->buffer);
        ctx->buf_len = 0;
    }

    /* Zeros até posição 56 */
    memset(ctx->buffer + ctx->buf_len, 0, 56 - ctx->buf_len);

    /* Comprimento original em bits, big-endian 64-bit */
    write_be64(ctx->buffer + 56, ctx->bit_count);
    sha256_transform(ctx, ctx->buffer);

    /* Serializar estado final */
    for (i = 0; i < 8; i++)
        write_be32(out + i * 4, ctx->state[i]);

    /* Zerar contexto por segurança */
    memset(ctx, 0, sizeof(*ctx));
}

void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}
