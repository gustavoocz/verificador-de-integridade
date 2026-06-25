/*
 * hash_tree.c — Merkle tree de blocos de disco (estilo dm-verity)
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "hash_tree.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Utilitários internos ─────────────────────────────────────────────────── */

uint64_t next_power_of_two(uint64_t n) {
    if (n == 0) return 1;
    if ((n & (n - 1)) == 0) return n;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

/* Índice BFS da folha do bloco b */
static inline uint64_t leaf_idx(const hash_tree_t *tree, uint64_t b) {
    return (tree->num_leaves - 1) + b;
}

/* Ponteiro para o hash do nó de índice BFS i */
static inline uint8_t *node_ptr(const hash_tree_t *tree, uint64_t i) {
    return tree->nodes + i * SHA256_DIGEST_SIZE;
}

/* ── Construção ────────────────────────────────────────────────────────────── */

int hash_tree_build(hash_tree_t *tree, const char *image_path,
                    uint32_t block_size) {
    FILE    *fp        = NULL;
    uint8_t *block_buf = NULL;
    uint8_t *zero_buf  = NULL;
    int      ret       = -1;

    if (!tree || !image_path || block_size == 0) {
        errno = EINVAL;
        return -1;
    }

    fp = fopen(image_path, "rb");
    if (!fp) return -1;

    /* Tamanho da imagem */
    if (fseek(fp, 0, SEEK_END) != 0) goto done;
    long file_size = ftell(fp);
    if (file_size < 0) goto done;
    rewind(fp);

    uint64_t num_blocks = ((uint64_t)file_size + block_size - 1) / block_size;
    if (num_blocks == 0) {
        fprintf(stderr, "hash_tree_build: imagem vazia\n");
        errno = EINVAL;
        goto done;
    }

    uint64_t num_leaves = next_power_of_two(num_blocks);
    uint64_t num_nodes  = 2 * num_leaves - 1;

    tree->block_size = block_size;
    tree->num_blocks = num_blocks;
    tree->num_leaves = num_leaves;
    tree->num_nodes  = num_nodes;

    tree->nodes = (uint8_t *)calloc(num_nodes, SHA256_DIGEST_SIZE);
    if (!tree->nodes) goto done;

    block_buf = (uint8_t *)malloc(block_size);
    if (!block_buf) goto done;

    /* Hash de um bloco de zeros (usado para folhas de padding) */
    zero_buf = (uint8_t *)calloc(1, block_size);
    if (!zero_buf) goto done;

    uint8_t zero_hash[SHA256_DIGEST_SIZE];
    sha256(zero_buf, block_size, zero_hash);

    /* Fase 1: hashes das folhas */
    for (uint64_t b = 0; b < num_leaves; b++) {
        uint64_t idx = leaf_idx(tree, b);
        if (b < num_blocks) {
            size_t n = fread(block_buf, 1, block_size, fp);
            /* Bloco parcial (último): preencher com zeros */
            if (n < block_size)
                memset(block_buf + n, 0, block_size - n);
            sha256(block_buf, block_size, node_ptr(tree, idx));
        } else {
            /* Bloco de padding virtual */
            memcpy(node_ptr(tree, idx), zero_hash, SHA256_DIGEST_SIZE);
        }
    }

    /* Fase 2: nós internos, de baixo para cima */
    {
        uint8_t concat[SHA256_DIGEST_SIZE * 2];
        for (int64_t i = (int64_t)(num_leaves - 2); i >= 0; i--) {
            uint64_t ui   = (uint64_t)i;
            uint64_t left  = 2 * ui + 1;
            uint64_t right = 2 * ui + 2;
            memcpy(concat,                      node_ptr(tree, left),  SHA256_DIGEST_SIZE);
            memcpy(concat + SHA256_DIGEST_SIZE, node_ptr(tree, right), SHA256_DIGEST_SIZE);
            sha256(concat, (size_t)SHA256_DIGEST_SIZE * 2, node_ptr(tree, ui));
        }
    }

    /* Raiz */
    memcpy(tree->root_hash, node_ptr(tree, 0), SHA256_DIGEST_SIZE);
    ret = 0;

done:
    if (fp)       fclose(fp);
    if (block_buf) free(block_buf);
    if (zero_buf)  free(zero_buf);
    if (ret != 0 && tree->nodes) {
        free(tree->nodes);
        tree->nodes = NULL;
    }
    return ret;
}

/* ── Persistência ──────────────────────────────────────────────────────────── */

int hash_tree_save(const hash_tree_t *tree, const char *verity_path) {
    if (!tree || !verity_path || !tree->nodes) {
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(verity_path, "wb");
    if (!fp) return -1;

    int ret = -1;

    /* Magic */
    if (fwrite(VERITY_MAGIC, 1, VERITY_MAGIC_SIZE, fp) != VERITY_MAGIC_SIZE)
        goto done;

    /* Cabeçalho */
    uint32_t version = VERITY_VERSION;
    if (fwrite(&version,          sizeof(version),          1, fp) != 1) goto done;
    if (fwrite(&tree->block_size, sizeof(tree->block_size), 1, fp) != 1) goto done;
    if (fwrite(&tree->num_blocks, sizeof(tree->num_blocks), 1, fp) != 1) goto done;
    if (fwrite(tree->root_hash,   1, SHA256_DIGEST_SIZE,    fp) != SHA256_DIGEST_SIZE)
        goto done;

    /* Array BFS de hashes */
    size_t total = (size_t)tree->num_nodes * SHA256_DIGEST_SIZE;
    if (fwrite(tree->nodes, 1, total, fp) != total) goto done;

    ret = 0;

done:
    fclose(fp);
    return ret;
}

int hash_tree_load(hash_tree_t *tree, const char *verity_path) {
    if (!tree || !verity_path) {
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(verity_path, "rb");
    if (!fp) return -1;

    int ret = -1;
    memset(tree, 0, sizeof(*tree));

    /* Verificar magic */
    char magic[VERITY_MAGIC_SIZE];
    if (fread(magic, 1, VERITY_MAGIC_SIZE, fp) != VERITY_MAGIC_SIZE) goto done;
    if (memcmp(magic, VERITY_MAGIC, VERITY_MAGIC_SIZE) != 0) {
        fprintf(stderr, "hash_tree_load: magic invalido\n");
        errno = EINVAL;
        goto done;
    }

    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1) goto done;
    if (version != VERITY_VERSION) {
        fprintf(stderr, "hash_tree_load: versao %u nao suportada\n", version);
        errno = EINVAL;
        goto done;
    }

    if (fread(&tree->block_size, sizeof(tree->block_size), 1, fp) != 1) goto done;
    if (fread(&tree->num_blocks, sizeof(tree->num_blocks), 1, fp) != 1) goto done;
    if (fread(tree->root_hash, 1, SHA256_DIGEST_SIZE, fp) != SHA256_DIGEST_SIZE)
        goto done;

    tree->num_leaves = next_power_of_two(tree->num_blocks);
    tree->num_nodes  = 2 * tree->num_leaves - 1;

    tree->nodes = (uint8_t *)malloc((size_t)tree->num_nodes * SHA256_DIGEST_SIZE);
    if (!tree->nodes) goto done;

    size_t total = (size_t)tree->num_nodes * SHA256_DIGEST_SIZE;
    if (fread(tree->nodes, 1, total, fp) != total) goto done;

    ret = 0;

done:
    fclose(fp);
    if (ret != 0 && tree->nodes) {
        free(tree->nodes);
        tree->nodes = NULL;
    }
    return ret;
}

/* ── Verificação ────────────────────────────────────────────────────────────── */

int hash_tree_verify_block(const hash_tree_t *tree,
                           const uint8_t     *block_data,
                           uint64_t           block_idx) {
    if (!tree || !block_data || !tree->nodes || block_idx >= tree->num_blocks) {
        errno = EINVAL;
        return -1;
    }

    /* 1. Hash da folha a partir dos dados do bloco */
    uint8_t current[SHA256_DIGEST_SIZE];
    sha256(block_data, tree->block_size, current);

    /* 2. Comparar com a folha armazenada na árvore */
    uint64_t idx = leaf_idx(tree, block_idx);
    if (memcmp(current, node_ptr(tree, idx), SHA256_DIGEST_SIZE) != 0) {
        fprintf(stderr,
                "[FALHA] Bloco %lu: hash da folha nao confere\n",
                (unsigned long)block_idx);
        return -1;
    }

    /* 3. Subir o caminho folha→raiz verificando cada nó interno */
    uint8_t concat[SHA256_DIGEST_SIZE * 2];
    uint8_t parent_computed[SHA256_DIGEST_SIZE];

    while (idx > 0) {
        uint64_t par = (idx - 1) / 2;
        uint64_t sib = (idx % 2 == 1) ? idx + 1 : idx - 1;

        /* Ordem: filho esquerdo (índice ímpar) || filho direito (índice par) */
        if (idx % 2 == 1) { /* sou filho esquerdo */
            memcpy(concat,                      current,              SHA256_DIGEST_SIZE);
            memcpy(concat + SHA256_DIGEST_SIZE, node_ptr(tree, sib), SHA256_DIGEST_SIZE);
        } else {             /* sou filho direito */
            memcpy(concat,                      node_ptr(tree, sib), SHA256_DIGEST_SIZE);
            memcpy(concat + SHA256_DIGEST_SIZE, current,              SHA256_DIGEST_SIZE);
        }

        sha256(concat, (size_t)SHA256_DIGEST_SIZE * 2, parent_computed);

        if (memcmp(parent_computed, node_ptr(tree, par), SHA256_DIGEST_SIZE) != 0) {
            fprintf(stderr,
                    "[FALHA] Bloco %lu: no interno %lu nao confere\n",
                    (unsigned long)block_idx, (unsigned long)par);
            return -1;
        }

        memcpy(current, parent_computed, SHA256_DIGEST_SIZE);
        idx = par;
    }

    /* 4. Verificar raiz */
    if (memcmp(current, tree->root_hash, SHA256_DIGEST_SIZE) != 0) {
        fprintf(stderr, "[FALHA] Bloco %lu: root hash nao confere\n",
                (unsigned long)block_idx);
        return -1;
    }

    return 0;
}

int hash_tree_get_node(const hash_tree_t *tree,
                       uint64_t           node_idx,
                       uint8_t            out[SHA256_DIGEST_SIZE]) {
    if (!tree || !out || !tree->nodes || node_idx >= tree->num_nodes) {
        errno = EINVAL;
        return -1;
    }
    memcpy(out, node_ptr(tree, node_idx), SHA256_DIGEST_SIZE);
    return 0;
}

/* ── Utilitários ────────────────────────────────────────────────────────────── */

void hash_tree_free(hash_tree_t *tree) {
    if (tree) {
        free(tree->nodes);
        memset(tree, 0, sizeof(*tree));
    }
}

void hash_tree_print_info(const hash_tree_t *tree) {
    if (!tree) return;
    printf("=== Merkle Tree (dm-verity) ===\n");
    printf("Tamanho do bloco : %u bytes\n",  tree->block_size);
    printf("Blocos reais     : %lu\n",  (unsigned long)tree->num_blocks);
    printf("Folhas (pot. 2)  : %lu\n",  (unsigned long)tree->num_leaves);
    printf("Total de nos     : %lu\n",   (unsigned long)tree->num_nodes);
    printf("Root hash        : ");
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
        printf("%02x", tree->root_hash[i]);
    printf("\n");
}
