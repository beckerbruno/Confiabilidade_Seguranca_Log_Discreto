/*
 * Solucao otimizada para o Problema do Logaritmo Discreto (DLP)
 * utilizando Pohlig-Hellman + BSGS + Pollard's Rho + CRT
 * 
 * Compilacao:
 *   macOS: make
 *   Windows: make
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <algorithm>
#include <random>
#include <cmath>
#include <gmpxx.h>
#include <iomanip>
#include <csignal>

using namespace std;

// ============================================================================
// VARIAVEIS GLOBAIS PARA SALVAMENTO INCREMENTAL
// ============================================================================

string g_outputFile = "solucao.txt";
mutex g_fileMutex;
atomic<bool> g_running{true};
vector<int> g_completed; // 0 = nao salvo, 1 = salvo. Acesso protegido por g_fileMutex

// ============================================================================
// CONFIGURACOES
// ============================================================================

const int MAX_THREADS = max(1, static_cast<int>(thread::hardware_concurrency()) - 1);
const int TIMEOUT_SECONDS = 3600; // 1 hora por cenario
const size_t BSGS_THRESHOLD = 100000000; // Limite para BSGS (100M)

// ============================================================================
// UTILITARIOS DE TEMPO
// ============================================================================

class Timer {
    chrono::steady_clock::time_point start;
    atomic<bool> running;
    
public:
    Timer() : running(false) {}
    
    void startTimer() {
        start = chrono::steady_clock::now();
        running = true;
    }
    
    double elapsed() const {
        if (!running) return 0.0;
        auto end = chrono::steady_clock::now();
        return chrono::duration<double>(end - start).count();
    }
    
    bool timeout(double seconds) const {
        return elapsed() > seconds;
    }
};

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

struct Challenge {
    int id;
    int bits;
    mpz_class p;
    mpz_class alpha;
    mpz_class A;
    mpz_class B;
    
    Challenge() : id(0), bits(0) {}
};

struct Solution {
    int id;
    mpz_class K_ab;
    double timeSeconds;
    bool solved;
    string error;
    
    Solution() : id(0), timeSeconds(0.0), solved(false) {}
};

// ============================================================================
// ARITMETICA MODULAR COM GMP
// ============================================================================

mpz_class modPow(const mpz_class& base, const mpz_class& exp, const mpz_class& mod) {
    mpz_class result;
    mpz_powm(result.get_mpz_t(), base.get_mpz_t(), exp.get_mpz_t(), mod.get_mpz_t());
    return result;
}

mpz_class modInv(const mpz_class& a, const mpz_class& mod) {
    mpz_class result;
    if (mpz_invert(result.get_mpz_t(), a.get_mpz_t(), mod.get_mpz_t()) == 0) {
        return 0; // Inverso nao existe
    }
    return result;
}

// ============================================================================
// FATORACAO - Pollard's Rho + Trial Division
// ============================================================================

class Factorizer {
    gmp_randclass rng;
    
    mpz_class pollardsRho(const mpz_class& n, const Timer& timer) {
        if (n % 2 == 0) return 2;
        if (n % 3 == 0) return 3;
        
        mpz_class c = rng.get_z_range(n - 1) + 1;
        mpz_class x = rng.get_z_range(n - 2) + 2;
        mpz_class y = x;
        mpz_class d = 1;
        
        while (d == 1) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            // f(x) = (x^2 + c) mod n
            x = (modPow(x, 2, n) + c) % n;
            
            // f(f(y))
            y = (modPow(y, 2, n) + c) % n;
            y = (modPow(y, 2, n) + c) % n;
            
            mpz_class diff = x > y ? x - y : y - x;
            mpz_gcd(d.get_mpz_t(), diff.get_mpz_t(), n.get_mpz_t());
            
            if (d == n) {
                // Falha, reinicia com novos parametros
                c = rng.get_z_range(n - 1) + 1;
                x = rng.get_z_range(n - 2) + 2;
                y = x;
                d = 1;
            }
        }
        
        return d;
    }
    
    void factorRecursive(const mpz_class& n, vector<mpz_class>& factors, const Timer& timer) {
        if (n == 1) return;
        if (timer.timeout(TIMEOUT_SECONDS)) return;
        
        if (mpz_probab_prime_p(n.get_mpz_t(), 25) > 0) {
            factors.push_back(n);
            return;
        }
        
        mpz_class d = pollardsRho(n, timer);
        if (d == 0 || d == 1 || d == n) {
            // Falha na fatoracao - adiciona como fator primo parcial
            factors.push_back(n);
            return;
        }
        
        factorRecursive(d, factors, timer);
        factorRecursive(n / d, factors, timer);
    }

public:
    Factorizer() : rng(gmp_randinit_default) {
        rng.seed(static_cast<unsigned long>(chrono::steady_clock::now().time_since_epoch().count()));
    }
    
    // Trial division para fatores pequenos
    void trialDivision(vector<mpz_class>& factors, mpz_class& n, const Timer& timer) {
        static const vector<unsigned long> smallPrimes = {
            2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
            53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
            127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
            179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
            233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
            283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
            353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
            419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
            467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
            547, 557, 563, 569, 571, 577, 587, 593, 599, 601,
            607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
            661, 673, 677, 683, 691, 701, 709, 719, 727, 733,
            739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
            811, 821, 823, 827, 829, 839, 853, 857, 859, 863,
            877, 881, 883, 887, 907, 911, 919, 929, 937, 941,
            947, 953, 967, 971, 977, 983, 991, 997
        };
        
        for (unsigned long p : smallPrimes) {
            if (timer.timeout(TIMEOUT_SECONDS)) return;
            while (n % p == 0) {
                factors.push_back(p);
                n /= p;
            }
            if (n == 1) break;
            if (p * p > n) break;
        }
    }
    
    vector<mpz_class> factor(const mpz_class& n, const Timer& timer) {
        vector<mpz_class> factors;
        mpz_class remaining = n;
        
        // Primeiro: trial division para fatores pequenos
        trialDivision(factors, remaining, timer);
        
        // Depois: Pollard's Rho para fatores maiores
        if (remaining > 1 && !timer.timeout(TIMEOUT_SECONDS)) {
            factorRecursive(remaining, factors, timer);
        }
        
        sort(factors.begin(), factors.end());
        return factors;
    }
};

// ============================================================================
// ALGORITMO BABY-STEP GIANT-STEP (BSGS)
// ============================================================================

class BSGSSolver {
public:
    mpz_class solve(const mpz_class& alpha, const mpz_class& beta, 
                    const mpz_class& p, const mpz_class& order,
                    const Timer& timer, bool& success) {
        success = false;
        
        mpz_class m;
        mpz_sqrt(m.get_mpz_t(), order.get_mpz_t());
        m += 1;
        
        // Baby steps: alpha^j mod p para j em [0, m)
        map<mpz_class, mpz_class> babySteps;
        mpz_class alpha_j = 1;
        
        for (mpz_class j = 0; j < m; ++j) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            if (babySteps.find(alpha_j) == babySteps.end()) {
                babySteps[alpha_j] = j;
            }
            alpha_j = (alpha_j * alpha) % p;
        }
        
        // Giant steps: beta * alpha^(-m*i) mod p para i em [0, m)
        mpz_class alpha_m_inv = modInv(modPow(alpha, m, p), p);
        if (alpha_m_inv == 0) return 0;
        
        mpz_class giant = beta;
        for (mpz_class i = 0; i < m; ++i) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            auto it = babySteps.find(giant);
            if (it != babySteps.end()) {
                mpz_class x = (i * m + it->second) % order;
                success = true;
                return x;
            }
            giant = (giant * alpha_m_inv) % p;
        }
        
        return 0;
    }
};

// ============================================================================
// ALGORITMO DE POLLARD'S RHO PARA DLP
// ============================================================================

class PollardRhoDLP {
    gmp_randclass rng;
    
    struct Triple {
        mpz_class x, a, b;
        Triple() : x(1), a(0), b(0) {}
        Triple(const mpz_class& x_, const mpz_class& a_, const mpz_class& b_) 
            : x(x_), a(a_), b(b_) {}
    };
    
    Triple step(const Triple& t, const mpz_class& alpha, const mpz_class& beta,
                const mpz_class& p, const mpz_class& order) {
        Triple next;
        mpz_class hash = t.x % 3;
        
        if (hash == 0) {
            // S1: x -> x * beta, a -> a, b -> b + 1
            next.x = (t.x * beta) % p;
            next.a = t.a % order;
            next.b = (t.b + 1) % order;
        } else if (hash == 1) {
            // S2: x -> x * x, a -> 2a, b -> 2b
            next.x = (t.x * t.x) % p;
            next.a = (2 * t.a) % order;
            next.b = (2 * t.b) % order;
        } else {
            // S3: x -> x * alpha, a -> a + 1, b -> b
            next.x = (t.x * alpha) % p;
            next.a = (t.a + 1) % order;
            next.b = t.b % order;
        }
        
        return next;
    }

public:
    PollardRhoDLP() : rng(gmp_randinit_default) {
        rng.seed(static_cast<unsigned long>(chrono::steady_clock::now().time_since_epoch().count()));
    }
    
    mpz_class solve(const mpz_class& alpha, const mpz_class& beta,
                    const mpz_class& p, const mpz_class& order,
                    const Timer& timer, bool& success) {
        success = false;
        
        Triple tortoise(1, 0, 0);
        Triple hare = tortoise;
        
        // Fase 1: Encontrar colisao
        int iterations = 0;
        while (true) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            // Tortoise: 1 passo
            tortoise = step(tortoise, alpha, beta, p, order);
            
            // Hare: 2 passos
            hare = step(hare, alpha, beta, p, order);
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            hare = step(hare, alpha, beta, p, order);
            
            iterations++;
            if (iterations % 100000 == 0) {
                // Checkpoint para timeout
                if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            }
            
            if (tortoise.x == hare.x) break;
        }
        
        // Fase 2: Resolver para o logaritmo
        mpz_class a_diff = (tortoise.a - hare.a) % order;
        mpz_class b_diff = (hare.b - tortoise.b) % order;
        
        if (b_diff == 0) {
            // Falha - reiniciar com parametros diferentes
            return 0;
        }
        
        // x = a_diff / b_diff mod order
        mpz_class b_diff_inv = modInv(b_diff, order);
        if (b_diff_inv == 0) return 0;
        
        mpz_class x = (a_diff * b_diff_inv) % order;
        if (x < 0) x += order;
        
        // Verificar
        mpz_class check = modPow(alpha, x, p);
        if (check == beta) {
            success = true;
            return x;
        }
        
        return 0;
    }
};

// ============================================================================
// TEOREMA CHINES DO RESTO (CRT)
// ============================================================================

mpz_class crt(const vector<mpz_class>& remainders, const vector<mpz_class>& moduli) {
    if (remainders.size() != moduli.size() || remainders.empty()) return 0;
    
    mpz_class result = remainders[0];
    mpz_class M = moduli[0];
    
    for (size_t i = 1; i < remainders.size(); ++i) {
        // Resolver: result + k*M = remainders[i] (mod moduli[i])
        mpz_class diff = (remainders[i] - result) % moduli[i];
        if (diff < 0) diff += moduli[i];
        
        mpz_class M_inv = modInv(M, moduli[i]);
        if (M_inv == 0) return 0; // Falha
        
        mpz_class k = (diff * M_inv) % moduli[i];
        result = result + k * M;
        M = M * moduli[i];
        result = result % M;
    }
    
    return result;
}

// ============================================================================
// ALGORITMO DE POHLIG-HELLMAN
// ============================================================================

class PohligHellmanSolver {
    Factorizer factorizer;
    BSGSSolver bsgs;
    PollardRhoDLP pollardRho;
    
    mpz_class solvePrimePower(const mpz_class& alpha, const mpz_class& beta,
                              const mpz_class& p, const mpz_class& q, int e,
                              const Timer& timer, bool& success) {
        success = false;
        
        mpz_class qe = 1;
        for (int i = 0; i < e; ++i) qe *= q;
        
        mpz_class x = 0;
        mpz_class gamma = 1;
        mpz_class alpha_q = modPow(alpha, (p - 1) / q, p);
        
        for (int i = 0; i < e; ++i) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            mpz_class q_i = 1;
            for (int j = 0; j < i; ++j) q_i *= q;
            
            // delta = (beta * alpha^(-x)) ^ ((p-1)/q^(i+1))
            mpz_class alpha_inv_x = modInv(modPow(alpha, x, p), p);
            if (alpha_inv_x == 0) return 0;
            
            mpz_class beta_prime = (beta * alpha_inv_x) % p;
            mpz_class exp = (p - 1) / (q * q_i);
            mpz_class delta = modPow(beta_prime, exp, p);
            
            // Resolver: alpha_q ^ x_i = delta (mod p)
            bool subSuccess = false;
            mpz_class x_i;
            
            // Escolher algoritmo baseado no tamanho de q
            if (q < BSGS_THRESHOLD) {
                x_i = bsgs.solve(alpha_q, delta, p, q, timer, subSuccess);
            } else {
                // Tentar Pollard's Rho para primos grandes
                x_i = pollardRho.solve(alpha_q, delta, p, q, timer, subSuccess);
                if (!subSuccess) {
                    x_i = bsgs.solve(alpha_q, delta, p, q, timer, subSuccess);
                }
            }
            
            if (!subSuccess) return 0;
            
            x = x + x_i * q_i;
            gamma = (gamma * modPow(alpha, x_i * q_i, p)) % p;
        }
        
        success = true;
        return x % qe;
    }

public:
    mpz_class solve(const mpz_class& alpha, const mpz_class& beta,
                    const mpz_class& p, const Timer& timer, bool& success) {
        success = false;
        
        // Passo 1: Fatorar p-1
        mpz_class p_minus_1 = p - 1;
        vector<mpz_class> factors = factorizer.factor(p_minus_1, timer);
        
        if (timer.timeout(TIMEOUT_SECONDS)) return 0;
        
        if (factors.empty()) {
            // p-1 eh primo - usar BSGS ou Pollard's Rho diretamente
            bool subSuccess = false;
            mpz_class result;
            
            if (p_minus_1 < BSGS_THRESHOLD) {
                result = bsgs.solve(alpha, beta, p, p_minus_1, timer, subSuccess);
            } else {
                result = pollardRho.solve(alpha, beta, p, p_minus_1, timer, subSuccess);
            }
            
            success = subSuccess;
            return result;
        }
        
        // Agrupar fatores primos
        map<mpz_class, int> primePowers;
        for (const auto& f : factors) {
            primePowers[f]++;
        }
        
        // Passo 2: Resolver DLP para cada potencia prima
        vector<mpz_class> remainders;
        vector<mpz_class> moduli;
        
        for (const auto& [q, e] : primePowers) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            mpz_class qe = 1;
            for (int i = 0; i < e; ++i) qe *= q;
            
            // Reduzir para subgrupo de ordem q^e
            mpz_class alpha_i = modPow(alpha, (p - 1) / qe, p);
            mpz_class beta_i = modPow(beta, (p - 1) / qe, p);
            
            bool subSuccess = false;
            mpz_class x_i = solvePrimePower(alpha_i, beta_i, p, q, e, timer, subSuccess);
            
            if (!subSuccess) {
                // Tentar BSGS direto no subgrupo
                x_i = bsgs.solve(alpha_i, beta_i, p, qe, timer, subSuccess);
            }
            
            if (!subSuccess) {
                // Tentar Pollard's Rho no subgrupo
                x_i = pollardRho.solve(alpha_i, beta_i, p, qe, timer, subSuccess);
            }
            
            if (!subSuccess) return 0;
            
            remainders.push_back(x_i);
            moduli.push_back(qe);
        }
        
        // Passo 3: Combinar usando CRT
        mpz_class x = crt(remainders, moduli);
        
        // Verificar
        if (modPow(alpha, x, p) == beta) {
            success = true;
            return x;
        }
        
        return 0;
    }
};

// ============================================================================
// SOLUCIONADOR PRINCIPAL
// ============================================================================

class DLPSolver {
    PohligHellmanSolver pohligHellman;
    
public:
    mpz_class solve(const Challenge& ch, const Timer& timer, bool& success) {
        success = false;
        
        // Encontrar 'a' tal que alpha^a = A (mod p)
        bool successA = false;
        mpz_class a = pohligHellman.solve(ch.alpha, ch.A, ch.p, timer, successA);
        
        if (!successA || timer.timeout(TIMEOUT_SECONDS)) {
            return 0;
        }
        
        // Calcular K_ab = B^a mod p
        mpz_class K_ab = modPow(ch.B, a, ch.p);
        
        success = true;
        return K_ab;
    }
};

// ============================================================================
// PARSING E I/O
// ============================================================================

vector<Challenge> parseChallenges(const string& filename) {
    vector<Challenge> challenges;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Erro: Nao foi possivel abrir " << filename << endl;
        return challenges;
    }
    
    string line;
    Challenge current;
    bool inChallenge = false;
    
    while (getline(file, line)) {
        // Ignorar comentarios e linhas vazias
        if (line.empty() || line[0] == '#') continue;
        
        // Verificar inicio de cenario
        if (line[0] == '[' && line.find("C") != string::npos) {
            if (inChallenge) {
                challenges.push_back(current);
            }
            
            current = Challenge();
            inChallenge = true;
            
            // Extrair ID e bits
            size_t cPos = line.find("C");
            size_t bracketPos = line.find("]", cPos);
            current.id = stoi(line.substr(cPos + 1, bracketPos - cPos - 1));
            
            size_t bitsPos = line.find("bits=");
            if (bitsPos != string::npos) {
                size_t spacePos = line.find(" ", bitsPos);
                if (spacePos == string::npos) spacePos = line.length();
                current.bits = stoi(line.substr(bitsPos + 5, spacePos - bitsPos - 5));
            }
        }
        else if (inChallenge) {
            // Parse de parametros
            size_t eqPos = line.find("=");
            if (eqPos != string::npos) {
                string key = line.substr(0, eqPos);
                string value = line.substr(eqPos + 1);
                
                // Trim key
                auto kstart = key.find_first_not_of(" \t");
                auto kend = key.find_last_not_of(" \t");
                if (kstart != string::npos) key = key.substr(kstart, kend - kstart + 1);
                
                // Trim value
                auto start = value.find_first_not_of(" \t");
                auto end = value.find_last_not_of(" \t");
                if (start != string::npos) {
                    value = value.substr(start, end - start + 1);
                }
                
                if (key.find("p") != string::npos && key.find("alpha") == string::npos) {
                    current.p = mpz_class(value);
                } else if (key.find("alpha") != string::npos) {
                    current.alpha = mpz_class(value);
                } else if (key == "A") {
                    current.A = mpz_class(value);
                } else if (key == "B") {
                    current.B = mpz_class(value);
                }
            }
        }
    }
    
    if (inChallenge) {
        challenges.push_back(current);
    }
    
    file.close();
    return challenges;
}

void writeSolutions(const string& filename, const vector<Solution>& solutions) {
    ofstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Erro: Nao foi possivel criar " << filename << endl;
        return;
    }
    
    for (const auto& sol : solutions) {
        if (sol.solved) {
            file << sol.K_ab << endl;
        } else {
            file << "# Timeout ou erro: " << sol.error << endl;
        }
    }
    
    file.close();
}

// Salvar uma solucao individual imediatamente (append mode)
void appendSolution(const string& filename, const Solution& sol, size_t index) {
    lock_guard<mutex> lock(g_fileMutex);
    
    // Verificar se ja foi salvo
    if (g_completed.size() > index && g_completed[index]) {
        return; // Ja foi salvo
    }
    
    ofstream file(filename, ios::app);
    if (!file.is_open()) {
        cerr << "[ERRO] Nao foi possivel abrir " << filename << " para append" << endl;
        return;
    }
    
    // Simplificacao: sempre escrevemos no final
    if (sol.solved) {
        file << "[C" << sol.id << "] " << sol.K_ab << endl;
    } else {
        file << "[C" << sol.id << "] # Timeout ou erro: " << sol.error << endl;
    }
    file.close();
    
    // Marcar como salvo
    if (g_completed.size() > index) {
        g_completed[index] = 1;
    }
}

// Inicializar arquivo com placeholders
void initSolutionFile(const string& filename, size_t numChallenges) {
    lock_guard<mutex> lock(g_fileMutex);
    
    // Criar/sobrescrever arquivo
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Erro: Nao foi possivel criar " << filename << endl;
        return;
    }
    
    // Escrever header
    file << "# DLP Solver - Solucoes" << endl;
    file << "# Gerado em: " << chrono::system_clock::now().time_since_epoch().count() << endl;
    file << "# Total de desafios: " << numChallenges << endl;
    file << "#" << endl;
    
    // Inicializar vetor de controle (0 = nao salvo)
    g_completed.resize(numChallenges, 0);
    
    file.close();
}

// ============================================================================
// PARALELISMO
// ============================================================================

void workerThread(const vector<Challenge>& challenges, vector<Solution>& solutions,
                  atomic<size_t>& nextIndex, mutex& solutionsMutex) {
    DLPSolver solver;
    
    while (g_running) {
        size_t idx = nextIndex.fetch_add(1);
        if (idx >= challenges.size()) break;
        
        const Challenge& ch = challenges[idx];
        Solution sol;
        sol.id = ch.id;
        
        cout << "[Thread " << this_thread::get_id() << "] Iniciando C" 
             << ch.id << " (" << ch.bits << " bits)..." << endl;
        
        Timer timer;
        timer.startTimer();
        
        bool success = false;
        mpz_class K_ab = solver.solve(ch, timer, success);
        
        sol.timeSeconds = timer.elapsed();
        sol.solved = success;
        
        if (success) {
            sol.K_ab = K_ab;
            cout << "[Thread " << this_thread::get_id() << "] C" 
                 << ch.id << " RESOLVIDO em " << fixed << setprecision(3) 
                 << sol.timeSeconds << "s" << endl;
        } else {
            sol.error = "Timeout apos " + to_string(static_cast<int>(sol.timeSeconds)) + "s";
            cout << "[Thread " << this_thread::get_id() << "] C" 
                 << ch.id << " TIMEOUT apos " << fixed << setprecision(3) 
                 << sol.timeSeconds << "s" << endl;
        }
        
        // Salvar no vetor
        {
            lock_guard<mutex> lock(solutionsMutex);
            solutions[idx] = sol;
        }
        
        // Salvar imediatamente no arquivo (incremental)
        if (g_running) {
            appendSolution(g_outputFile, sol, idx);
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

// Handler para SIGINT (Ctrl+C)
void signalHandler(int signum) {
    cout << endl << "[AVISO] Interrupcao recebida (Ctrl+C). Encerrando..." << endl;
    g_running = false;
    // Dar tempo para threads salvarem
    this_thread::sleep_for(chrono::milliseconds(500));
    exit(signum);
}

int main(int argc, char* argv[]) {
    // Registrar handler de sinal
    signal(SIGINT, signalHandler);
    #ifdef SIGTERM
    signal(SIGTERM, signalHandler);
    #endif
    
    string inputFile = "desafios.txt";
    string outputFile = "solucao.txt";
    
    if (argc > 1) inputFile = argv[1];
    if (argc > 2) outputFile = argv[2];
    
    g_outputFile = outputFile; // Global para threads
    
    cout << "========================================" << endl;
    cout << "DLP Solver - Pohlig-Hellman Optimizado" << endl;
    cout << "========================================" << endl;
    cout << "Threads disponiveis: " << MAX_THREADS << endl;
    cout << "Timeout por cenario: " << TIMEOUT_SECONDS << "s" << endl;
    cout << "Arquivo de entrada: " << inputFile << endl;
    cout << "Arquivo de saida: " << outputFile << endl;
    cout << "========================================" << endl << endl;
    
    // Parse dos desafios
    vector<Challenge> challenges = parseChallenges(inputFile);
    
    if (challenges.empty()) {
        cerr << "Nenhum desafio encontrado!" << endl;
        return 1;
    }
    
    cout << "Carregados " << challenges.size() << " desafios:" << endl;
    for (const auto& ch : challenges) {
        cout << "  C" << ch.id << ": " << ch.bits << " bits" << endl;
    }
    cout << endl;
    
    // Inicializar arquivo de solucoes (modo incremental)
    initSolutionFile(outputFile, challenges.size());
    
    // Preparar estruturas para paralelismo
    vector<Solution> solutions(challenges.size());
    atomic<size_t> nextIndex(0);
    mutex solutionsMutex;
    
    // Criar threads
    vector<thread> threads;
    int numThreads = min(MAX_THREADS, static_cast<int>(challenges.size()));
    
    cout << "Iniciando solucao com " << numThreads << " threads..." << endl << endl;
    
    auto startTime = chrono::steady_clock::now();
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, ref(challenges), ref(solutions),
                           ref(nextIndex), ref(solutionsMutex));
    }
    
    // Aguardar todas as threads
    for (auto& t : threads) {
        t.join();
    }
    
    auto endTime = chrono::steady_clock::now();
    double totalTime = chrono::duration<double>(endTime - startTime).count();
    
    cout << endl << "========================================" << endl;
    cout << "RESUMO" << endl;
    cout << "========================================" << endl;
    
    int solvedCount = 0;
    for (const auto& sol : solutions) {
        if (sol.solved) solvedCount++;
    }
    
    cout << "Total de desafios: " << challenges.size() << endl;
    cout << "Resolvidos: " << solvedCount << endl;
    cout << "Timeout: " << (challenges.size() - solvedCount) << endl;
    cout << "Tempo total: " << fixed << setprecision(3) << totalTime << "s" << endl;
    
    cout << endl << "Tempos individuais:" << endl;
    for (size_t i = 0; i < solutions.size(); ++i) {
        const auto& sol = solutions[i];
        cout << "  C" << challenges[i].id << " (" << challenges[i].bits << " bits): ";
        if (sol.solved) {
            cout << fixed << setprecision(3) << sol.timeSeconds << "s - K_ab = " << sol.K_ab;
        } else {
            cout << "TIMEOUT (" << fixed << setprecision(3) << sol.timeSeconds << "s)";
        }
        cout << endl;
    }
    
    // Salvar solucoes pendentes (caso alguma nao tenha sido salva)
    cout << endl << "Salvando solucoes pendentes..." << endl;
    for (size_t i = 0; i < solutions.size(); ++i) {
        bool alreadySaved = false;
        {
            lock_guard<mutex> lock(g_fileMutex);
            if (i < g_completed.size() && g_completed[i]) {
                alreadySaved = true;
            }
        }
        if (!alreadySaved) {
            appendSolution(outputFile, solutions[i], i);
        }
    }
    
    cout << endl << "Solucoes salvas em: " << outputFile << endl;
    cout << "(Arquivo salvo incrementalmente durante execucao)" << endl;
    
    return 0;
}
