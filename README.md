# Verificador de Integridade de Disco — Tema 14

**Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro**
Disciplinas: Estrutura de Dados (C) × Sistemas Operacionais

---

## Descrição

Ferramenta que constrói uma **Merkle tree** (árvore de hash estilo dm-verity) sobre blocos de
uma imagem de disco, persiste a árvore e expõe uma **API de leitura verificada**: cada leitura
valida o caminho folha→raiz antes de entregar os dados, detectando adulteração e identificando
exatamente qual bloco foi corrompido.

### Estrutura de Dados (ED)
- Árvore binária de hash (Merkle tree) sobre todos os blocos da imagem
- Folhas: `SHA-256(bloco[i])`; nós internos: `SHA-256(filho_esq || filho_dir)`
- Verificação de um bloco exige apenas o caminho folha→raiz: **O(log n)** hashes

### Mecanismo de SO
- Camada de leitura verificada sobre arquivo de imagem somente-leitura
- Cache LRU dos nós de hash já verificados (reduz recomputação)

---

## Estrutura do repositório (planejada)

```
verificador-de-integridade/
├── src/
│   ├── sha256.h / sha256.c      # SHA-256 portável (sem deps externas)
│   ├── hash_tree.h / hash_tree.c # Merkle tree de blocos
│   ├── node_cache.h / node_cache.c # Cache LRU de nós (SO)
│   └── verity.h / verity.c      # Camada de leitura verificada
├── tools/
│   ├── mkverity.c               # Constrói e persiste a árvore
│   ├── verify_block.c           # Verifica um bloco específico
│   └── corrupt.c                # Corrompe byte para teste de fogo
├── tests/                       # Bateria de testes unitários e integração
├── scripts/                     # Scripts de benchmark e geração de gráficos
├── Makefile
├── DIARIO.md
└── README.md
```

---

## Build

```bash
make          # compila tudo
make test     # executa bateria de testes
make stress   # teste de estresse
make clean    # limpa artefatos
```

> Compilado com `gcc -std=c11 -Wall -Wextra -Werror`

---

## Cronograma

| Entrega | Escopo | Peso |
|---------|--------|------|
| E1 — Fundação | SHA-256, estruturas base, persistência, testes unitários | 15% |
| E2 — Núcleo ED | Árvore completa (build/verify/persist), caminho folha→raiz | 40% |
| E3 — Núcleo SO | Cache LRU, camada verity_read, benchmark | 10% |
| E4 — Robustez | Teste de fogo, relatório experimental, defesa | 35% |

**Data de apresentação: 02/07/2026**
