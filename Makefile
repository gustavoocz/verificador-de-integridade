# Makefile — Verificador de Integridade de Disco (Tema 14)
# Compilado com gcc -std=c11 -Wall -Wextra -Werror (sem warnings)

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -g -Isrc
BUILD   = build

# ── Fontes da biblioteca core ─────────────────────────────────────────────────
LIB_SRCS = src/sha256.c src/hash_tree.c
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRCS))

# ── Binários de teste ─────────────────────────────────────────────────────────
TEST_BINS = $(BUILD)/test_sha256 $(BUILD)/test_hash_tree

# ── Ferramentas (adicionar em E2/E3) ──────────────────────────────────────────
# TOOLS = mkverity verify_block corrupt

.PHONY: all test stress clean

# ── all ───────────────────────────────────────────────────────────────────────
all: $(BUILD) $(LIB_OBJS)
	@echo "Build OK (E1: sha256 + hash_tree)"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── test ──────────────────────────────────────────────────────────────────────
test: $(BUILD) $(TEST_BINS)
	@echo "=== Bateria de testes ==="
	@$(BUILD)/test_sha256    && echo "[OK] test_sha256"    || (echo "[FAIL] test_sha256";    exit 1)
	@$(BUILD)/test_hash_tree && echo "[OK] test_hash_tree" || (echo "[FAIL] test_hash_tree"; exit 1)

$(BUILD)/test_sha256: tests/test_sha256.c $(BUILD)/sha256.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_hash_tree: tests/test_hash_tree.c $(BUILD)/sha256.o $(BUILD)/hash_tree.o
	$(CC) $(CFLAGS) $^ -o $@

# ── stress ────────────────────────────────────────────────────────────────────
stress:
	@echo "Teste de estresse disponivel apos E2 (ferramentas mkverity/verify_block)."

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)/
	@echo "Limpo."
