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

## 25/06/2026 — Merkle tree: construção, persistência e verificação (E1/E2)

### Decisões de Projeto

**1. Indexação BFS em array plano**
- Nós armazenados em array linear: raiz=0, filhos de `i` em `2i+1` e `2i+2`.
- Folha do bloco `b` = `(num_leaves - 1) + b`.
- Permite acesso O(1) a qualquer nó via `fseek` no arquivo `.verity`.

**2. Blocos de padding para potência de 2**
- Folhas virtuais (para completar `num_leaves`) recebem `SHA-256(bloco_de_zeros)`.
- Garante que a árvore é sempre binária completa sem casos especiais no algoritmo.

**3. Critério de parada da verificação: caminho folha→raiz**
- `verify_block` percorre apenas `O(log n)` nós — não relê a árvore inteira.
- Sobe usando o hash irmão já armazenado; compara cada nó pai com o valor gravado.
- Verificação final contra `root_hash` (campo separado do array, golden truth).

**4. Formato do `.verity`**
- Magic `VERITY14` + versão (u32) + block_size (u32) + num_blocks (u64) + root_hash (32 B).
- Seguido do array BFS completo de hashes.
- Valores gravados em representação nativa (máquina homogênea; portabilidade big/little-endian deixada para versão futura).

### Bugs encontrados

**Bug 1 — `%llu` não reconhecido pelo gcc 4.9.2 (TDM-GCC Windows)**
- **Descrição:** `hash_tree.c` usava `%llu` com cast `(unsigned long long)` para imprimir `uint64_t`. O gcc 4.9.2 do Windows não reconhece `%llu` e emitiu erro `-Werror=format=`.
- **Causa raiz:** Versão antiga do gcc bundled no TDM-GCC (4.9.2) não suporta o especificador `%llu` em `printf`.
- **Solução:** Substituído por `%lu` com cast `(unsigned long)`. Aceitável para os valores do projeto (imagens < 4 GB de blocos).

### Uso de IA

**Prompt:** "Desenvolva funções para construir e persistir uma árvore e ler e verificar um bloco."

**O que a IA gerou corretamente:**
- `hash_tree.c` completo: `build` (fases folhas + nós internos bottom-up), `save`/`load` com magic VERITY14, `verify_block` O(log n) e utilitários.
- `test_hash_tree.c`: 41 verificações cobrindo `next_power_of_two`, construção (1 e 3 blocos), round trip save/load, magic inválido, verificação de blocos íntegros, **teste de fogo** (corrupção de 1 byte detectada, demais blocos intactos) e verificação pós-load.
- Makefile atualizado com `hash_tree.c` na biblioteca e target `test_hash_tree`.

**O que a IA errou:**
- Usou `%llu` / `unsigned long long` em `printf`, incompatível com gcc 4.9.2 no Windows.

**O que a equipe corrigiu:**
- Substituiu `%llu` por `%lu` com cast `(unsigned long)` para compatibilidade com o compilador do ambiente.

---

## 25/06/2026 — Ferramentas CLI: mkverity, verify_block, corrupt (E2)

### Decisões de Projeto

**1. `mkverity` aceita `block_size` opcional (padrão 4096)**
- Mantém flexibilidade para experimentos com blocos diferentes.
- Valida o intervalo 1..65536 para evitar abusos.

**2. `verify_block` carrega a árvore do `.verity` e lê o bloco da imagem**
- Não depende de estado em memória — pode ser chamado isoladamente.
- Retorna exit code 0 (OK) ou 1 (falha), adequado para uso em scripts.

**3. `corrupt` opera diretamente no arquivo (in-place)**
- Usa block_size fixo de 4096 (mesma convenção padrão do `mkverity`).
- Intencionalmente simples: apenas um byte por chamada, para teste controlado.
- Imprime o valor original → novo para rastreabilidade no log.

**4. Target `stress` no Makefile**
- Automatiza o ciclo: gerar imagem → mkverity → verify (OK) → corrupt → verify (FAIL) → verify outro bloco (OK).
- Requer `dd` e ambiente Unix/WSL para rodar; no Windows é executado manualmente.

### Bugs encontrados
*(Nenhum nesta sessão)*

### Uso de IA

**Prompt:** "Realize a próxima etapa" (ferramentas CLI E2).

**O que a IA gerou corretamente:**
- `mkverity.c`, `verify_block.c`, `corrupt.c` com zero warnings (`-Wall -Wextra -Werror`).
- Makefile atualizado com targets das ferramentas e `stress` automatizado.
- Teste de fogo end-to-end validado: bloco 3 corrompido detectado, bloco 0 intacto permanece OK.

**O que a IA errou:** Nenhum erro identificado nesta sessão.

**O que a equipe corrigiu:** N/A.

---

## 25/06/2026 — Cache LRU de nós (E3)

### Decisões de Projeto

**1. Hash table com endereçamento aberto (linear probing + tombstones)**
- Sentinels: `SLOT_EMPTY = UINT64_MAX` (nunca usado) e `SLOT_DELETED = UINT64_MAX-1` (tombstone pós-evicção).
- Fator de carga ≤ 0.5 (`table_sz = 2 * capacity + 1`) — garante que a sondagem é curta.
- Tombstones permitem que `find_entry` continue a sondagem após uma entrada eviccionada sem quebrar a cadeia de colisões.

**2. Lista duplamente encadeada para política LRU**
- `lru_head` = MRU (mais recentemente usado); `lru_tail` = LRU (candidato à evicção).
- `lru_move_to_head` ao acessar qualquer entrada já presente.
- Ponteiros `lru_prev`/`lru_next` vivem dentro da própria entrada do hash table — sem alocação extra.

**3. Fibonacci hashing para distribuição uniforme**
- `key * 11400714819323198485ULL` distribui índices sequenciais de forma uniforme, evitando clustering com sondagem linear.

**4. Compatibilidade com gcc 4.9.2**
- `%zu` substituído por `%lu` com cast `(unsigned long)` (mesma limitação encontrada nas sessões anteriores).

### Bugs encontrados

**Bug 1 — `%zu` não reconhecido pelo gcc 4.9.2**
- Mesma causa raiz da sessão anterior. Corrigido antes de commitar.

**Bug 2 — Comparação signed/unsigned no teste**
- `c.evictions == N - (int)CAP` gerava `-Werror=sign-compare`.
- Corrigido com cast explícito: `(uint64_t)(N - (int)CAP)`.

### Uso de IA

**Prompt:** "Siga para a implementação de node_cache.c com a estrutura descrita no header."

**O que a IA gerou corretamente:**
- `node_cache.c` com hash table (linear probing + tombstones) + lista LRU integrada.
- `test_node_cache.c`: 60 verificações cobrindo init, put/get, ordem LRU, evicção, evicção com acessos intercalados, atualização de entrada existente, invalidate, clear, estatísticas, null-safety e stress (64 itens em cache de 16).
- Makefile atualizado com `node_cache.c` e target `test_node_cache`.

**O que a IA errou:**
- Usou `%zu` e comparação signed/unsigned — incompatíveis com gcc 4.9.2.

**O que a equipe corrigiu:**
- Substituiu `%zu` por `%lu` com cast e corrigiu a comparação com cast explícito.

---

## 25/06/2026 — Camada de leitura verificada com cache (E3)

### Decisões de Projeto

**1. Parada antecipada folha→raiz com cache**
- `verity_read` sobe o caminho calculando o hash de cada nó pai.
- Se o pai está no cache: compara com o valor computado e para — o caminho acima já foi verificado em leitura anterior.
- Se o pai não está: compara com a árvore, armazena no cache e continua subindo.
- Benefício medido: 8 blocos, 2ª leitura completa faz **22 → 16 hashes** (−27%).

**2. Sem cache: apenas 1 hash por leitura (folha)**
- Sem cache habilitado, `verity_read` calcula apenas `sha256(bloco)` e compara com a folha armazenada. Não sobe o caminho — mais rápido, mas sem caching de nós internos.
- Essa diferença de comportamento é o que o relatório experimental medirá.

**3. Detecção de inconsistência no cache**
- Se o cache retorna um hash para o pai, mas o valor computado não bate → corrupção detectada mesmo com cache quente.
- Garante que um bloco nunca é declarado íntegro incorretamente por causa de um hit de cache desatualizado.

**4. `verity_open` com `cache_capacity = 0`** desabilita o cache (degradação graciosa).

### Bugs encontrados
*(Nenhum nesta sessão — zero warnings em primeira compilação)*

### Uso de IA

**Prompt:** "Implemente a camada de leitura verificada com cache integrado."

**O que a IA gerou corretamente:**
- `verity.c` com `verity_open/close/read/print_stats`; algoritmo de parada antecipada folha→raiz com cache.
- `test_verity.c`: 30 verificações cobrindo open/close, leitura sem/com cache, detecção de corrupção (sem e com cache), parada antecipada mensurável, null-safety.
- Makefile atualizado com `verity.c` e target `test_verity`.

**O que a IA errou:** Nenhum erro identificado nesta sessão.

**O que a equipe corrigiu:** N/A.

---

## 25/06/2026 — Scripts de benchmark e relatório experimental (E4)

### Decisões de Projeto

**1. `bench.c` como ferramenta C nativa (não shell script)**
- Garante portabilidade Windows/Linux e compilação com `-Werror`.
- Saída em CSV para fácil importação em planilha ou Python.
- Mede `clock()` (tempo de CPU) — adequado para comparar o custo de SHA-256.

**2. `cache_cap = 0` comporta-se diferente de cache habilitado**
- Sem cache: `verity_read` só calcula 1 hash por bloco (folha). Caminho completo não é percorrido.
- Com cache: percorre o caminho completo no 1º passe para popular o cache; poupa no 2º.
- Essa diferença foi documentada e analisada no relatório.

**3. Resultado experimental confirmado**
- Com `cache_cap = 256` (≥ total de nós): 2º passe usa **256 hashes** (1/bloco) em vez de 382 → **redução de 33 %**.
- Teste de fogo: 128 blocos, bloco 10 corrompido → detectado; 127 demais íntegros → OK.

### Bugs encontrados
*(Nenhum nesta sessão)*

### Uso de IA

**Prompt:** "Gere os scripts de benchmark e relatório."

**O que a IA gerou corretamente:**
- `tools/bench.c` com saída CSV, 2 passes, suporte a `n_passes` configurável.
- `scripts/stress.ps1`: automação completa do teste de fogo em PowerShell.
- `scripts/run_benchmark.ps1`: roda bench com 7 capacidades de cache e exibe tabela.
- `RELATORIO.md`: relatório experimental com dados reais coletados do benchmark.
- Makefile atualizado com target `bench`.

**O que a IA errou:** Nenhum erro identificado nesta sessão.

**O que a equipe corrigiu:** N/A.

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
