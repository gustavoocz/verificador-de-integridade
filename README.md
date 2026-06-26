# Tema 14 — Verificador de Integridade de Disco (Merkle Tree / dm-verity)

[![build](https://img.shields.io/badge/build-passing-brightgreen)](#)
[![lang](https://img.shields.io/badge/linguagem-C11-blue)](#)

Este projeto implementa um **Verificador de Integridade de Disco** baseado em **Merkle Trees** (árvores de hash), simulando o comportamento do módulo de segurança **dm-verity** do Kernel Linux. Desenvolvido em **C11** como Trabalho Interdisciplinar integrando as disciplinas de **Estrutura de Dados** e **Sistemas Operacionais** do IFES Campus Cachoeiro.

---

## 📖 Descrição

O sistema cria uma estrutura de árvore criptográfica sobre os blocos de dados de uma imagem de disco somente leitura para garantir que nenhuma modificação física ou corrupção de dados passe despercebida. 

Ao ler qualquer bloco de dados:
1. O caminho na árvore de hash do bloco (folha) até a raiz dourada (*root hash*) é verificado de forma ascendente.
2. A complexidade dessa validação é **O(log n)** em termos de operações de hash, em vez do custo linear de verificar o arquivo inteiro.
3. Para otimizar a performance, um **Cache LRU** de nós é utilizado na verificação das subárvores compartilhadas, minimizando recomputações em acessos subsequentes.

---

## 🏛️ Arquitetura do Sistema

O projeto é modularizado em 4 componentes centrais:

1. **`sha256` (Criptografia):** Implementação portável e independente da especificação SHA-256 (FIPS PUB 180-4), com suporte a atualizações de hash parciais.
2. **`hash_tree` (Estrutura de Dados):** Criação, persistência e validação da Merkle Tree. Armazena os nós sequencialmente no arquivo em formato BFS (Breadth-First Search) para permitir posicionamento direto via `fseek` em complexidade **O(1)**.
3. **`node_cache` (Políticas de SO):** Cache LRU de nós implementado como uma tabela hash de endereçamento aberto (sondagem linear e *tombstones*) integrada a uma lista duplamente encadeada. Permite inserções e acessos em tempo esperado **O(1)**.
4. **`verity` (E/S Verificada):** Camada de leitura integrada que orquestra a leitura da imagem e a árvore de verificação, com algoritmo de parada antecipada no primeiro ancestral quente do cache.

---

## 📁 Estrutura do Repositório

```
verificador-de-integridade/
├── src/
│   ├── sha256.h / sha256.c        # Módulo SHA-256 portável (FIPS 180-4)
│   ├── hash_tree.h / hash_tree.c  # Construção, persistência e validação da Merkle Tree
│   ├── node_cache.h / node_cache.c # Cache LRU de nós com Hash Table + Doubly Linked List
│   └── verity.h / verity.c        # Camada verity_read com parada antecipada
├── tools/
│   ├── mkverity.c                 # Constrói e persiste o arquivo .verity de uma imagem
│   ├── verify_block.c             # Verifica a integridade de um bloco individual de disco
│   ├── corrupt.c                  # Modifica um byte arbitrário de um bloco para simular corrupções
│   ├── scan.c                     # Varre todos os blocos de uma imagem e lista os corrompidos
│   ├── dump_verity.c              # Inspeciona cabeçalho e hashes do arquivo .verity
│   └── bench.c                    # Coleta de métricas e performance de hits de cache
├── tests/
│   ├── test_sha256.c              # Suíte de testes unitários do SHA-256
│   ├── test_hash_tree.c           # Suíte de testes da árvore e caminhos O(log n)
│   ├── test_node_cache.c          # Suíte de testes das políticas de cache LRU
│   └── test_verity.c              # Suíte de testes da leitura verificada integrada
├── scripts/
│   ├── stress.ps1                 # Automação do teste de fogo em PowerShell
│   └── run_benchmark.ps1          # Coleta e tabulação de dados de benchmark
├── Makefile                       # Automação do build completo
├── DIARIO.md                      # Diário de bordo das decisões e IA
├── RELATORIO.md                   # Análise experimental dos resultados
└── README.md                      # Esta documentação
```

---

## 🛠️ Como Compilar e Usar

A compilação do projeto é automatizada via `Makefile`. Recomenda-se rodar em ambiente Unix-like (WSL, Linux ou Git Bash no Windows).

```bash
# Compilar todas as ferramentas e bibliotecas (all)
make all

# Executar a bateria com os 137 testes unitários
make test

# Executar o teste de estresse end-to-end (all + ferramentas + simulação)
make stress

# Limpar binários e arquivos da pasta build/
make clean

# Compilar e executar testes instrumentados com sanitizers (WSL ou Linux)
make memcheck
```

---

## 🎛️ Ferramentas CLI

### 1. `mkverity`
Gera a Merkle Tree de uma imagem de disco e a salva em formato `.verity`.
```bash
build/mkverity <caminho_imagem> <caminho_verity> [block_size]
# Exemplo:
build/mkverity disco.img disco.verity 4096
```

### 2. `verify_block`
Verifica a integridade de um bloco de dados específico.
```bash
build/verify_block <caminho_imagem> <caminho_verity> <numero_bloco>
# Exemplo:
build/verify_block disco.img disco.verity 10
```

### 3. `corrupt`
Modifica arbitrariamente um byte de dados para testar mecanismos de detecção de falha.
```bash
build/corrupt <caminho_imagem> <numero_bloco> <offset_no_bloco> <valor_hex>
# Exemplo (escreve 0xFF no byte 0 do bloco 3):
build/corrupt disco.img 3 0 FF
```

### 4. `scan`
Varre todos os blocos de uma imagem comparando-os com a Merkle tree e aponta os corrompidos.
```bash
build/scan <caminho_imagem> <caminho_verity> [--quiet]
# Exemplo:
build/scan disco.img disco.verity --quiet
```

### 5. `dump_verity`
Lê e exibe os metadados do cabeçalho de um arquivo `.verity`.
```bash
build/dump_verity <caminho_verity> [--full]
# Exemplo:
build/dump_verity disco.verity
```

### 6. `bench`
Mede o impacto e número de computações de hashes conforme a capacidade do Cache LRU varia.
```bash
build/bench [n_passes] [image_size]
# Exemplo:
build/bench
```

---

## 📈 Resultados Experimentais (Benchmark)

Os experimentos práticos confirmam o benefício da cache de nós em acessos sequenciais:

* **Configuração:** Imagem de 128 blocos de 4096 bytes (512 KB) com Merkle Tree de 255 nós totais.
* **Cenário Sem Cache:** No 2º passe de leitura, cada bloco lido requer recomeçar a computação (1 hash por leitura de folha), totalizando **382 computações**.
* **Cenário Com Cache total (Capacidade = 256):** No 2º passe, as subárvores quentes são lidas da cache. O número total de hashes cai para **256 computações**, representando uma **redução de 33% no esforço de CPU**.

Para gerar ou atualizar o gráfico comparativo do benchmark, execute:
```bash
python scripts/plot_benchmark.py
# Gera: scripts/benchmark_results.png
```

---

## 🧪 Cobertura de Testes

O projeto conta com **137 testes automatizados** distribuídos em 4 módulos fundamentais:

* **SHA-256 (`test_sha256.c` — 6/6):** Vetores oficiais do FIPS 180-4, testes de determinismo, e atualização incremental de buffer.
* **Merkle Tree (`test_hash_tree.c` — 41/41):** Validação matemática de tamanhos, arredondamento de potência de 2, gravação física, roundtrip e o **teste de fogo** (detecção de 1 byte alterado).
* **Cache LRU (`test_node_cache.c` — 60/60):** Mecanismo de busca, atualização, invalidade, limpeza, promoção LRU e testes sob stress de evicções.
* **Leitura Verificada (`test_verity.c` — 30/30):** Testes de parada antecipada mensurando redução de contadores de CPU, integridade sob falhas com cache aquecido e null-safety.

---

## 📚 Referências

* [dm-verity da Documentação do Kernel Linux](https://www.kernel.org/doc/html/latest/admin-guide/device-mapper/verity.html)
* [FIPS PUB 180-4 (Secure Hash Standard)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
