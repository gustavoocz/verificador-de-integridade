/*
 * corrupt.c — Corrompe um único byte de um bloco (ferramenta de teste de fogo)
 *
 * Uso: corrupt <imagem> <bloco> <offset> <valor_hex>
 *   imagem      : arquivo de imagem de disco (modificado in-place)
 *   bloco       : índice do bloco a corromper (0-based)
 *   offset      : offset do byte dentro do bloco (0-based)
 *   valor_hex   : novo valor do byte em hex (ex.: FF, 00, A3)
 *
 * ATENÇÃO: modifica o arquivo permanentemente. Use uma cópia para testes.
 *
 * Tema 14 — Verificador de Integridade de Disco
 * Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
 */

#include "../src/hash_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s <imagem> <bloco> <offset> <valor_hex>\n"
            "  Exemplo: %s disco.img 3 42 FF\n",
            prog, prog);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
        return 1;
    }

    const char *image_path = argv[1];
    long block_idx  = atol(argv[2]);
    long byte_off   = atol(argv[3]);
    unsigned int hex_val = 0;

    if (block_idx < 0 || byte_off < 0) {
        fprintf(stderr, "Erro: bloco e offset devem ser >= 0\n");
        return 1;
    }

    if (sscanf(argv[4], "%x", &hex_val) != 1 || hex_val > 0xFF) {
        fprintf(stderr, "Erro: valor hex invalido (%s). Use ex.: FF, 00, A3\n",
                argv[4]);
        return 1;
    }

    /* Precisamos saber o block_size — lemos o .verity se houver,
     * mas corrupt opera direto na imagem sem depender do .verity.
     * Por simplicidade, usamos 4096 como padrão ou o usuário deve
     * garantir que offset < block_size. */
    FILE *fp = fopen(image_path, "r+b");
    if (!fp) {
        perror("Erro ao abrir imagem para escrita");
        return 1;
    }

    long abs_offset = block_idx * 4096L + byte_off;

    if (fseek(fp, abs_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    /* Ler byte original */
    int original = fgetc(fp);
    if (original == EOF) {
        fprintf(stderr, "Erro: offset %ld fora do arquivo\n", abs_offset);
        fclose(fp);
        return 1;
    }

    /* Voltar e escrever novo valor */
    if (fseek(fp, abs_offset, SEEK_SET) != 0) {
        perror("fseek (write)");
        fclose(fp);
        return 1;
    }

    if (fputc((int)hex_val, fp) == EOF) {
        perror("fputc");
        fclose(fp);
        return 1;
    }

    fclose(fp);

    printf("Corrompido: bloco %ld, offset %ld: 0x%02X -> 0x%02X\n",
           block_idx, byte_off, original, hex_val);
    return 0;
}
