@echo off
chcp 65001 >nul
echo ========================================
echo Compilando DLP Solver v2 (Boost Edition)
echo ========================================
echo.

REM Verificar se boost existe
if not exist "boost\boost\multiprecision\cpp_int.hpp" (
    echo Boost nao encontrado. Baixando automaticamente...
    echo.
    powershell -ExecutionPolicy Bypass -File "%~dp0setup-boost.ps1"
    if errorlevel 1 (
        echo.
        echo Falha ao baixar Boost. Execute manualmente:
        echo   powershell -ExecutionPolicy Bypass -File .\setup-boost.ps1
        pause
        exit /b 1
    )
)

echo.
echo Boost encontrado!
echo.
echo Compilando dlp_solver_v2.cpp...
echo.

g++ -std=c++17 -O3 -march=native -Wall -I. -o dlp_solver_v2.exe dlp_solver_v2.cpp -pthread

if errorlevel 1 (
    echo.
    echo ========================================
    echo Erro na compilacao!
    echo ========================================
    echo.
    echo Possiveis causas:
    echo   1. g++ nao esta no PATH
    echo   2. Boost incompleto - delete pasta 'boost' e reexecute
    echo   3. Erro de sintaxe no codigo
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Compilacao concluida com sucesso!
echo ========================================
echo.
echo Executavel: dlp_solver_v2.exe
echo.
echo Para executar:
echo   dlp_solver_v2.exe
echo.
echo Ou com arquivos customizados:
echo   dlp_solver_v2.exe desafios.txt solucao_v2.txt
echo.

choice /C SE /N /M "Deseja executar agora? (S=Sim, E=Sair)"
if errorlevel 2 exit /b 0
if errorlevel 1 (
    echo.
    dlp_solver_v2.exe
)

pause
