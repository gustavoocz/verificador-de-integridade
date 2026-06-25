# Makefile — Verificador de Integridade de Disco (Tema 14)
# Compilado com gcc -std=c11 -Wall -Wextra -Werror (sem warnings)

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -g -Isrc
BUILD   = build

# ── Biblioteca core ───────────────────────────────────────────────────────────
LIB_SRCS = src/sha256.c src/hash_tree.c
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRCS))

# ── Ferramentas CLI ───────────────────────────────────────────────────────────
TOOLS = $(BUILD)/mkverity $(BUILD)/verify_block $(BUILD)/corrupt

# ── Testes ────────────────────────────────────────────────────────────────────
TEST_BINS = $(BUILD)/test_sha256 $(BUILD)/test_hash_tree

.PHONY: all test stress clean

# ── all ───────────────────────────────────────────────────────────────────────
all: $(BUILD) $(LIB_OBJS) $(TOOLS)
	@echo "Build OK (E1/E2: sha256 + hash_tree + ferramentas)"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Ferramentas ───────────────────────────────────────────────────────────────
$(BUILD)/mkverity: tools/mkverity.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/verify_block: tools/verify_block.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/corrupt: tools/corrupt.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# ── test ──────────────────────────────────────────────────────────────────────
test: $(BUILD) $(TEST_BINS)
	@echo "=== Bateria de testes ==="
	@$(BUILD)/test_sha256    && echo "[OK] test_sha256"    || (echo "[FAIL] test_sha256";    exit 1)
	@$(BUILD)/test_hash_tree && echo "[OK] test_hash_tree" || (echo "[FAIL] test_hash_tree"; exit 1)

$(BUILD)/test_sha256: tests/test_sha256.c $(BUILD)/sha256.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_hash_tree: tests/test_hash_tree.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# ── stress ────────────────────────────────────────────────────────────────────
stress: $(BUILD) $(TOOLS)
	@echo "=== Teste de estresse ==="
	@echo "Gerando imagem de 16 MB..."
	dd if=/dev/urandom of=$(BUILD)/stress.img bs=4096 count=4096 2>/dev/null
	@echo "Construindo arvore..."
	$(BUILD)/mkverity $(BUILD)/stress.img $(BUILD)/stress.verity
	@echo "Verificando bloco 0..."
	$(BUILD)/verify_block $(BUILD)/stress.img $(BUILD)/stress.verity 0
	@echo "Corrompendo bloco 100..."
	$(BUILD)/corrupt $(BUILD)/stress.img 100 0 FF
	@echo "Bloco 100 deve falhar:"
	$(BUILD)/verify_block $(BUILD)/stress.img $(BUILD)/stress.verity 100 ; true
	@echo "Bloco 0 deve passar:"
	$(BUILD)/verify_block $(BUILD)/stress.img $(BUILD)/stress.verity 0
	rm -f $(BUILD)/stress.img $(BUILD)/stress.verity
	@echo "Teste de estresse concluido."

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)/
	@echo "Limpo."
