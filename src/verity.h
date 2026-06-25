/*
 * verity.h — Interface da camada de leitura verificada (SO)
 *
 * Integra a Merkle tree e o cache LRU para fornecer leitura com verificação
 * automática de integridade bloco a bloco.
 *
 * A ser implementada em verity.c (E3).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#ifndef VERITY_H
#define VERITY_H

#include "hash_tree.h"
#include "node_cache.h"
#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE         *image_fp;
    hash_tree_t   tree;
    node_cache_t  cache;
    int           cache_enabled;
    /* Estatísticas */
    uint64_t reads_ok;
    uint64_t reads_failed;
    uint64_t hash_computations;
} verity_ctx_t;

/* Abre imagem para leitura verificada (cache_capacity=0 desabilita cache) */
int  verity_open(verity_ctx_t *ctx, const char *image_path,
                 const char *verity_path, size_t cache_capacity);

/* Fecha e libera todos os recursos */
void verity_close(verity_ctx_t *ctx);

/* Lê e verifica o bloco block_idx; retorna 0 (OK) ou -1 (corrompido) */
int  verity_read(verity_ctx_t *ctx, uint8_t *buf, uint64_t block_idx);

/* Imprime estatísticas de leitura e cache */
void verity_print_stats(const verity_ctx_t *ctx);

#endif /* VERITY_H */
