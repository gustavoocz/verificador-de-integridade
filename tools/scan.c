/*
 * scan.c — Varre todos os blocos de uma imagem e lista os corrompidos
 *
 * Uso: scan <imagem> <arquivo.verity> [--quiet]
 *   --quiet : omite linhas [OK], mostra só corrompidos e o resumo final
 *
 * Saída:
 *   [OK]        Bloco N integro
 *   [CORRUPTO]  Bloco N corrompido!
 *   ---
 *   Resumo: X/Y blocos OK | Z corrompidos
 *
 * Código de saída: 0 = tudo OK | 1 = algum bloco corrompido | 2 = erro fatal
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <imagem> <arquivo.verity> [--quiet]\n", prog);
    fprintf(stderr, "  --quiet : mostra apenas blocos corrompidos\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 2;
    }

    const char *image_path  = argv[1];
    const char *verity_path = argv[2];
    int         quiet       = (argc == 4 && strcmp(argv[3], "--quiet") == 0);

    /* Carregar árvore */
    hash_tree_t tree;
    if (hash_tree_load(&tree, verity_path) != 0) {
        perror("Erro ao carregar .verity");
        return 2;
    }

    /* Abrir imagem */
    FILE *fp = fopen(image_path, "rb");
    if (!fp) {
        perror("Erro ao abrir imagem");
        hash_tree_free(&tree);
        return 2;
    }

    uint8_t *buf = (uint8_t *)malloc(tree.block_size);
    if (!buf) {
        perror("malloc");
        fclose(fp);
        hash_tree_free(&tree);
        return 2;
    }

    uint64_t total       = tree.num_blocks;
    uint64_t ok_count    = 0;
    uint64_t bad_count   = 0;

    printf("Varrendo %lu blocos de %u bytes...\n\n",
           (unsigned long)total, tree.block_size);

    for (uint64_t b = 0; b < total; b++) {
        /* Ler bloco (bloco parcial: preencher com zeros) */
        size_t n = fread(buf, 1, tree.block_size, fp);
        if (n < tree.block_size)
            memset(buf + n, 0, tree.block_size - n);

        int result = hash_tree_verify_block(&tree, buf, b);

        if (result == 0) {
            ok_count++;
            if (!quiet)
                printf("[OK]       Bloco %lu integro\n", (unsigned long)b);
        } else {
            bad_count++;
            printf("[CORRUPTO] Bloco %lu corrompido!\n", (unsigned long)b);
        }
    }

    /* Resumo */
    printf("\n---\n");
    printf("Resumo: %lu/%lu blocos OK",
           (unsigned long)ok_count, (unsigned long)total);
    if (bad_count == 0) {
        printf(" | nenhum corrompido\n");
    } else {
        printf(" | %lu corrompido(s)\n", (unsigned long)bad_count);
        printf("\nBlocos corrompidos detectados — integridade comprometida!\n");
    }

    free(buf);
    fclose(fp);
    hash_tree_free(&tree);
    return (bad_count == 0) ? 0 : 1;
}
