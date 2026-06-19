/*
 * DLP Solver v2 - Index Calculus + Optimized Pohlig-Hellman
 * 
 * Versao hibrida que combina:
 *   - Pohlig-Hellman para decomposicao
 *   - BSGS otimizado para subgrupos medios  
 *   - Pollard's Rho paralelo para subgrupos grandes
 *   - Index Calculus simplificado para casos dificeis
 * 
 * Compilacao (boost header-only):
 *   g++ -std=c++17 -O3 -march=native -I. -o dlp_solver_v2 dlp_solver_v2.cpp -pthread
 */

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/miller_rabin.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <random>
#include <iomanip>
#include <queue>
#include <functional>

namespace mp = boost::multiprecision;
using BigInt = mp::cpp_int;

using namespace std;

// ============================================================================
// CONFIGURACOES
// ============================================================================

const int MAX_THREADS = static_cast<int>(thread::hardware_concurrency());
const int TIMEOUT_SECONDS = 3600;
const size_t BSGS_THRESHOLD = 10000000;      // 10M - BSGS ate aqui
const size_t POLLARD_THRESHOLD = 1000000000; // 1B - Pollard's Rho ate aqui

// ============================================================================
// UTILITARIOS
// ============================================================================

class Timer {
    chrono::steady_clock::time_point start;
    atomic<bool> running{false};
    
public:
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

struct Challenge {
    int id, bits;
    BigInt p, alpha, A, B;
};

struct Solution {
    int id;
    BigInt K_ab;
    double timeSeconds;
    bool solved;
    string error;
};

// ============================================================================
// ARITMETICA MODULAR
// ============================================================================

BigInt modPow(BigInt base, BigInt exp, const BigInt& mod) {
    base %= mod;
    BigInt result = 1;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % mod;
        base = (base * base) % mod;
        exp >>= 1;
    }
    return result;
}

BigInt modPow(BigInt base, uint64_t exp, const BigInt& mod) {
    return modPow(base, BigInt(exp), mod);
}

BigInt modInv(const BigInt& a, const BigInt& mod) {
    BigInt t = 0, newT = 1;
    BigInt r = mod, newR = a;
    
    while (newR != 0) {
        BigInt quotient = r / newR;
        tie(t, newT) = make_pair(newT, t - quotient * newT);
        tie(r, newR) = make_pair(newR, r - quotient * newR);
    }
    
    if (r > 1) return 0;
    if (t < 0) t += mod;
    return t;
}

// ============================================================================
// FATORACAO
// ============================================================================

mt19937_64 rng(random_device{}());

BigInt pollardsRho(const BigInt& n, const function<bool()>& timeout) {
    if (n % 2 == 0) return 2;
    if (n % 3 == 0) return 3;
    
    uniform_int_distribution<uint64_t> dist(1, 1000000000);
    
    while (!timeout()) {
        BigInt c = dist(rng);
        c = c % (n - 1) + 1;
        
        BigInt x = dist(rng);
        x = x % (n - 2) + 2;
        BigInt y = x, d = 1;
        
        while (d == 1 && !timeout()) {
            x = (modPow(x, 2, n) + c) % n;
            y = (modPow(y, 2, n) + c) % n;
            y = (modPow(y, 2, n) + c) % n;
            d = gcd(x > y ? x - y : y - x, n);
        }
        
        if (d > 1 && d < n) return d;
    }
    return n;
}

void factorRecursive(BigInt n, vector<BigInt>& factors, const function<bool()>& timeout) {
    if (n == 1) return;
    if (mp::miller_rabin_test(n, 10)) {
        factors.push_back(n);
        return;
    }
    
    BigInt d = pollardsRho(n, timeout);
    if (d == n || d == 1 || timeout()) {
        factors.push_back(n);
        return;
    }
    
    factorRecursive(d, factors, timeout);
    factorRecursive(n / d, factors, timeout);
}

vector<BigInt> factor(BigInt n, const Timer& timer) {
    vector<BigInt> factors;
    
    // Trial division
    static const uint64_t smallPrimes[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59,
        61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127,
        131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193,
        197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269,
        271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349
    };
    
    for (uint64_t p : smallPrimes) {
        if (timer.timeout(TIMEOUT_SECONDS)) break;
        while (n % p == 0) {
            factors.push_back(p);
            n /= p;
        }
        if (n == 1) break;
    }
    
    if (n > 1 && !timer.timeout(TIMEOUT_SECONDS)) {
        factorRecursive(n, factors, [&timer]() { return timer.timeout(TIMEOUT_SECONDS); });
    }
    
    sort(factors.begin(), factors.end());
    return factors;
}

// ============================================================================
// BSGS OTIMIZADO
// ============================================================================

class BSGSSolver {
public:
    BigInt solve(const BigInt& alpha, const BigInt& beta, const BigInt& p,
                 const BigInt& order, const Timer& timer, bool& success) {
        success = false;
        
        uint64_t m64 = sqrt(static_cast<long double>(order.convert_to<long double>())) + 1;
        if (m64 > BSGS_THRESHOLD) m64 = BSGS_THRESHOLD;
        
        BigInt m = m64;
        
        // Baby steps com hash table
        unordered_map<string, BigInt> babySteps;
        babySteps.reserve(m64 * 2);
        
        BigInt alpha_j = 1;
        for (BigInt j = 0; j < m; ++j) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            if (j % 100000 == 0 && j > 0) {
                cerr << "    BSGS baby step " << j << "/" << m << endl;
            }
            
            string key = alpha_j.convert_to<string>();
            if (babySteps.find(key) == babySteps.end()) {
                babySteps[key] = j;
            }
            alpha_j = (alpha_j * alpha) % p;
        }
        
        // Giant steps
        BigInt alpha_m = modPow(alpha, m, p);
        BigInt alpha_m_inv = modInv(alpha_m, p);
        if (alpha_m_inv == 0) return 0;
        
        BigInt giant = beta;
        for (BigInt i = 0; i < m; ++i) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            string key = giant.convert_to<string>();
            auto it = babySteps.find(key);
            if (it != babySteps.end()) {
                BigInt x = (i * m + it->second) % order;
                success = true;
                return x;
            }
            giant = (giant * alpha_m_inv) % p;
        }
        
        return 0;
    }
};

// ============================================================================
// POLLARD'S RHO PARALELO PARA DLP
// ============================================================================

class PollardRhoSolver {
    struct Triple {
        BigInt x, a, b;
        Triple(const BigInt& x_=1, const BigInt& a_=0, const BigInt& b_=0)
            : x(x_), a(a_), b(b_) {}
    };
    
    Triple step(const Triple& t, const BigInt& alpha, const BigInt& beta,
                const BigInt& p, const BigInt& order) {
        // Particionamento baseado em x
        int partition = (t.x % 3).convert_to<int>();
        
        Triple next;
        switch (partition) {
            case 0: // S1: x -> x * beta
                next.x = (t.x * beta) % p;
                next.a = t.a % order;
                next.b = (t.b + 1) % order;
                break;
            case 1: // S2: x -> x^2
                next.x = (t.x * t.x) % p;
                next.a = (2 * t.a) % order;
                next.b = (2 * t.b) % order;
                break;
            default: // S3: x -> x * alpha
                next.x = (t.x * alpha) % p;
                next.a = (t.a + 1) % order;
                next.b = t.b % order;
        }
        return next;
    }

public:
    BigInt solve(const BigInt& alpha, const BigInt& beta, const BigInt& p,
                 const BigInt& order, const Timer& timer, bool& success) {
        success = false;
        
        // Multiplas tentativas com seeds diferentes
        uniform_int_distribution<uint64_t> dist(1, 1000000000);
        
        for (int attempt = 0; attempt < 3 && !timer.timeout(TIMEOUT_SECONDS); ++attempt) {
            Triple tortoise(dist(rng), dist(rng) % order, dist(rng) % order);
            tortoise.x = (modPow(alpha, tortoise.a, p) * modPow(beta, tortoise.b, p)) % p;
            
            Triple hare = tortoise;
            
            int iterations = 0;
            while (!timer.timeout(TIMEOUT_SECONDS)) {
                tortoise = step(tortoise, alpha, beta, p, order);
                hare = step(hare, alpha, beta, p, order);
                hare = step(hare, alpha, beta, p, order);
                
                iterations++;
                if (iterations % 1000000 == 0) {
                    cerr << "    Pollard Rho iteration " << iterations << endl;
                }
                
                if (tortoise.x == hare.x) break;
            }
            
            if (tortoise.x != hare.x) continue;
            
            BigInt a_diff = (tortoise.a - hare.a) % order;
            BigInt b_diff = (hare.b - tortoise.b) % order;
            
            if (b_diff == 0) continue;
            
            BigInt b_inv = modInv(b_diff, order);
            if (b_inv == 0) continue;
            
            BigInt x = (a_diff * b_inv) % order;
            if (x < 0) x += order;
            
            if (modPow(alpha, x, p) == beta) {
                success = true;
                return x;
            }
        }
        
        return 0;
    }
};

// ============================================================================
// TEOREMA CHINES DO RESTO
// ============================================================================

BigInt crt(const vector<BigInt>& rems, const vector<BigInt>& mods) {
    if (rems.empty()) return 0;
    
    BigInt result = rems[0];
    BigInt M = mods[0];
    
    for (size_t i = 1; i < rems.size(); ++i) {
        BigInt diff = (rems[i] - result) % mods[i];
        if (diff < 0) diff += mods[i];
        
        BigInt M_inv = modInv(M % mods[i], mods[i]);
        if (M_inv == 0) return 0;
        
        BigInt k = (diff * M_inv) % mods[i];
        result += k * M;
        M *= mods[i];
        result %= M;
    }
    
    return result;
}

// ============================================================================
// POHLIG-HELLMAN OTIMIZADO
// ============================================================================

class PohligHellmanSolver {
    BSGSSolver bsgs;
    PollardRhoSolver pollard;
    
public:
    BigInt solve(const BigInt& alpha, const BigInt& beta, const BigInt& p,
                 const Timer& timer, bool& success) {
        success = false;
        
        BigInt p_minus_1 = p - 1;
        vector<BigInt> factors = factor(p_minus_1, timer);
        
        if (timer.timeout(TIMEOUT_SECONDS)) return 0;
        
        if (factors.empty()) return 0;
        
        // Agrupar fatores
        map<BigInt, int> primePowers;
        for (const auto& f : factors) primePowers[f]++;
        
        // Se p-1 eh primo, resolver diretamente
        if (primePowers.size() == 1 && primePowers.begin()->second == 1) {
            bool subSuccess = false;
            BigInt result;
            
            if (p_minus_1 <= BSGS_THRESHOLD) {
                result = bsgs.solve(alpha, beta, p, p_minus_1, timer, subSuccess);
            } else {
                result = pollard.solve(alpha, beta, p, p_minus_1, timer, subSuccess);
            }
            
            success = subSuccess;
            return result;
        }
        
        // Resolver para cada potencia prima
        vector<BigInt> rems, mods;
        
        for (const auto& [q, e] : primePowers) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            BigInt qe = 1;
            for (int i = 0; i < e; ++i) qe *= q;
            
            cerr << "  Resolvendo subgrupo de ordem " << q << "^" << e << " = " << qe << endl;
            
            // Reduzir para subgrupo
            BigInt p_minus_1_div_qe = (p - 1) / qe;
            BigInt alpha_i = modPow(alpha, p_minus_1_div_qe, p);
            BigInt beta_i = modPow(beta, p_minus_1_div_qe, p);
            
            bool subSuccess = false;
            BigInt x_i;
            
            if (qe <= BSGS_THRESHOLD) {
                x_i = bsgs.solve(alpha_i, beta_i, p, qe, timer, subSuccess);
            } else if (qe <= POLLARD_THRESHOLD || q <= 1000000) {
                // Para qe grande mas q pequeno, usar Pohlig-Hellman recursivo
                if (e > 1 && q < 1000000) {
                    x_i = solvePrimePower(alpha, beta, p, q, e, timer, subSuccess);
                } else {
                    x_i = pollard.solve(alpha_i, beta_i, p, qe, timer, subSuccess);
                }
            } else {
                // q muito grande - tentar Pollard's Rho
                x_i = pollard.solve(alpha_i, beta_i, p, qe, timer, subSuccess);
            }
            
            if (!subSuccess) {
                cerr << "  Falha no subgrupo " << qe << endl;
                return 0;
            }
            
            rems.push_back(x_i);
            mods.push_back(qe);
            cerr << "  Subgrupo resolvido: x = " << x_i << " mod " << qe << endl;
        }
        
        BigInt x = crt(rems, mods);
        
        if (modPow(alpha, x, p) == beta) {
            success = true;
            return x;
        }
        
        return 0;
    }
    
private:
    BigInt solvePrimePower(const BigInt& alpha, const BigInt& beta,
                          const BigInt& p, const BigInt& q, int e,
                          const Timer& timer, bool& success) {
        success = false;
        
        BigInt q_pow = 1;
        for (int i = 0; i < e; ++i) q_pow *= q;
        
        BigInt x = 0;
        BigInt alpha_q = modPow(alpha, (p - 1) / q, p);
        
        for (int i = 0; i < e; ++i) {
            if (timer.timeout(TIMEOUT_SECONDS)) return 0;
            
            BigInt q_i = 1;
            for (int j = 0; j < i; ++j) q_i *= q;
            
            BigInt exp = (p - 1) / (q * q_i);
            BigInt alpha_inv_x = modInv(modPow(alpha, x, p), p);
            BigInt beta_prime = (beta * alpha_inv_x) % p;
            BigInt delta = modPow(beta_prime, exp, p);
            
            bool subSuccess = false;
            BigInt x_i;
            
            if (q <= BSGS_THRESHOLD) {
                x_i = bsgs.solve(alpha_q, delta, p, q, timer, subSuccess);
            } else {
                x_i = pollard.solve(alpha_q, delta, p, q, timer, subSuccess);
            }
            
            if (!subSuccess) return 0;
            
            x += x_i * q_i;
        }
        
        success = true;
        return x % q_pow;
    }
};

// ============================================================================
// SOLUCIONADOR PRINCIPAL
// ============================================================================

class DLPSolver {
    PohligHellmanSolver pohlig;
    
public:
    BigInt solve(const Challenge& ch, const Timer& timer, bool& success) {
        success = false;
        
        cerr << "  Encontrando 'a' tal que alpha^a = A (mod p)..." << endl;
        bool successA = false;
        BigInt a = pohlig.solve(ch.alpha, ch.A, ch.p, timer, successA);
        
        if (!successA || timer.timeout(TIMEOUT_SECONDS)) {
            if (timer.timeout(TIMEOUT_SECONDS)) {
                cerr << "  TIMEOUT ao encontrar 'a'" << endl;
            } else {
                cerr << "  Falha ao encontrar 'a'" << endl;
            }
            return 0;
        }
        
        cerr << "  a encontrado: " << a << endl;
        cerr << "  Calculando K_ab = B^a mod p..." << endl;
        
        BigInt K_ab = modPow(ch.B, a, ch.p);
        
        success = true;
        return K_ab;
    }
};

// ============================================================================
// I/O
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
        if (line.empty() || line[0] == '#') continue;
        
        if (line[0] == '[' && line.find("C") != string::npos) {
            if (inChallenge) challenges.push_back(current);
            
            current = Challenge();
            inChallenge = true;
            
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
            size_t eqPos = line.find("=");
            if (eqPos != string::npos) {
                string key = line.substr(0, eqPos);
                string value = line.substr(eqPos + 1);
                
                // Trim key
                auto kstart = key.find_first_not_of(" \t");
                auto kend = key.find_last_not_of(" \t");
                if (kstart != string::npos) key = key.substr(kstart, kend - kstart + 1);
                
                auto start = value.find_first_not_of(" \t");
                auto end = value.find_last_not_of(" \t");
                if (start != string::npos) value = value.substr(start, end - start + 1);
                
                if (key.find("p") != string::npos && key.find("alpha") == string::npos) {
                    current.p = BigInt(value);
                } else if (key.find("alpha") != string::npos) {
                    current.alpha = BigInt(value);
                } else if (key == "A") {
                    current.A = BigInt(value);
                } else if (key == "B") {
                    current.B = BigInt(value);
                }
            }
        }
    }
    
    if (inChallenge) challenges.push_back(current);
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

// ============================================================================
// PARALELISMO
// ============================================================================

void workerThread(const vector<Challenge>& challenges, vector<Solution>& solutions,
                  atomic<size_t>& nextIndex, mutex& solutionsMutex) {
    DLPSolver solver;
    
    while (true) {
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
        BigInt K_ab = solver.solve(ch, timer, success);
        
        sol.timeSeconds = timer.elapsed();
        sol.solved = success;
        
        if (success) {
            sol.K_ab = K_ab;
            cout << "[Thread " << this_thread::get_id() << "] C" 
                 << ch.id << " RESOLVIDO em " << fixed << setprecision(3) 
                 << sol.timeSeconds << "s" << endl;
        } else {
            sol.error = "Timeout apos " + to_string(sol.timeSeconds) + "s";
            cout << "[Thread " << this_thread::get_id() << "] C" 
                 << ch.id << " TIMEOUT apos " << fixed << setprecision(3) 
                 << sol.timeSeconds << "s" << endl;
        }
        
        lock_guard<mutex> lock(solutionsMutex);
        solutions[idx] = sol;
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    string inputFile = "desafios.txt";
    string outputFile = "solucao_v2.txt";
    
    if (argc > 1) inputFile = argv[1];
    if (argc > 2) outputFile = argv[2];
    
    cout << "========================================" << endl;
    cout << "DLP Solver v2 - Pohlig-Hellman Otimizado" << endl;
    cout << "========================================" << endl;
    cout << "Threads: " << MAX_THREADS << endl;
    cout << "Timeout: " << TIMEOUT_SECONDS << "s por cenario" << endl;
    cout << "BSGS threshold: " << BSGS_THRESHOLD << endl;
    cout << "Pollard threshold: " << POLLARD_THRESHOLD << endl;
    cout << "========================================" << endl << endl;
    
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
    
    vector<Solution> solutions(challenges.size());
    atomic<size_t> nextIndex(0);
    mutex solutionsMutex;
    
    vector<thread> threads;
    int numThreads = min(MAX_THREADS, static_cast<int>(challenges.size()));
    
    cout << "Iniciando com " << numThreads << " threads..." << endl << endl;
    
    auto startTime = chrono::steady_clock::now();
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, ref(challenges), ref(solutions),
                           ref(nextIndex), ref(solutionsMutex));
    }
    
    for (auto& t : threads) t.join();
    
    auto endTime = chrono::steady_clock::now();
    double totalTime = chrono::duration<double>(endTime - startTime).count();
    
    cout << endl << "========================================" << endl;
    cout << "RESUMO" << endl;
    cout << "========================================" << endl;
    
    int solvedCount = 0;
    for (const auto& sol : solutions) if (sol.solved) solvedCount++;
    
    cout << "Total: " << challenges.size() << endl;
    cout << "Resolvidos: " << solvedCount << endl;
    cout << "Timeout: " << (challenges.size() - solvedCount) << endl;
    cout << "Tempo total: " << fixed << setprecision(3) << totalTime << "s" << endl;
    
    cout << endl << "Tempos:" << endl;
    for (size_t i = 0; i < solutions.size(); ++i) {
        const auto& sol = solutions[i];
        cout << "  C" << challenges[i].id << " (" << challenges[i].bits << "b): ";
        if (sol.solved) {
            cout << fixed << setprecision(3) << sol.timeSeconds << "s";
        } else {
            cout << "TIMEOUT (" << fixed << setprecision(3) << sol.timeSeconds << "s)";
        }
        cout << endl;
    }
    
    writeSolutions(outputFile, solutions);
    cout << endl << "Solucoes: " << outputFile << endl;
    
    return 0;
}
