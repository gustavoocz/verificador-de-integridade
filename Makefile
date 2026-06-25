# Makefile — Verificador de Integridade de Disco (Tema 14)
# Compilado com gcc -std=c11 -Wall -Wextra -Werror (sem warnings)

CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -Isrc

# TODO (E1): adicionar fontes conforme forem implementadas
LIB_SRCS =
LIB_OBJS =

.PHONY: all test stress clean

all:
	@echo "Nada compilado ainda — implemente os módulos em src/"

test:
	@echo "Bateria de testes ainda não implementada."

stress:
	@echo "Teste de estresse ainda não implementado."

clean:
	rm -rf build/
	@echo "Limpo."
