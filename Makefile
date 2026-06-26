# Makefile — Verificador de Integridade de Disco (Tema 14)
# Compilado com gcc -std=c11 -Wall -Wextra -Werror (sem warnings)

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -g -Isrc
BUILD   = build

# ── Biblioteca core ───────────────────────────────────────────────────────────
LIB_SRCS = src/sha256.c src/hash_tree.c src/node_cache.c src/verity.c
LIB_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRCS))

# ── Ferramentas CLI ───────────────────────────────────────────────────────────
TOOLS = $(BUILD)/mkverity $(BUILD)/verify_block $(BUILD)/corrupt $(BUILD)/bench $(BUILD)/scan

# ── Testes ────────────────────────────────────────────────────────────────────
TEST_BINS = $(BUILD)/test_sha256 $(BUILD)/test_hash_tree $(BUILD)/test_node_cache $(BUILD)/test_verity $(BUILD)/test_integration

.PHONY: all test stress clean memcheck

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

$(BUILD)/bench: tools/bench.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/scan: tools/scan.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# ── test ──────────────────────────────────────────────────────────────────────
test: $(BUILD) $(TEST_BINS)
	@echo "=== Bateria de testes ==="
	@$(BUILD)/test_sha256     && echo "[OK] test_sha256"     || (echo "[FAIL] test_sha256";     exit 1)
	@$(BUILD)/test_hash_tree  && echo "[OK] test_hash_tree"  || (echo "[FAIL] test_hash_tree";  exit 1)
	@$(BUILD)/test_node_cache && echo "[OK] test_node_cache" || (echo "[FAIL] test_node_cache"; exit 1)
	@$(BUILD)/test_verity     && echo "[OK] test_verity"     || (echo "[FAIL] test_verity";     exit 1)
	@$(BUILD)/test_integration && echo "[OK] test_integration" || (echo "[FAIL] test_integration"; exit 1)

$(BUILD)/test_sha256: tests/test_sha256.c $(BUILD)/sha256.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_hash_tree: tests/test_hash_tree.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_node_cache: tests/test_node_cache.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_verity: tests/test_verity.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/test_integration: tests/test_integration.c $(LIB_OBJS)
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

# ── memcheck ──────────────────────────────────────────────────────────────────
# ATENÇÃO: TDM-GCC 4.9.2 no Windows NÃO suporta ASAN. Use WSL ou Linux para este target.
memcheck:
	@echo "=== Compilando testes com AddressSanitizer e UBSan ==="
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -fsanitize=address,undefined src/sha256.c tests/test_sha256.c -o $(BUILD)/test_sha256_asan
	$(CC) $(CFLAGS) -fsanitize=address,undefined $(LIB_SRCS) tests/test_hash_tree.c -o $(BUILD)/test_hash_tree_asan
	$(CC) $(CFLAGS) -fsanitize=address,undefined $(LIB_SRCS) tests/test_node_cache.c -o $(BUILD)/test_node_cache_asan
	$(CC) $(CFLAGS) -fsanitize=address,undefined $(LIB_SRCS) tests/test_verity.c -o $(BUILD)/test_verity_asan
	@echo "=== Rodando testes com AddressSanitizer e UBSan ==="
	$(BUILD)/test_sha256_asan
	$(BUILD)/test_hash_tree_asan
	$(BUILD)/test_node_cache_asan
	$(BUILD)/test_verity_asan

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)/
	@echo "Limpo."
