/*
 * verify_block.c — Verifica a integridade de um bloco via Merkle tree
 *
 * Uso: verify_block <imagem> <arquivo.verity> <bloco>
 *   imagem          : arquivo de imagem de disco
 *   arquivo.verity  : arquivo de árvore gerado pelo mkverity
 *   bloco           : índice do bloco a verificar (0-based)
 *
 * Saída: 0 (OK) ou 1 (corrompido / erro)
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <imagem> <arquivo.verity> <bloco>\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }

    const char *image_path  = argv[1];
    const char *verity_path = argv[2];
    long        block_idx_l = atol(argv[3]);

    if (block_idx_l < 0) {
        fprintf(stderr, "Erro: indice de bloco invalido.\n");
        return 1;
    }
    uint64_t block_idx = (uint64_t)block_idx_l;

    /* Carregar árvore */
    hash_tree_t tree;
    if (hash_tree_load(&tree, verity_path) != 0) {
        perror("Erro ao carregar .verity");
        return 1;
    }

    if (block_idx >= tree.num_blocks) {
        fprintf(stderr, "Erro: bloco %lu fora do intervalo (0..%lu)\n",
                (unsigned long)block_idx,
                (unsigned long)(tree.num_blocks - 1));
        hash_tree_free(&tree);
        return 1;
    }

    /* Ler bloco da imagem */
    FILE *fp = fopen(image_path, "rb");
    if (!fp) {
        perror("Erro ao abrir imagem");
        hash_tree_free(&tree);
        return 1;
    }

    uint8_t *buf = (uint8_t *)malloc(tree.block_size);
    if (!buf) {
        perror("malloc");
        fclose(fp);
        hash_tree_free(&tree);
        return 1;
    }

    if (fseek(fp, (long)(block_idx * tree.block_size), SEEK_SET) != 0) {
        perror("fseek");
        free(buf);
        fclose(fp);
        hash_tree_free(&tree);
        return 1;
    }

    size_t n = fread(buf, 1, tree.block_size, fp);
    /* Bloco parcial (último): preencher restante com zeros */
    if (n < tree.block_size)
        memset(buf + n, 0, tree.block_size - n);
    fclose(fp);

    /* Verificar */
    int result = hash_tree_verify_block(&tree, buf, block_idx);

    if (result == 0) {
        printf("[OK]    Bloco %lu integro\n", (unsigned long)block_idx);
    } else {
        printf("[FALHA] Bloco %lu corrompido ou adulterado!\n",
               (unsigned long)block_idx);
    }

    free(buf);
    hash_tree_free(&tree);
    return (result == 0) ? 0 : 1;
}
