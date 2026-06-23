/*
 * Solucao para o Problema do Logaritmo Discreto (DLP)
 * Metodo: Baby-step Giant-step
 *
 * Resolve A = alpha^a mod p usando BSGS e em seguida
 * calcula K_ab = B^a mod p.
 *
 * Compilacao:
 *   g++ -std=c++17 -O2 -o dlp_solver_bsgs dlp_solver_bsgs.cpp -lgmp -lgmpxx
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <csignal>
#include <atomic>
#include <gmpxx.h>

using namespace std;

atomic<bool> g_running{true};
ofstream* g_outFile = nullptr;

void signalHandler(int) {
    g_running = false;
    if (g_outFile != nullptr) {
        g_outFile->flush();
    }
    cerr << "\n[Interrompido] Salvando progresso e encerrando..." << endl;
    exit(130);
}

struct Challenge {
    int id;
    int bits;
    mpz_class p;
    mpz_class alpha;
    mpz_class A;
    mpz_class B;
};

vector<Challenge> parseChallenges(const string& filename) {
    vector<Challenge> challenges;
    ifstream in(filename);
    string line;
    Challenge current;
    bool hasCurrent = false;

    while (getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("[C") == 0) {
            if (hasCurrent) challenges.push_back(current);
            current = Challenge();
            hasCurrent = true;
            size_t end = line.find(']');
            current.id = stoi(line.substr(2, end - 2));
            size_t bitsPos = line.find("bits=");
            if (bitsPos != string::npos) current.bits = stoi(line.substr(bitsPos + 5));
            continue;
        }

        size_t eq = line.find('=');
        if (eq == string::npos || !hasCurrent) continue;
        string key = line.substr(0, eq);
        string value = line.substr(eq + 1);
        key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
        value.erase(remove_if(value.begin(), value.end(), ::isspace), value.end());

        if (key == "p") current.p = value;
        else if (key == "alpha") current.alpha = value;
        else if (key == "A") current.A = value;
        else if (key == "B") current.B = value;
    }
    if (hasCurrent) challenges.push_back(current);
    return challenges;
}

mpz_class modPow(const mpz_class& base, const mpz_class& exp, const mpz_class& mod) {
    mpz_class result;
    mpz_powm(result.get_mpz_t(), base.get_mpz_t(), exp.get_mpz_t(), mod.get_mpz_t());
    return result;
}

mpz_class modInv(const mpz_class& a, const mpz_class& mod) {
    mpz_class result;
    if (mpz_invert(result.get_mpz_t(), a.get_mpz_t(), mod.get_mpz_t()) == 0) {
        return 0;
    }
    return result;
}

mpz_class solveBSGS(const Challenge& c) {
    mpz_class n = c.p - 1;
    mpz_class m;
    mpz_sqrt(m.get_mpz_t(), n.get_mpz_t());
    m += 1;

    // Baby steps: alpha^j mod p
    map<mpz_class, mpz_class> table;
    mpz_class value = 1;
    for (mpz_class j = 0; j < m && g_running; ++j) {
        if (table.find(value) == table.end()) {
            table[value] = j;
        }
        value = (value * c.alpha) % c.p;
    }
    if (!g_running) return -1;

    // Giant steps: A * alpha^(-m*i) mod p
    mpz_class alpha_m = modPow(c.alpha, m, c.p);
    mpz_class alpha_m_inv = modInv(alpha_m, c.p);
    if (alpha_m_inv == 0) return -1;

    value = c.A;
    for (mpz_class i = 0; i < m && g_running; ++i) {
        auto it = table.find(value);
        if (it != table.end()) {
            mpz_class x = i * m + it->second;
            return modPow(c.B, x, c.p);
        }
        value = (value * alpha_m_inv) % c.p;
    }

    return -1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <arquivo_entrada> <arquivo_saida>" << endl;
        return 1;
    }

    signal(SIGINT, signalHandler);

    vector<Challenge> challenges = parseChallenges(argv[1]);
    ofstream out(argv[2]);
    g_outFile = &out;
    out << "# DLP Solver - Baby-step Giant-step\n";
    out.flush();

    for (const auto& c : challenges) {
        if (!g_running) break;

        cout << "[C" << c.id << "] bits=" << c.bits << " - resolvendo..." << endl;
        auto start = chrono::steady_clock::now();

        mpz_class K = solveBSGS(c);

        double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();

        if (K < 0) {
            out << "[C" << c.id << "] # Nao resolvido (timeout/limite excedido)\n";
            cout << "[C" << c.id << "] Nao resolvido" << endl;
        } else {
            out << "[C" << c.id << "] " << K << "\n";
            cout << "[C" << c.id << "] RESOLVIDO em " << elapsed << "s" << endl;
        }
        out.flush();
    }

    return 0;
}
