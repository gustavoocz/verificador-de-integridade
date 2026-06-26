/*
 * verity.c — Camada de leitura verificada com cache LRU integrado (E3)
 *
 * Algoritmo de verity_read com cache:
 *   1. Lê o bloco da imagem e calcula SHA-256 da folha.
 *   2. Compara com a folha armazenada na árvore (detecção rápida).
 *   3. Sobe o caminho folha→raiz calculando o hash de cada nó pai a partir
 *      do filho atual e do irmão (armazenado na árvore):
 *        • Cache HIT  no pai → compara com o valor computado e para cedo
 *          (o caminho acima já foi verificado numa leitura anterior).
 *        • Cache MISS no pai → compara com a árvore, armazena no cache e sobe.
 *   4. Após verificação bem-sucedida, cacheia o hash da folha.
 *
 * Benefício mensurável: `hash_computations` cai nas leituras seguintes de
 * blocos que compartilham nós internos com blocos já lidos.
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "verity.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers internos ─────────────────────────────────────────────────────── */

/* Ponteiro direto ao hash do nó i (nodes é campo público de hash_tree_t) */
static inline const uint8_t *tree_node(const hash_tree_t *t, uint64_t i) {
    return t->nodes + i * SHA256_DIGEST_SIZE;
}

/* Índice BFS da folha correspondente ao bloco b */
static inline uint64_t leaf_index(const hash_tree_t *t, uint64_t b) {
    return (t->num_leaves - 1) + b;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

int verity_open(verity_ctx_t *ctx, const char *image_path,
                const char *verity_path, size_t cache_capacity) {
    if (!ctx || !image_path || !verity_path) {
        errno = EINVAL;
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));

    /* Carregar árvore de hash */
    if (hash_tree_load(&ctx->tree, verity_path) != 0)
        return -1;

    /* Abrir imagem em modo leitura binária */
    ctx->image_fp = fopen(image_path, "rb");
    if (!ctx->image_fp) {
        hash_tree_free(&ctx->tree);
        return -1;
    }

    /* Inicializar cache LRU (opcional) */
    if (cache_capacity > 0) {
        if (node_cache_init(&ctx->cache, cache_capacity) == 0)
            ctx->cache_enabled = 1;
        /* Se init falhar, continua sem cache (graceful degradation) */
    }

    return 0;
}

void verity_close(verity_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->image_fp)    fclose(ctx->image_fp);
    hash_tree_free(&ctx->tree);
    if (ctx->cache_enabled) node_cache_destroy(&ctx->cache);
    memset(ctx, 0, sizeof(*ctx));
}

int verity_read(verity_ctx_t *ctx, uint8_t *buf, uint64_t block_idx) {
    if (!ctx || !buf || !ctx->image_fp || !ctx->tree.nodes) {
        errno = EINVAL;
        return -1;
    }

    hash_tree_t *tree = &ctx->tree;

    if (block_idx >= tree->num_blocks) {
        errno = EINVAL;
        return -1;
    }

    /* ── 1. Ler bloco da imagem ─────────────────────────────────────────── */
    if (fseek(ctx->image_fp,
              (long)(block_idx * (uint64_t)tree->block_size), SEEK_SET) != 0)
        return -1;

    size_t n = fread(buf, 1, tree->block_size, ctx->image_fp);
    /* Bloco parcial (último): completar com zeros (mesma convenção do build) */
    if (n < tree->block_size)
        memset(buf + n, 0, tree->block_size - n);

    /* ── 2. Hash da folha ───────────────────────────────────────────────── */
    uint8_t leaf_hash[SHA256_DIGEST_SIZE];
    sha256(buf, tree->block_size, leaf_hash);
    ctx->hash_computations++;

    /* ── 3. Verificar folha vs árvore (detecção rápida) ────────────────── */
    uint64_t idx = leaf_index(tree, block_idx);
    if (memcmp(leaf_hash, tree_node(tree, idx), SHA256_DIGEST_SIZE) != 0) {
        ctx->reads_failed++;
        return -1;
    }

    /* ── 4. Sem cache: retorna OK ───────────────────────────────────────── */
    if (!ctx->cache_enabled) {
        ctx->reads_ok++;
        return 0;
    }

    /* ── 5. Com cache: subir folha→raiz com parada antecipada ───────────── */
    uint8_t current[SHA256_DIGEST_SIZE];
    memcpy(current, leaf_hash, SHA256_DIGEST_SIZE);

    while (idx > 0) {
        uint64_t par = (idx - 1) / 2;
        uint64_t sib = (idx % 2 == 1) ? idx + 1 : idx - 1;

        /* Montar concatenação esquerdo||direito e calcular hash do pai */
        uint8_t concat[SHA256_DIGEST_SIZE * 2];
        if (idx % 2 == 1) { /* filho esquerdo */
            memcpy(concat,                      current,              SHA256_DIGEST_SIZE);
            memcpy(concat + SHA256_DIGEST_SIZE, tree_node(tree, sib), SHA256_DIGEST_SIZE);
        } else {             /* filho direito  */
            memcpy(concat,                      tree_node(tree, sib), SHA256_DIGEST_SIZE);
            memcpy(concat + SHA256_DIGEST_SIZE, current,              SHA256_DIGEST_SIZE);
        }

        uint8_t computed_par[SHA256_DIGEST_SIZE];
        sha256(concat, SHA256_DIGEST_SIZE * 2, computed_par);
        ctx->hash_computations++;

        /* ── Cache HIT no pai: parada antecipada ─────────────────────── */
        uint8_t cached_par[SHA256_DIGEST_SIZE];
        if (node_cache_get(&ctx->cache, par, cached_par)) {
            if (memcmp(computed_par, cached_par, SHA256_DIGEST_SIZE) == 0)
                break; /* tudo acima já verificado → parar */
            /* Inconsistência com valor cacheado → corrupção */
            ctx->reads_failed++;
            return -1;
        }

        /* ── Cache MISS: verificar vs árvore e armazenar ─────────────── */
        if (memcmp(computed_par, tree_node(tree, par), SHA256_DIGEST_SIZE) != 0) {
            ctx->reads_failed++;
            return -1;
        }
        node_cache_put(&ctx->cache, par, computed_par);

        memcpy(current, computed_par, SHA256_DIGEST_SIZE);
        idx = par;
    }

    /* Cachear hash da folha (próxima leitura do mesmo bloco beneficia-se) */
    node_cache_put(&ctx->cache, leaf_index(tree, block_idx), leaf_hash);

    ctx->reads_ok++;
    return 0;
}

void verity_print_stats(const verity_ctx_t *ctx) {
    if (!ctx) return;
    printf("=== verity_read — estatisticas ===\n");
    printf("Leituras OK    : %lu\n",  (unsigned long)ctx->reads_ok);
    printf("Leituras FALHA : %lu\n",  (unsigned long)ctx->reads_failed);
    printf("Hashes comp.   : %lu\n",  (unsigned long)ctx->hash_computations);
    if (ctx->cache_enabled)
        node_cache_print_stats(&ctx->cache);
    else
        printf("Cache          : desabilitado\n");
}
