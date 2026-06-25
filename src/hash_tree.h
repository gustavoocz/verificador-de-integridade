/*
 * hash_tree.h — Interface da Merkle tree de blocos (estilo dm-verity)
 *
 * Árvore binária completa com num_leaves = próxima potência de 2 >= num_blocks.
 * Nós indexados em array BFS: raiz=0, filhos de i em 2i+1 e 2i+2.
 *
 * Formato do arquivo .verity:
 *   [magic "VERITY14": 8 B][version: 4 B][block_size: 4 B]
 *   [num_blocks: 8 B][root_hash: 32 B][nós BFS: num_nodes * 32 B]
 *
 * A ser implementada em hash_tree.c (E1 e E2).
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#ifndef HASH_TREE_H
#define HASH_TREE_H

#include "sha256.h"
#include <stdint.h>

#define VERITY_MAGIC      "VERITY14"
#define VERITY_MAGIC_SIZE 8
#define VERITY_VERSION    1

/* Representação em memória da árvore */
typedef struct {
    uint32_t block_size;
    uint64_t num_blocks;  /* blocos reais da imagem          */
    uint64_t num_leaves;  /* próxima potência de 2           */
    uint64_t num_nodes;   /* 2 * num_leaves - 1              */
    uint8_t  root_hash[SHA256_DIGEST_SIZE];
    uint8_t *nodes;       /* array BFS alocado dinamicamente */
} hash_tree_t;

/* Constrói a árvore sobre o arquivo de imagem */
int hash_tree_build(hash_tree_t *tree, const char *image_path, uint32_t block_size);

/* Persiste a árvore em arquivo .verity */
int hash_tree_save(const hash_tree_t *tree, const char *verity_path);

/* Carrega a árvore de um arquivo .verity */
int hash_tree_load(hash_tree_t *tree, const char *verity_path);

/* Verifica a integridade de um bloco (caminho folha→raiz) */
int hash_tree_verify_block(const hash_tree_t *tree,
                           const uint8_t     *block_data,
                           uint64_t           block_idx);

/* Retorna o hash do nó de índice BFS node_idx */
int hash_tree_get_node(const hash_tree_t *tree,
                       uint64_t           node_idx,
                       uint8_t            out[SHA256_DIGEST_SIZE]);

/* Libera memória da árvore */
void hash_tree_free(hash_tree_t *tree);

/* Imprime informações resumidas */
void hash_tree_print_info(const hash_tree_t *tree);

/* Utilitário: próxima potência de 2 >= n */
uint64_t next_power_of_two(uint64_t n);

#endif /* HASH_TREE_H */
