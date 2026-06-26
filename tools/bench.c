/*
 * bench.c — Benchmark da camada verity_read com e sem cache LRU
 *
 * Uso: bench <imagem> <arquivo.verity> <cache_capacity> [n_passes]
 *   cache_capacity : 0 = sem cache; >0 = capacidade do LRU
 *   n_passes       : quantas varreduras completas (padrão: 2)
 *
 * Saída CSV:
 *   pass,cache_cap,hash_comp,hit_ratio,reads_ok,reads_failed,time_ms
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/verity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s <imagem> <arquivo.verity> <cache_capacity> [n_passes]\n",
            prog);
}

int main(int argc, char *argv[]) {
    if (argc < 4) { usage(argv[0]); return 1; }

    const char *img      = argv[1];
    const char *vrt      = argv[2];
    size_t      cache_cap = (size_t)atol(argv[3]);
    int         n_passes  = (argc >= 5) ? atoi(argv[4]) : 2;

    if (n_passes <= 0 || n_passes > 100) {
        fprintf(stderr, "Erro: n_passes deve ser entre 1 e 100.\n");
        return 1;
    }

    /* Abrir contexto de leitura verificada */
    verity_ctx_t ctx;
    if (verity_open(&ctx, img, vrt, cache_cap) != 0) {
        perror("verity_open");
        return 1;
    }

    uint8_t *buf = (uint8_t *)malloc(ctx.tree.block_size);
    if (!buf) { perror("malloc"); verity_close(&ctx); return 1; }

    uint64_t n_blocks = ctx.tree.num_blocks;

    /* Cabeçalho CSV */
    printf("pass,cache_cap,blocks,hash_comp,hit_ratio,reads_ok,reads_failed,time_ms\n");

    for (int pass = 1; pass <= n_passes; pass++) {
        /* Resetar contadores entre passes (cache mantém estado entre passes) */
        ctx.reads_ok          = 0;
        ctx.reads_failed      = 0;
        ctx.hash_computations = 0;
        if (ctx.cache_enabled) {
            ctx.cache.hits   = 0;
            ctx.cache.misses = 0;
        }

        clock_t start = clock();
        for (uint64_t b = 0; b < n_blocks; b++) {
            verity_read(&ctx, buf, b);
        }
        clock_t end = clock();

        double time_ms  = (double)(end - start) / (double)CLOCKS_PER_SEC * 1000.0;
        double hit_ratio = ctx.cache_enabled
                         ? node_cache_hit_ratio(&ctx.cache)
                         : 0.0;

        printf("%d,%lu,%lu,%lu,%.4f,%lu,%lu,%.3f\n",
               pass,
               (unsigned long)cache_cap,
               (unsigned long)n_blocks,
               (unsigned long)ctx.hash_computations,
               hit_ratio,
               (unsigned long)ctx.reads_ok,
               (unsigned long)ctx.reads_failed,
               time_ms);
        fflush(stdout);
    }

    free(buf);
    verity_close(&ctx);
    return 0;
}
