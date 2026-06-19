# Makefile para DLP Solver
# Compativel com macOS e Windows (MinGW/MSYS2)

# Nome do executavel
TARGET = dlp_solver
TARGET_V2 = dlp_solver_v2

# Compilador
CXX = g++

# Auto-detect GMP installation
GMP_DETECTED := 0

# Auto-detect Boost installation
BOOST_DETECTED := 0

# Detectar sistema operacional
ifeq ($(OS),Windows_NT)
    # Windows
    RM = del /Q
    EXE = .exe
    THREAD_FLAGS = -pthread
    
    # Tentar detectar GMP em varios locais comuns no Windows
    ifneq (,$(wildcard C:/msys64/mingw64/include/gmp.h))
        # MSYS2 encontrado
        GMP_CFLAGS = -I/C/msys64/mingw64/include
        GMP_LDFLAGS = -L/C/msys64/mingw64/lib -lgmp -lgmpxx
        GMP_DETECTED := 1
    else ifneq (,$(wildcard C:/TDM-GCC-64/include/gmp.h))
        # TDM-GCC encontrado
        GMP_CFLAGS = -IC:/TDM-GCC-64/include
        GMP_LDFLAGS = -LC:/TDM-GCC-64/lib -lgmp -lgmpxx
        GMP_DETECTED := 1
    else ifneq (,$(wildcard /mingw64/include/gmp.h))
        # MinGW64 encontrado
        GMP_CFLAGS = -I/mingw64/include
        GMP_LDFLAGS = -L/mingw64/lib -lgmp -lgmpxx
        GMP_DETECTED := 1
    else
        # Fallback - usuario deve instalar GMP
        GMP_CFLAGS = 
        GMP_LDFLAGS = -lgmp -lgmpxx
    endif
else
    UNAME_S := $(shell uname -s)
    
    ifeq ($(UNAME_S),Darwin)
        # macOS - GMP via Homebrew
        ifneq (,$(wildcard /opt/homebrew/include/gmp.h))
            GMP_CFLAGS = -I/opt/homebrew/include -I/usr/local/include
            GMP_LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lgmp -lgmpxx
            GMP_DETECTED := 1
        else ifneq (,$(wildcard /usr/local/include/gmp.h))
            GMP_CFLAGS = -I/usr/local/include
            GMP_LDFLAGS = -L/usr/local/lib -lgmp -lgmpxx
            GMP_DETECTED := 1
        else
            GMP_CFLAGS = 
            GMP_LDFLAGS = -lgmp -lgmpxx
        endif
        ifneq (,$(wildcard /opt/homebrew/include/boost/multiprecision/cpp_int.hpp))
            BOOST_CFLAGS = -I/opt/homebrew/include
            BOOST_DETECTED := 1
        else ifneq (,$(wildcard /usr/local/include/boost/multiprecision/cpp_int.hpp))
            BOOST_CFLAGS = -I/usr/local/include
            BOOST_DETECTED := 1
        else
            BOOST_CFLAGS =
        endif
    else
        # Linux - GMP padrao
        GMP_CFLAGS = 
        GMP_LDFLAGS = -lgmp -lgmpxx
        GMP_DETECTED := 1
        ifneq (,$(wildcard /usr/include/boost/multiprecision/cpp_int.hpp))
            BOOST_CFLAGS = 
            BOOST_DETECTED := 1
        else
            BOOST_CFLAGS =
        endif
    endif
    
    RM = rm -f
    EXE =
    THREAD_FLAGS = -pthread
endif

# Flags de compilacao
CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra $(GMP_CFLAGS)
LDFLAGS = $(THREAD_FLAGS) $(GMP_LDFLAGS)

# Modo debug (opcional)
ifdef DEBUG
    CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra $(GMP_CFLAGS)
endif

# Fontes v1
SRCS = dlp_solver.cpp
OBJS = $(SRCS:.cpp=.o)

# Fontes v2
SRCS_V2 = dlp_solver_v2.cpp
OBJS_V2 = $(SRCS_V2:.cpp=.o)
CXXFLAGS_V2 = -std=c++17 -O3 -march=native -Wall -Wextra $(BOOST_CFLAGS)

# Alvo padrao
all: $(TARGET)$(EXE)

# Compilar
$(TARGET)$(EXE): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Regra de compilacao
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Limpar
clean:
	$(RM) $(OBJS) $(TARGET)$(EXE) $(OBJS_V2) $(TARGET_V2)$(EXE) solucao.txt solucao_v2.txt

# Executar v1
run: $(TARGET)$(EXE)
	./$(TARGET)$(EXE) desafios.txt solucao.txt

# Compilar v2
$(TARGET_V2)$(EXE): $(OBJS_V2)
	$(CXX) $(OBJS_V2) -o $@ $(THREAD_FLAGS)

$(OBJS_V2): $(SRCS_V2)
	$(CXX) $(CXXFLAGS_V2) -c $(SRCS_V2) -o $(OBJS_V2)

# Executar v2
run2: $(TARGET_V2)$(EXE)
	./$(TARGET_V2)$(EXE) desafios.txt solucao_v2.txt

# Teste rapido (apenas C0)
test: $(TARGET)$(EXE)
	@echo "# Trabalho Pratico --- Logaritmo Discreto" > test_input.txt
	@echo "[C0] bits=16" >> test_input.txt
	@echo "p = 36467" >> test_input.txt
	@echo "alpha = 20347" >> test_input.txt
	@echo "A = 13686" >> test_input.txt
	@echo "B = 8710" >> test_input.txt
	./$(TARGET)$(EXE) test_input.txt test_output.txt
	@del /Q test_input.txt 2>nul || rm -f test_input.txt

# Verificar se GMP esta instalado
check:
	@echo "=============================================="
	@echo "Verificando instalacao do GMP..."
	@echo "=============================================="
ifeq ($(GMP_DETECTED),1)
	@echo "GMP DETECTADO!"
	@echo "CFLAGS: $(GMP_CFLAGS)"
	@echo "LDFLAGS: $(GMP_LDFLAGS)"
else
	@echo "ATENCAO: GMP NAO DETECTADO!"
	@echo ""
	@echo "Para instalar GMP:"
	@echo "  macOS:    brew install gmp"
	@echo "  MSYS2:    pacman -S mingw-w64-x86_64-gmp"
	@echo "  Ubuntu:   sudo apt-get install libgmp-dev"
	@echo "  Fedora:   sudo dnf install gmp-devel"
	@echo ""
	@echo "Ou especifique manualmente:"
	@echo "  make GMP_CFLAGS=-I/caminho/include GMP_LDFLAGS=\"-L/caminho/lib -lgmp -lgmpxx\""
	@echo "=============================================="
	@exit 1
endif
	@echo "Compilador: $(CXX)"
	$(CXX) --version

# Instalacao do GMP via Homebrew (macOS)
install-gmp-mac:
	brew install gmp

# Instalacao do GMP via MSYS2 (Windows)
install-gmp-win:
	@echo "Execute no MSYS2: pacman -S mingw-w64-x86_64-gmp"

.PHONY: all clean run run2 test check install-gmp-mac install-gmp-win
