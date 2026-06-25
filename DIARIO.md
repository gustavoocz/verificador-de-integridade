# DIÁRIO DE ENGENHARIA — Verificador de Integridade de Disco

**Tema 14 | ED × SO | Ifes Cachoeiro**
**Apresentação: 02/07/2026**

---

## 23/06/2026 — Fundação do projeto

### Decisões de Projeto

**1. Estrutura da Merkle tree**
- Árvore binária completa; número de blocos arredondado para próxima potência de 2.
- Nós indexados em array BFS plano: raiz = 0, filhos de `i` em `2i+1` e `2i+2`.
- Isso permite `fseek` direto para qualquer nó no arquivo `.verity` (acesso O(1)).
- Alternativa descartada: árvore n-ária estilo dm-verity real (fator 128).
  Complexidade desnecessária para E1; revisitar se o tempo permitir.

**2. Formato do arquivo `.verity`**
- Cabeçalho fixo: magic `VERITY14` (8 B) + versão (4 B) + block_size (4 B)
  + num_blocks (8 B) + root_hash (32 B).
- Corpo: array BFS de hashes (cada nó = 32 bytes SHA-256).
- Justificativa: sem parsing, acesso O(1) a qualquer nó via `fseek`, fácil de
  inspecionar com `xxd`.

**3. Cache de nós (planejado para E3)**
- Hash table com endereçamento aberto + lista duplamente encadeada para LRU.
- Capacidade configurável em número de nós.

### Bugs encontrados
*(Nenhum — início do projeto)*

### Uso de IA

**Prompt:** Analisar o PDF do trabalho e gerar a estrutura inicial do projeto (Tema 14).

**O que a IA errou:** Gerou toda a implementação de uma vez, violando a regra de
commits incrementais do edital.

**O que a equipe corrigiu:** Descartou a implementação gerada e manteve apenas
README, DIARIO, .gitignore e headers `.h` para o primeiro commit. A implementação
será feita incrementalmente.

---

## 25/06/2026 — Implementação do SHA-256 (E1)

### Decisões de Projeto

**1. SHA-256 com funções `static inline` (sem macros)**
- Decisão: não usar macros de pré-processador para as funções lógicas
  (`ch`, `maj`, `ep0`, `ep1`, `sig0`, `sig1`, `rotr32`).
- Motivo: macros com operadores bit a bit tiveram comportamento inesperado no
  gcc 4.9.2 (TDM-GCC, Windows). Funções `static inline` eliminam ambiguidade
  de precedência e promoção de tipo.
- Resultado: todos os 6 testes unitários passando.

**2. Vetor de teste SHA-256 verificado em fonte externa**
- O valor correto de `SHA-256("abc")` é:
  `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`
- Confirmado em múltiplas ferramentas online antes de fechar o teste.

### Bugs encontrados

**Bug 1 — Vetor de teste incorreto em `test_sha256.c`**
- **Descrição:** O teste `B.1: SHA-256("abc")` falhava consistentemente (5/6).
- **Investigação:** Imprimimos o digest gerado e comparamos byte a byte.
  O digest produzido pela implementação divergia a partir do byte 14.
  Os outros vetores (string vazia e 56 bytes) passavam.
- **Causa raiz:** O valor esperado hardcoded estava errado — copiado de uma versão
  do FIPS 180-4 com erro tipográfico no Appendix B. A implementação estava correta.
- **Solução:** Corrigido o valor esperado no teste após confirmar o digest correto
  em fonte externa. Adicionado comentário explicativo no código.

### Uso de IA

**Prompt:** "Realize a próxima etapa" (SHA-256, E1).

**O que a IA gerou corretamente:**
- `sha256.c` com funções `static inline` defensivas.
- `test_sha256.c` com vetores FIPS, teste incremental, determinismo e inputs distintos.
- Makefile atualizado para compilar e rodar `make test`.

**O que a IA errou:**
- Hardcodou o vetor `SHA-256("abc")` com valor incorreto.
- Tentou depurar os macros de rotação como causa do bug, perdendo tempo, quando
  o erro estava apenas no valor esperado do teste.

**O que a equipe corrigiu:**
- Confirmou o digest correto via ferramenta online.
- Corrigiu o valor esperado em `test_sha256.c`.

---

## DD/MM/AAAA — (preencher)

### Decisões de Projeto
- ...

### Bugs encontrados
- ...

### Uso de IA (se houver)
- **Prompt:**
- **O que a IA gerou corretamente:**
- **O que a IA errou / o que a equipe corrigiu:**
