# DLP Solver - Solucao Otimizada para Logaritmo Discreto

Solucao em C++ para quebrar o Problema do Logaritmo Discreto (DLP) no contexto do protocolo Diffie-Hellman.

## Algoritmos Implementados

- **Fatoracao**: Pollard's Rho + Trial Division para fatorar p-1
- **DLP em Subgrupos**: Baby-Step Giant-Step (BSGS) para grupos pequenos
- **DLP em Subgrupos Grandes**: Pollard's Rho para primos grandes
- **Combinacao**: Teorema Chines do Resto (CRT)
- **Estrategia Principal**: Pohlig-Hellman para reduzir o problema

## Requisitos

- C++17 ou superior
- Biblioteca GMP (GNU Multiple Precision Arithmetic Library)
- Compilador g++ com suporte a threads

### Instalacao do GMP

**macOS (Homebrew):**
```bash
brew install gmp
```

**Windows:**

Opcao 1 - MSYS2 (Recomendado):
1. Instale MSYS2 de https://www.msys2.org/
2. Abra MSYS2 UCRT64 terminal
3. Execute: `pacman -S mingw-w64-x86_64-gmp`
4. Compile com: `make`

Opcao 2 - TDM-GCC (Atual):  
Se voce ja tem TDM-GCC instalado, precisa adicionar GMP:
1. Baixe GMP para MinGW de: https://gmplib.org/download.html
2. Ou instale via MSYS2 e copie os arquivos para o TDM-GCC

Opcao 3 - vcpkg:
```bash
vcpkg install gmp
make GMP_CFLAGS="-I$(vcpkg root)/installed/x64-windows/include" GMP_LDFLAGS="-L$(vcpkg root)/installed/x64-windows/lib -lgmp -lgmpxx"
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libgmp-dev
```

**Fedora:**
```bash
sudo dnf install gmp-devel
```

**Arch Linux:**
```bash
sudo pacman -S gmp
```

## Compilacao

### macOS
```bash
make
```

### Windows (MSYS2/MinGW)
```bash
make
```

### Linux
```bash
make
```

### Compilacao Manual (com paths customizados)

Se o GMP esta instalado em um local nao-padrao:

```bash
# Exemplo: GMP em /custom/path
make GMP_CFLAGS="-I/custom/path/include" GMP_LDFLAGS="-L/custom/path/lib -lgmp -lgmpxx"

# Exemplo: vcpkg no Windows
make GMP_CFLAGS="-I$VCPKG_ROOT/installed/x64-windows/include" \
     GMP_LDFLAGS="-L$VCPKG_ROOT/installed/x64-windows/lib -lgmp -lgmpxx"

# Exemplo: MSYS2 no Windows (PowerShell)
make GMP_CFLAGS="-I/C/msys64/mingw64/include" \
     GMP_LDFLAGS="-L/C/msys64/mingw64/lib -lgmp -lgmpxx"
```

### Compilacao Direta (sem Makefile)

```bash
# macOS
g++ -std=c++17 -O3 -I/opt/homebrew/include -o dlp_solver dlp_solver.cpp \
    -L/opt/homebrew/lib -lgmp -lgmpxx -pthread

# Linux
g++ -std=c++17 -O3 -o dlp_solver dlp_solver.cpp -lgmp -lgmpxx -pthread

# Windows MSYS2
g++ -std=c++17 -O3 -I/mingw64/include -o dlp_solver.exe dlp_solver.cpp \
    -L/mingw64/lib -lgmp -lgmpxx -pthread
```

## Execucao

### Padrao
```bash
./dlp_solver
```

### Com arquivos customizados
```bash
./dlp_solver desafios.txt solucao.txt
```

## Estrutura dos Arquivos

### Entrada (desafios.txt)
```
[C0] bits=16
p = 36467
alpha = 20347
A = 13686
B = 8710

[C1] bits=32
p = 3008149519
...
```

### Saida (solucao.txt)
Valores de K_ab em decimal, um por linha:
```
15261
<valor K_ab do C1>
<valor K_ab do C2>
...
```

## Configuracoes

Editar as constantes no inicio de `dlp_solver.cpp`:

```cpp
const int MAX_THREADS = thread::hardware_concurrency();
const int TIMEOUT_SECONDS = 3600;    // Timeout por cenario (1 hora)
const size_t BSGS_THRESHOLD = 100000000; // Limite para BSGS
```

## Funcionalidades

- **Paralelismo**: Utiliza todas as threads disponiveis da CPU
- **Timeout**: Cada cenario tem limite de tempo configuravel
- **Progresso**: Exibe tempo gasto em cada cenario
- **Verificacao**: Valida automaticamente as solucoes
- **Recuperacao**: Cenarios que falham sao marcados com #

## Comandos Make

```bash
make              # Compilar
make clean      # Limpar arquivos
make run        # Compilar e executar
make test       # Testar com C0 (exemplo)
make check      # Verificar instalacao GMP
```

## Arquitetura do Codigo

```
dlp_solver.cpp
├── Timer                      - Controle de tempo
├── Factorizer                 - Fatoracao Pollard's Rho + Trial Division
├── BSGSSolver                 - Baby-Step Giant-Step
├── PollardRhoDLP              - Pollard's Rho para DLP
├── crt()                      - Teorema Chines do Resto
├── PohligHellmanSolver        - Algoritmo principal
├── DLPSolver                  - Interface do solucionador
├── parseChallenges()          - Parser de entrada
├── workerThread()             - Thread worker
└── main()                     - Orquestracao
```

## Estrategia Algoritmica

1. **Fatorar p-1** usando Pollard's Rho
2. **Calcular a ordem** do gerador alpha
3. **Aplicar Pohlig-Hellman** para decompor o problema
4. **Resolver em subgrupos** usando BSGS ou Pollard's Rho
5. **Combinar com CRT** para obter o logaritmo discreto
6. **Calcular K_ab** = B^a mod p

## Notas de Desempenho

- Cenarios com p-1 tendo fatores primos grandes (> 10^8 bits) podem atingir timeout
- A dificuldade depende da fatoracao de p-1, nao apenas do tamanho de p
- BSGS usa O(sqrt(n)) memoria e tempo
- Pollard's Rho usa O(1) memoria mas pode ser mais lento

## Exemplo de Saida

```
========================================
DLP Solver - Pohlig-Hellman Optimizado
========================================
Threads disponiveis: 8
Timeout por cenario: 3600s
========================================

[Thread 1234] Iniciando C1 (32 bits)...
[Thread 5678] Iniciando C2 (32 bits)...
[Thread 1234] C1 RESOLVIDO em 0.023s - K_ab = 123456789
[Thread 5678] C2 RESOLVIDO em 0.045s - K_ab = 987654321
...

RESUMO
Total de desafios: 23
Resolvidos: 15
Timeout: 8
Tempo total: 1234.567s
```

## Troubleshooting

### Erro: `gmpxx.h: No such file or directory`

O GMP nao esta instalado ou nao foi encontrado. Execute:
```bash
make check
```

Isso mostrara onde o Makefile esta procurando o GMP.

**Solucao Windows:**
1. Instale MSYS2 de https://www.msys2.org/
2. Abra terminal MSYS2 UCRT64
3. Execute: `pacman -S mingw-w64-x86_64-gmp`
4. Compile: `make`

### Erro: `undefined reference to '__gmp...'`

As bibliotecas GMP nao foram linkadas corretamente. Tente:
```bash
make clean
make GMP_LDFLAGS="-lgmp -lgmpxx"
```

### Erro: `cannot find -lgmp`

A biblioteca GMP nao esta no path de bibliotecas. Especifique o path completo:
```bash
make GMP_LDFLAGS="-L/caminho/para/gmp/lib -lgmp -lgmpxx"
```

### Erro: `omp.h not found` (OpenMP)

Se encontrar problemas com OpenMP, o codigo usa `std::thread` entao nao precisa de OpenMP. A opcao `-pthread` e suficiente.

### Erro de compilacao no Windows com TDM-GCC

Se voce tem TDM-GCC mas sem GMP:
1. Baixe GMP pre-compilado para MinGW
2. Extraia `gmp.h` e `gmpxx.h` para `C:\TDM-GCC-64\include`
3. Extraia `libgmp.dll.a` e `libgmpxx.dll.a` para `C:\TDM-GCC-64\lib`
4. Ou use MSYS2 que ja tem tudo configurado

### Programa trava em cenarios grandes

Isso e esperado! Cenarios com p-1 tendo um fator primo muito grande (> 10^8) podem exigir muito tempo para resolver. O timeout de 1 hora protege contra isso. Para aumentar o timeout:
```cpp
const int TIMEOUT_SECONDS = 7200; // 2 horas
```

### Solucoes incorretas para C0

C0 e um exemplo com a solucao revelada. Se o programa nao retornar `15261`, verifique:
1. Se o parser esta lendo corretamente o arquivo
2. Se a aritmetica modular esta correta
3. Execute `make test` para verificar

## Licenca

Este codigo foi desenvolvido para fins academicos.
Estrategias algoritmicas baseadas em:
- Pohlig-Hellman algorithm
- Baby-Step Giant-Step (Shanks)
- Pollard's Rho
- Chinese Remainder Theorem
