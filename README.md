# DLP Solver - Solucao para Logaritmo Discreto

Solucao em C++ para o Problema do Logaritmo Discreto (DLP) no contexto do protocolo Diffie-Hellman.
Dado `A = alpha^a mod p` e `B = alpha^b mod p`, calcular `K_ab = B^a mod p = alpha^(ab) mod p`.

Duas versoes disponiveis:

| Versao | Arquivo | Biblioteca | Saida |
|--------|---------|------------|-------|
| **v1** | `dlp_solver.cpp` | GMP | `solucao.txt` |
| **v2** | `dlp_solver_v2.cpp` | Boost.Multiprecision | `solucao_v2.txt` |

---

## Algoritmos Implementados

- **Pohlig-Hellman** — estrategia principal: decompoe o DLP em subproblemas por cada fator primo de p-1
- **BSGS (Baby-Step Giant-Step)** — resolve subgrupos de ordem pequena em O(sqrt(n))
- **Pollard's Rho** — resolve subgrupos maiores com O(1) de memoria
- **Fatoracao** — Trial Division + Pollard's Rho para fatorar p-1
- **CRT (Teorema Chines do Resto)** — combina solucoes parciais

A dificuldade de cada cenario **nao depende do tamanho de p**, mas sim dos **fatores primos de p-1**:
- Se p-1 tem apenas fatores pequenos → resolvivel rapidamente
- Se p-1 tem um fator primo grande → pode atingir timeout

---

## Requisitos

### v1 — GMP

**macOS:**
```bash
brew install gmp
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libgmp-dev
```

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-x86_64-gmp
```

### v2 — Boost.Multiprecision (header-only)

**macOS:**
```bash
brew install boost
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libboost-dev
```

---

## Compilacao e Execucao

### Comandos Make

```bash
make          # Compila v1 (dlp_solver)
make run      # Compila e executa v1 -> solucao.txt
make run2     # Compila e executa v2 -> solucao_v2.txt
make clean    # Remove binarios e arquivos de saida
make test     # Testa v1 com C0 (sanity check)
make check    # Verifica instalacao do GMP
```

### Compilacao Manual

**v1 (macOS):**
```bash
g++ -std=c++17 -O3 -I/opt/homebrew/include -o dlp_solver dlp_solver.cpp \
    -L/opt/homebrew/lib -lgmp -lgmpxx -pthread
```

**v2 (macOS):**
```bash
g++ -std=c++17 -O3 -I/opt/homebrew/include -o dlp_solver_v2 dlp_solver_v2.cpp -pthread
```

**v1 (Linux):**
```bash
g++ -std=c++17 -O3 -o dlp_solver dlp_solver.cpp -lgmp -lgmpxx -pthread
```

**v2 (Linux):**
```bash
g++ -std=c++17 -O3 -o dlp_solver_v2 dlp_solver_v2.cpp -pthread
```

### Execucao com arquivos customizados

```bash
./dlp_solver    desafios.txt solucao.txt
./dlp_solver_v2 desafios.txt solucao_v2.txt
```

---

## Formato dos Arquivos

### Entrada (`desafios.txt`)

```
# Trabalho Pratico --- Logaritmo Discreto
#
[C0]  bits=16  (EXEMPLO RESOLVIDO)
p     = 36467
alpha = 20347
A     = 13686
B     = 8710

[C1]  bits=32
p     = 3008149519
alpha = 223722476
A     = 1957790020
B     = 1354383354
```

### Saida (`solucao.txt` / `solucao_v2.txt`)

Cada linha corresponde a um cenario: `[Cx] <K_ab>` se resolvido, ou `[Cx] # Timeout...` se falhou.
Os resultados sao salvos **incrementalmente** — interromper com Ctrl+C preserva o que ja foi resolvido.

```
# DLP Solver - Solucoes
# Gerado em: 1781894338812611
# Total de desafios: 23
#
[C0] 15261
[C1] 2039871234
[C2] # Timeout ou erro: Timeout apos 3600s
[C3] 748291038475
...
```

> **Sanity check**: C0 tem solucao revelada — o resultado correto e `K_ab = 15261`.

---

## Funcionalidades

- **Paralelismo** — usa todas as threads da CPU disponíveis
- **Timeout** — cada cenario tem limite de 3600s (configuravel)
- **Salvamento incremental** — cada resultado e gravado imediatamente apos ser resolvido
- **Interrupcao segura** — Ctrl+C salva os resultados ja obtidos antes de encerrar
- **Verificacao automatica** — valida alpha^a = A (mod p) antes de calcular K_ab

---

## Configuracoes

Editar as constantes no inicio de cada arquivo:

**v1 (`dlp_solver.cpp`):**
```cpp
const int TIMEOUT_SECONDS = 3600;       // Timeout por cenario
const size_t BSGS_THRESHOLD = 100000000; // Limite para BSGS (100M)
```

**v2 (`dlp_solver_v2.cpp`):**
```cpp
const int TIMEOUT_SECONDS = 3600;
const size_t BSGS_THRESHOLD = 10000000;      // 10M
const size_t POLLARD_THRESHOLD = 1000000000; // 1B
```

---

## Arquitetura

```
v1: dlp_solver.cpp
├── Timer                  - Controle de tempo por cenario
├── Factorizer             - Trial Division + Pollard's Rho para fatorar p-1
├── BSGSSolver             - Baby-Step Giant-Step (map ordenado)
├── PollardRhoDLP          - Pollard's Rho para DLP
├── crt()                  - Teorema Chines do Resto
├── PohligHellmanSolver    - Algoritmo principal de decomposicao
├── DLPSolver              - Calcula K_ab = B^a mod p
├── parseChallenges()      - Parser do arquivo de entrada
├── appendSolution()       - Salvamento incremental thread-safe
└── main()                 - Orquestracao com SIGINT handler

v2: dlp_solver_v2.cpp  (mesma estrutura, bibliotecas diferentes)
├── BigInt (Boost)         - Precisao arbitraria sem GMP
├── BSGSSolver             - BSGS com unordered_map (hash, O(1) medio)
├── PollardRhoSolver       - Pollard's Rho com 3 seeds diferentes
└── (demais identicos ao v1)
```

---

## Estrategia Algoritmica

```
Para cada cenario Ci:
  1. Fatorar p-1 = q1^e1 * q2^e2 * ... * qk^ek
  2. Para cada fator primo qi^ei:
       a. Reduzir ao subgrupo: alpha_i = alpha^((p-1)/qi^ei) mod p
       b. Se qi^ei <= threshold  → BSGS
          Se qi^ei <= 1B        → Pollard's Rho
          Caso contrario         → Pohlig-Hellman recursivo
  3. Combinar resultados parciais com CRT → expoente 'a'
  4. Calcular K_ab = B^a mod p
```

---

## Troubleshooting

### `gmpxx.h: No such file or directory`
```bash
make check   # Diagnostica a instalacao do GMP
brew install gmp   # macOS
```

### `boost/multiprecision/cpp_int.hpp: No such file or directory`
```bash
brew install boost   # macOS
sudo apt-get install libboost-dev   # Linux
```

### `undefined reference to '__gmp...'`
```bash
make clean && make
```

### Todos os cenarios retornam "Timeout apos 0s"
Verificar se o arquivo de entrada tem o formato correto com espacos nos campos (`A     = valor`).
O parser aceita chaves com espacos antes do `=`.

### Cenario C0 nao retorna 15261
```bash
make test   # Testa isoladamente com C0
```
Se falhar, a aritmetica modular ou o parser estao com problema.

### Programa trava em cenarios grandes
Esperado para cenarios com fator primo grande em p-1. Aguardar timeout ou usar Ctrl+C —
os resultados ja obtidos serao salvos automaticamente.

---

## Licenca

Desenvolvido para fins academicos — CSS 98G08-04.
Algoritmos baseados em: Pohlig-Hellman (1978), Pollard's Rho (1978), Baby-Step Giant-Step (Shanks).
