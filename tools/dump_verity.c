/*
 * dump_verity.c — Ferramenta de inspeção/debug de arquivos .verity
 *
 * Uso: dump_verity <arquivo.verity> [--full]
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <arquivo.verity> [--full]\n", prog);
    fprintf(stderr, "  --full : exibe todos os hashes dos nos em ordem BFS\n");
}

int main(int argc, char *argv[]) {
    /* 1. Tratamento de --help e --version */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("verificador-integridade v1.0 (Tema 14 IFES 2026)\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 2;
    }

    const char *verity_path = argv[1];
    int full = (argc == 3 && strcmp(argv[2], "--full") == 0);

    if (argc == 3 && !full) {
        usage(argv[0]);
        return 2;
    }

    /* 2. Validar existencia e leitura do arquivo (Erro E/S -> codigo 1) */
    FILE *fp = fopen(verity_path, "rb");
    if (!fp) {
        perror("Erro ao abrir arquivo .verity");
        return 1;
    }
    fclose(fp);

    /* 3. Carregar arvore (Erro de formato -> codigo 2) */
    hash_tree_t tree;
    if (hash_tree_load(&tree, verity_path) != 0) {
        fprintf(stderr, "Erro: Arquivo .verity com formato invalido.\n");
        return 2;
    }

    /* 4. Exibir informacoes */
    if (!full) {
        /* Exibe apenas o cabecalho */
        printf("Magic:       %s\n", VERITY_MAGIC);
        printf("Versao:      %d\n", VERITY_VERSION);
        printf("Block size:  %u bytes\n", tree.block_size);
        printf("Num blocks:  %lu (blocos reais da imagem)\n", (unsigned long)tree.num_blocks);
        printf("Num folhas:  %lu (proxima potencia de 2)\n", (unsigned long)tree.num_leaves);
        printf("Num nos:     %lu (2 * folhas - 1)\n", (unsigned long)tree.num_nodes);
        printf("Root hash:   ");
        for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
            printf("%02x", tree.root_hash[i]);
        }
        printf("\n");
    } else {
        /* Exibe todos os hashes BFS */
        uint8_t hash_buf[SHA256_DIGEST_SIZE];
        for (uint64_t node_idx = 0; node_idx < tree.num_nodes; node_idx++) {
            const char *label;
            const char *spaces;

            if (node_idx == 0) {
                label = "raiz";
                spaces = "    "; /* 4 espacos */
            } else if (node_idx < tree.num_leaves - 1) {
                label = "interno";
                spaces = " ";    /* 1 espaco */
            } else {
                label = "folha";
                spaces = "   ";  /* 3 espacos */
            }

            printf("Node[%4lu] (%s):%s", (unsigned long)node_idx, label, spaces);

            if (hash_tree_get_node(&tree, node_idx, hash_buf) == 0) {
                for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
                    printf("%02x", hash_buf[i]);
                }
            } else {
                printf("<erro ao obter hash>");
            }
            printf("\n");
        }
    }

    hash_tree_free(&tree);
    return 0;
}
