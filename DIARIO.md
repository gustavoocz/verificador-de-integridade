# DIÁRIO DE ENGENHARIA — Verificador de Integridade de Disco

**Tema 14 | ED × SO | Ifes Cachoeiro**

---

## Semana 1 — 23/06/2026

### Decisões de Projeto

**1. Estrutura da Merkle tree**
- Árvore binária completa; número de blocos arredondado para próxima potência de 2.
- Nós indexados em array BFS plano: raiz = 0, filhos de `i` em `2i+1` e `2i+2`.
- Isso permite `fseek` direto para qualquer nó no arquivo `.verity` (acesso O(1)).
- Alternativa descartada: árvore n-ária estilo dm-verity real (fator 128). Complexidade
  desnecessária para E1; revisitar em E2 se o tempo permitir.

**2. Formato do arquivo `.verity`**
- Cabeçalho fixo: magic `VERITY14` (8 B) + versão (4 B) + block_size (4 B)
  + num_blocks (8 B) + root_hash (32 B).
- Corpo: array BFS de hashes (cada nó = 32 bytes SHA-256).
- Justificativa: simples, sem parsing, fácil de validar com `xxd`.

**3. SHA-256 portável**
- Implementação própria sem OpenSSL para evitar problemas de ambiente.
- Primeiro passo: passar os vetores de teste do FIPS 180-4.

**4. Cache de nós (planejado para E3)**
- Hash table com endereçamento aberto + lista duplamente encadeada para LRU.
- Capacidade configurável em número de nós.

### Bugs encontrados
*(Nenhum — início do projeto)*

### Uso de IA
**Prompt:** Analisar o PDF do trabalho e gerar a estrutura inicial do projeto (Tema 14).

**O que a IA errou:** Gerou toda a implementação de uma vez ao invés de apenas
o esqueleto de fundação — violando a regra de commits incrementais do edital.

**O que a equipe corrigiu:** Descartou toda a implementação gerada e manteve
apenas README, DIARIO e Makefile skeleton para o primeiro commit.
A implementação será feita incrementalmente pela equipe.

---

## Template para próximas semanas

```
## Semana N — DD/MM/AAAA

### Decisões de Projeto
- ...

### Bugs encontrados
- Descrição, causa raiz, solução adotada.

### Uso de IA (se houver)
- Prompt utilizado:
- O que a IA gerou corretamente:
- O que a IA errou / o que a equipe corrigiu:
```
