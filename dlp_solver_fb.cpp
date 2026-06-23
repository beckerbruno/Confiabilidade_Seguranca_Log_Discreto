/*
 * Solucao para o Problema do Logaritmo Discreto (DLP)
 * Metodo: Forca Bruta (Ridiculamente Simples)
 *
 * Resolve A = alpha^a mod p testando a = 0, 1, 2, ...
 * e em seguida calcula K_ab = B^a mod p.
 *
 * Compilacao:
 *   g++ -std=c++17 -O2 -o dlp_solver_fb dlp_solver_fb.cpp -lgmp -lgmpxx
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
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

mpz_class solveBruteForce(const Challenge& c) {
    mpz_class x = 0;
    while (x < c.p - 1 && g_running) {
        if (modPow(c.alpha, x, c.p) == c.A) {
            return modPow(c.B, x, c.p);
        }
        x += 1;
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
    out << "# DLP Solver - Forca Bruta\n";
    out.flush();

    for (const auto& c : challenges) {
        if (!g_running) break;

        cout << "[C" << c.id << "] bits=" << c.bits << " - resolvendo..." << endl;
        auto start = chrono::steady_clock::now();

        mpz_class K = solveBruteForce(c);

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
