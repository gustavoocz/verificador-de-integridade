/*
 * test_integration.c — Teste de integração end-to-end em C puro
 *
 * Simula o ciclo de vida completo:
 * 1. Criação de imagem temporária com padrão determinístico
 * 2. Construção da Merkle Tree
 * 3. Persistência no disco (.verity)
 * 4. Recarregamento do arquivo persistido
 * 5. Verificação de integridade de todos os blocos (deve passar)
 * 6. Injeção de corrupção controlada de 1 byte
 * 7. Detecção da corrupção no bloco alterado
 * 8. Confirmação de que os demais blocos permanecem íntegros
 * 9. Remoção dos arquivos temporários
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  [FAIL] linha %d: %s\n", __LINE__, (msg)); \
    } else { \
        tests_passed++; \
        printf("  [OK]   %s\n", (msg)); \
    } \
} while (0)

int main(void) {
    const char *img_path = "build/test_integ.img";
    const char *verity_path = "build/test_integ.verity";
    uint32_t block_size = 4096;
    uint32_t num_blocks = 16;
    int build_ok, save_ok, load_ok;

    printf("=== Teste de Integracao End-to-End ===\n\n");

    /* 1. Criar imagem temporaria */
    FILE *img_fp = fopen(img_path, "wb");
    if (!img_fp) {
        perror("Erro ao criar build/test_integ.img");
        return 1;
    }

    uint8_t *temp_block = (uint8_t *)malloc(block_size);
    if (!temp_block) {
        perror("Erro ao alocar temp_block");
        fclose(img_fp);
        return 1;
    }

    for (uint32_t b = 0; b < num_blocks; b++) {
        for (uint32_t i = 0; i < block_size; i++) {
            temp_block[i] = (uint8_t)(i % 256);
        }
        fwrite(temp_block, 1, block_size, img_fp);
    }
    fclose(img_fp);
    free(temp_block);

    /* 2. Construir arvore */
    hash_tree_t tree;
    build_ok = hash_tree_build(&tree, img_path, block_size);
    ASSERT(build_ok == 0, "Construcao da Merkle Tree com sucesso");

    /* 3. Salvar arvore */
    save_ok = hash_tree_save(&tree, verity_path);
    ASSERT(save_ok == 0, "Merkle Tree salva no disco com sucesso");
    hash_tree_free(&tree);

    /* 4. Carregar arvore recem-salva */
    hash_tree_t loaded_tree;
    load_ok = hash_tree_load(&loaded_tree, verity_path);
    ASSERT(load_ok == 0, "Merkle Tree recarregada do disco com sucesso");

    /* Alocar buffer para leitura dos blocos */
    uint8_t *block_buf = (uint8_t *)malloc(block_size);
    if (!block_buf) {
        perror("Erro ao alocar block_buf");
        hash_tree_free(&loaded_tree);
        return 1;
    }

    /* 5. Verificar blocos integros */
    FILE *read_img_fp = fopen(img_path, "rb");
    if (!read_img_fp) {
        perror("Erro ao abrir imagem para leitura");
        free(block_buf);
        hash_tree_free(&loaded_tree);
        return 1;
    }

    for (uint64_t b = 0; b < num_blocks; b++) {
        fseek(read_img_fp, (long)b * block_size, SEEK_SET);
        size_t n = fread(block_buf, 1, block_size, read_img_fp);
        if (n == block_size) {
            int verify_ret = hash_tree_verify_block(&loaded_tree, block_buf, b);
            ASSERT(verify_ret == 0, "Bloco integro verificado com sucesso");
        } else {
            ASSERT(0, "Falha ao ler bloco integro da imagem");
        }
    }
    fclose(read_img_fp);

    /* 6. Corromper 1 byte do bloco 5 */
    FILE *corrupt_fp = fopen(img_path, "r+b");
    if (!corrupt_fp) {
        perror("Erro ao abrir imagem para corrupcao");
        free(block_buf);
        hash_tree_free(&loaded_tree);
        return 1;
    }
    fseek(corrupt_fp, (long)(5 * block_size), SEEK_SET);
    uint8_t corrupted_byte = 0xFF;
    fwrite(&corrupted_byte, 1, 1, corrupt_fp);
    fclose(corrupt_fp);

    /* 7. Verificar bloco 5 (deve falhar) */
    read_img_fp = fopen(img_path, "rb");
    if (!read_img_fp) {
        perror("Erro ao abrir imagem pos-corrupcao");
        free(block_buf);
        hash_tree_free(&loaded_tree);
        return 1;
    }

    fseek(read_img_fp, (long)(5 * block_size), SEEK_SET);
    size_t n_read = fread(block_buf, 1, block_size, read_img_fp);
    if (n_read == block_size) {
        int verify_ret_5 = hash_tree_verify_block(&loaded_tree, block_buf, 5);
        ASSERT(verify_ret_5 != 0, "Falha detectada corretamente no bloco 5 corrompido");
    } else {
        ASSERT(0, "Falha ao ler bloco 5 corrompido");
    }

    /* 8. Verificar demais blocos 0..4 e 6..15 (devem continuar integros) */
    for (uint64_t b = 0; b < num_blocks; b++) {
        if (b == 5) continue;
        fseek(read_img_fp, (long)b * block_size, SEEK_SET);
        size_t n = fread(block_buf, 1, block_size, read_img_fp);
        if (n == block_size) {
            int verify_ret = hash_tree_verify_block(&loaded_tree, block_buf, b);
            ASSERT(verify_ret == 0, "Bloco permanece integro pos-corrupcao de vizinho");
        } else {
            ASSERT(0, "Falha ao ler bloco integro pos-corrupcao");
        }
    }
    fclose(read_img_fp);

    /* Liberacao de recursos */
    free(block_buf);
    hash_tree_free(&loaded_tree);

    /* 9. Remover arquivos temporarios */
    remove(img_path);
    remove(verity_path);

    /* 10. Imprimir resultado final */
    printf("\ntest_integration: %lu/%lu assertions passed\n",
           (unsigned long)tests_passed, (unsigned long)tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
