# Relatório Experimental — Verificador de Integridade de Disco
## Tema 14 · Trabalho Interdisciplinar ED × SO
### Ifes Cachoeiro de Itapemirim · 2026/1

---

## 1. Introdução

Este relatório documenta os experimentos realizados no projeto **Verificador de Integridade de Disco**, que implementa um mecanismo análogo ao Linux dm-verity para detectar corrupção silenciosa em imagens de disco.

O sistema utiliza uma **Merkle tree** (árvore de hash binária completa) cujos hashes de folha correspondem ao SHA-256 de cada bloco da imagem. A verificação de um bloco exige percorrer o caminho `folha → raiz` (custo O(log n) de recálculos de SHA-256). Para reduzir esse custo em acessos repetidos, integrou-se um **cache LRU de nós** que armazena hashes de nós internos já verificados, permitindo parada antecipada no caminho.

---

## 2. Metodologia

### 2.1 Ambiente

| Item | Valor |
|---|---|
| Sistema operacional | Windows 11 |
| Compilador | TDM-GCC 4.9.2 (x86_64) |
| Flags | `-std=c11 -Wall -Wextra -Werror -g` |
| Medição de tempo | `clock()` (tempo de CPU) |

### 2.2 Parâmetros do experimento

| Parâmetro | Valor |
|---|---|
| Tamanho da imagem | 128 blocos × 4096 bytes = **512 KB** |
| Profundidade da árvore | log₂(128) = **7 níveis** |
| Total de nós na árvore | 2 × 128 − 1 = **255 nós** |
| Capacidades de cache testadas | 0, 8, 16, 32, 64, 128, 256 |
| Passes por configuração | 2 (1º passo = cache frio; 2º passo = cache quente) |
| Conteúdo da imagem | bytes aleatórios (`os.urandom`) |

### 2.3 Protocolo

Para cada capacidade de cache (`cache_cap`):
1. Abrir imagem + carregar árvore (`verity_open`)
2. **Passe 1** (cache frio): ler todos os 128 blocos sequencialmente, registrar `hash_computations` e `hit_ratio`
3. **Passe 2** (cache quente): repetir a leitura com o mesmo contexto (cache populado do passe 1), registrar os mesmos contadores
4. Fechar contexto

O benchmark é reproduzível via:
```powershell
.\scripts\run_benchmark.ps1 -Blocks 128 -BlockSize 4096 -Passes 2
```

---

## 3. Resultados

### 3.1 Hash computations por passe

| cache\_cap | Passe 1 — Hashes | Passe 2 — Hashes | Redução P1→P2 | Hit Ratio P2 |
|---:|---:|---:|---:|---:|
| 0 (sem cache) | 128 | 128 | 0 % | 0,00 % |
| 8 | 640 | 640 | 0 % | 12,50 % |
| 16 | 416 | 416 | 0 % | 38,89 % |
| 32 | 392 | 392 | 0 % | 45,45 % |
| 64 | 384 | 384 | 0 % | 48,44 % |
| 128 | 382 | 382 | 0 % | 49,61 % |
| **256** | **382** | **256** | **−33 %** | **100,00 %** |

> **Observação:** com `cache_cap ≥ 255` (igual ao total de nós da árvore), todos os nós internos são armazenados no passe 1. No passe 2, cada leitura de bloco encontra imediatamente o nó pai no cache e para — caindo de 382 para 256 hash computations (apenas os hashes das folhas, 1 por bloco).

### 3.2 Interpretação por faixa de cache

| Faixa | Comportamento |
|---|---|
| `cache_cap = 0` | Sem caminho para o cache; apenas 1 hash por bloco (folha). Caminho de verificação completo não é realizado — modo "light". |
| `0 < cache_cap < 128` | Cache parcial: hits crescentes conforme blocos compartilham nós internos na segunda varredura. |
| `cache_cap ≥ 255` | Cache total: no 2º passe, apenas as folhas são recalculadas (1 hash por bloco). Redução de 33 % nas computações. |

### 3.3 Teste de fogo — corrupção de 1 byte

| Cenário | Resultado |
|---|---|
| 128 blocos íntegros → verify | ✅ 128/128 passaram |
| 1 byte do bloco 10 corrompido → verify bloco 10 | ✅ Falha detectada |
| Demais 127 blocos verificados após corrupção | ✅ 127/127 passaram |
| Ferramenta `corrupt` + `verify_block` | Detecta corrupção e preserva os demais blocos |

Reproduzível via:
```powershell
.\scripts\stress.ps1 -Blocks 64 -CorruptBlock 10
```

---

## 4. Análise

### 4.1 Por que `cache_cap = 0` tem menos hashes que cache habilitado?

No modo sem cache, `verity_read` calcula apenas `SHA-256(bloco)` e compara com a folha — **1 hash por leitura**. O caminho completo até a raiz não é percorrido.

Com cache habilitado, o primeiro passe percorre o caminho completo para popular o cache, custando **até log₂(128) + 1 = 8 hashes por bloco** no primeiro acesso. O benefício do cache aparece nos acessos subsequentes ao segundo passe.

| Modo | Hashes/bloco (1º passe) | Hashes/bloco (2º passe) |
|---|---|---|
| Sem cache | 1 | 1 |
| Com cache (cap ≥ 255) | ~3 em média | 1 (só folha) |

**Conclusão:** o cache LRU é benéfico em cenários com múltiplas leituras repetidas de blocos pertencentes à mesma subárvore; em leitura única sequencial, o modo sem cache é mais eficiente.

### 4.2 Tradeoff memória × computação

| cache\_cap | Memória adicional | Hashes poupados (2º passe) |
|---:|---:|---:|
| 0 | 0 bytes | 0 |
| 32 | ~1 KB | não mensurável (< total de nós) |
| 255 | ~8 KB | 126 hashes (−33 %) |

Para a imagem de 512 KB, o cache total ocupa apenas **8 KB** — custo de memória negligenciável em relação ao benefício.

### 4.3 Política LRU — adequação ao padrão de acesso

A política LRU é adequada para acessos sequenciais com localidade temporal: ao ler os blocos em ordem, os nós internos de nível mais alto (raiz, sub-raízes) são reutilizados por múltiplos blocos e permanecem no cache. Nós de folha têm reuso nulo em acesso sequencial, porém são úteis em padrões de releitura do mesmo bloco.

---

## 5. Conclusão

O sistema implementado atinge os objetivos do Tema 14:

1. **SHA-256 portável** (FIPS 180-4): implementação manual validada por vetores de teste oficiais.
2. **Merkle tree funcional**: construção O(n), persistência em arquivo `.verity`, verificação O(log n).
3. **Cache LRU eficaz**: com capacidade igual ao número de nós internos, o 2º passe reduz hashes em **33 %** (de 382 para 256 para 128 blocos).
4. **Teste de fogo aprovado**: corrupção de 1 byte detectada; blocos íntegros permanecem legíveis.
5. **Zero warnings** em toda a base de código (`-Wall -Wextra -Werror`).
6. **137 testes unitários** cobrindo SHA-256, árvore, cache LRU e camada `verity_read`.

---

## 6. Como reproduzir

```powershell
# 1. Compilar tudo
make all

# 2. Rodar bateria de testes
make test

# 3. Teste de fogo (64 blocos, corrompe bloco 10)
.\scripts\stress.ps1

# 4. Benchmark completo (128 blocos, cache 0..256)
.\scripts\run_benchmark.ps1

# 5. Uso manual
.\build\mkverity disco.img disco.verity
.\build\verify_block disco.img disco.verity 0
.\build\corrupt disco.img 3 42 FF
.\build\verify_block disco.img disco.verity 3
```
