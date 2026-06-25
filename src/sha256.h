/*
 * sha256.h — Interface SHA-256 portável (sem dependências externas)
 *
 * Implementação prevista conforme FIPS PUB 180-4.
 * A ser implementada em sha256.c (E1).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

/* Tamanho do digest em bytes */
#define SHA256_DIGEST_SIZE 32

/* Estado interno do algoritmo */
typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[64];
    size_t   buf_len;
} sha256_ctx_t;

/* Inicializa o contexto com os valores IV do SHA-256 */
void sha256_init(sha256_ctx_t *ctx);

/* Alimenta dados ao contexto (pode ser chamado múltiplas vezes) */
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len);

/* Finaliza e escreve o digest de 32 bytes em `out` */
void sha256_final(sha256_ctx_t *ctx, uint8_t out[SHA256_DIGEST_SIZE]);

/* Conveniência: hash de um buffer inteiro em uma chamada */
void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);

#endif /* SHA256_H */
