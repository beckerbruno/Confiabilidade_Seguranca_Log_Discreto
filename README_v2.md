# DLP Solver v2 - Versão Otimizada

Segunda versão do solver com melhorias significativas de performance para cenários grandes.

## Melhorias em Relação à v1

### 1. Sem Dependência do GMP
- Usa **Boost.Multiprecision** (header-only)
- Não requer instalação de bibliotecas externas
- Compilação simples e portátil

### 2. Algoritmos Otimizados
- **Pohlig-Hellman** com estratégia adaptativa
- **BSGS** otimizado com hash table
- **Pollard's Rho** paralelo com múltiplas tentativas
- **Fatoração** com Pollard's Rho + Trial Division

### 3. Estratégia de Hibridização
```
┌─────────────────────────────────────────────────────────┐
│ Ordem do Subgrupo   │ Algoritmo                         │
├─────────────────────────────────────────────────────────┤
│ < 10M               │ BSGS (hash table)                 │
│ 10M - 1B            │ Pollard's Rho (várias seeds)      │
│ > 1B                │ Pohlig-Hellman recursivo          │
│ p-1 primo grande    │ Pollard's Rho + timeout           │
└─────────────────────────────────────────────────────────┘
```

## Compilação (Windows)

### Opção 1: Script Automático (Recomendado)
```batch
compile_v2.bat
```

Isso:
1. Verifica/Baixa Boost automaticamente
2. Compila o código
3. Pergunta se quer executar

### Opção 2: Manual

**Passo 1: Baixar Boost**
```powershell
powershell -ExecutionPolicy Bypass -File .\setup-boost.ps1
```

**Passo 2: Compilar**
```batch
g++ -std=c++17 -O3 -march=native -Wall -I. -o dlp_solver_v2.exe dlp_solver_v2.cpp -pthread
```

### Compilação (macOS/Linux)

**Baixar Boost:**
```bash
# macOS com Homebrew
brew install boost

# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# Fedora
sudo dnf install boost-devel

# Ou baixar headers manualmente:
# https://www.boost.org/users/history/version_1_84_0.html
```

**Compilar:**
```bash
# macOS (Homebrew)
g++ -std=c++17 -O3 -march=native -I/opt/homebrew/include -o dlp_solver_v2 dlp_solver_v2.cpp -pthread

# Linux
g++ -std=c++17 -O3 -march=native -o dlp_solver_v2 dlp_solver_v2.cpp -pthread
```

## Execução

```batch
# Padrão
dlp_solver_v2.exe

# Arquivos customizados
dlp_solver_v2.exe desafios.txt solucao_v2.txt
```

## Arquitetura

```
dlp_solver_v2.cpp
├── Timer                    → Controle de tempo com timeout
├── BigInt (Boost)           → Aritmética de precisão arbitrária
├── Factorizer               → Fatoração otimizada
│   ├── Trial Division       → Fatores pequenos
│   └── Pollard's Rho        → Fatores grandes
├── BSGSSolver               → Baby-Step Giant-Step com hash
├── PollardRhoDLP            → Pollard's Rho para DLP
│   └── Multi-seed retry     → 3 tentativas com seeds diferentes
├── PohligHellmanSolver      → Decomposição + estratégia adaptativa
└── DLPSolver                → Interface principal
```

## Diferenças de Performance (Esperadas)

| Cenário   | Bits  | v1 (GMP+BSGS) | v2 (Otimizado) | Speedup |
|-----------|-------|---------------|----------------|---------|
| C0-C6     | 16-48 | ~0.01s        | ~0.01s         | 1x      |
| C7-C10    | 64-80 | ~0.1-1s       | ~0.05-0.5s     | 2x      |
| C11-C16   | 96-128| ~1-10s        | ~0.5-5s        | 2x      |
| C17-C18   | 256   | ~10-100s      | ~5-50s         | 2x      |
| C19-C20   | 512   | Timeout (>1h) | ~10-60min*     | ?       |
| C21-C22   | 1024  | Timeout       | Timeout*       | -       |

*Depende da fatoração de p-1

## Por Que a v2 é Melhor?

### 1. Pollard's Rho Paralelo
```cpp
// v2: Múltiplas tentativas com seeds diferentes
for (int attempt = 0; attempt < 3; ++attempt) {
    // Tentativa com seed aleatória diferente
    // Aumenta probabilidade de sucesso
}
```

### 2. Estratégia Adaptativa
```cpp
if (qe <= BSGS_THRESHOLD) {
    return bsgs.solve(...);           // Mais rápido para pequenos
} else if (qe <= POLLARD_THRESHOLD) {
    return pollard.solve(...);        // Menos memória para médios
} else {
    return solvePrimePower(...);      // Decomposição para grandes
}
```

### 3. Hash Table Otimizada
```cpp
// v1: map (O(log n))
// v2: unordered_map (O(1) médio)
unordered_map<string, BigInt> babySteps;
babySteps.reserve(m * 2);  // Pre-alocação
```

## Index Calculus (Futuro)

Para cenários 1024 bits, Index Calculus seria necessário:

```
Complexidade:
  - BSGS: O(√p) - exponencial
  - Pollard's Rho: O(√p) - exponencial  
  - Index Calculus: L_p[1/3, c] - sub-exponencial

L_p[1/3, c] = exp((c + o(1)) * (ln p)^(1/3) * (ln ln p)^(2/3))
```

Implementação do Index Calculus requer:
1. **Factor Base** - primos até bound B
2. **Relations Collection** - sieving
3. **Linear Algebra** - eliminação Gaussiana esparsa
4. **Individual Log** - fase final

Tempo de implementação: ~2-3 dias adicionais

## Troubleshooting

### Erro: "boost/multiprecision/cpp_int.hpp: No such file or directory"
```batch
# Executar setup manualmente
powershell -ExecutionPolicy Bypass -File .\setup-boost.ps1

# Ou baixar manualmente de:
# https://www.boost.org/users/history/version_1_84_0.html
# Extrair pasta "boost" para o diretório do projeto
```

### Erro: "undefined reference to pthread..."
Adicionar `-pthread` na compilação (já incluído no compile_v2.bat)

### Compilação muito lenta
Normal! Boost.Multiprecision é template-heavy. A compilação pode levar 30s-2min.

### Timeout em cenários grandes
Cenários 512+ bits com p-1 tendo fator primo grande podem exigir Index Calculus completo.

## Comparação com Outras Ferramentas

| Ferramenta        | Algoritmo        | Cenário 256b | Cenário 512b | Cenário 1024b |
|--------------------|------------------|--------------|--------------|---------------|
| **dlp_solver_v2**  | PH+BSGS+Rho      | ✓ segundos   | ? minutos    | ✗ timeout     |
| CADO-NFS           | NFS (Index)      | ✓ rápido     | ✓ ~minutos   | ✓ ~horas      |
| SageMath           | Vários           | ✓ segundos   | ✓ minutos    | ✓ horas       |
| Magma              | Vários           | ✓ segundos   | ✓ minutos    | ✓ horas       |

Para cenários 1024 bits profissionalmente, recomenda-se:
- **CADO-NFS**: https://cado-nfs.gforge.inria.fr/
- **SageMath**: https://www.sagemath.org/

## Referências

1. **Pohlig-Hellman**: Pohlig, S., & Hellman, M. (1978). "An improved algorithm for computing logarithms over GF(p)"
2. **Pollard's Rho**: Pollard, J.M. (1978). "Monte Carlo methods for index computation"
3. **Index Calculus**: Adleman, L.M. (1979). "A subexponential algorithm for the discrete logarithm problem"
4. **CADO-NFS**: Barbulescu, R., et al. (2014). "The relationship between the General Number Field Sieve and the Function Field Sieve"

## Changelog v2

- ✓ Substituição GMP por Boost.Multiprecision
- ✓ Pollard's Rho com múltiplas tentativas
- ✓ BSGS com unordered_map otimizado
- ✓ Estratégia adaptativa baseada em threshold
- ✓ Melhor tratamento de timeout
- ✓ Compilação simplificada (sem dependências externas)
- ✗ Index Calculus não implementado (complexo)
