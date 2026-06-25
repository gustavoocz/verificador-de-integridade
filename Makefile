# Makefile — Verificador de Integridade de Disco (Tema 14)
# Compilado com gcc -std=c11 -Wall -Wextra -Werror (sem warnings)

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -g -Isrc
BUILD   = build

# ── Fontes da biblioteca core (adicionar conforme E1/E2/E3 avançam) ───────────
LIB_SRCS = src/sha256.c
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRCS))

# ── Binários de teste ─────────────────────────────────────────────────────────
TEST_BINS = $(BUILD)/test_sha256

# ── Ferramentas (adicionar em E2/E3) ──────────────────────────────────────────
# TOOLS = mkverity verify_block corrupt

.PHONY: all test stress clean

# ── all ───────────────────────────────────────────────────────────────────────
all: $(BUILD) $(LIB_OBJS)
	@echo "Build OK (E1: sha256)"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── test ──────────────────────────────────────────────────────────────────────
test: $(BUILD) $(TEST_BINS)
	@echo "=== Bateria de testes ==="
	@$(BUILD)/test_sha256 && echo "[OK] test_sha256" || (echo "[FAIL] test_sha256"; exit 1)

$(BUILD)/test_sha256: tests/test_sha256.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# ── stress ────────────────────────────────────────────────────────────────────
stress:
	@echo "Teste de estresse disponivel apos E2 (hash_tree completa)."

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)/
	@echo "Limpo."
