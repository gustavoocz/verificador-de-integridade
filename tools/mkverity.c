/*
 * mkverity.c — Constrói a Merkle tree de uma imagem de disco e salva em .verity
 *
 * Uso: mkverity <imagem> <saida.verity> [block_size]
 *   imagem       : arquivo de imagem de disco (qualquer tamanho)
 *   saida.verity : arquivo de saída com a árvore de hash
 *   block_size   : tamanho do bloco em bytes (padrão: 4096)
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <imagem> <saida.verity> [block_size]\n", prog);
    fprintf(stderr, "  block_size padrao: 4096 bytes\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 1;
    }

    const char *image_path  = argv[1];
    const char *verity_path = argv[2];
    uint32_t    block_size  = 4096;

    if (argc == 4) {
        long bs = atol(argv[3]);
        if (bs <= 0 || bs > 65536) {
            fprintf(stderr, "Erro: block_size invalido (%s). Use 512..65536.\n", argv[3]);
            return 1;
        }
        block_size = (uint32_t)bs;
    }

    printf("Construindo arvore de hash...\n");
    printf("  Imagem     : %s\n", image_path);
    printf("  Saida      : %s\n", verity_path);
    printf("  Block size : %u bytes\n", block_size);

    hash_tree_t tree;
    if (hash_tree_build(&tree, image_path, block_size) != 0) {
        perror("Erro ao construir arvore");
        return 1;
    }

    hash_tree_print_info(&tree);

    if (hash_tree_save(&tree, verity_path) != 0) {
        perror("Erro ao salvar .verity");
        hash_tree_free(&tree);
        return 1;
    }

    printf("Arvore salva em: %s\n", verity_path);
    hash_tree_free(&tree);
    return 0;
}
