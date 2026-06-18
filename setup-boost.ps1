# Script PowerShell para baixar e extrair Boost Multiprecision (header-only)
# Nao requer compilacao - apenas headers

param(
    [string]$BoostVersion = "1.84.0",
    [string]$InstallDir = "."
)

$ErrorActionPreference = "Stop"

function Write-Info($msg) {
    Write-Host "[INFO] $msg" -ForegroundColor Cyan
}

function Write-Success($msg) {
    Write-Host "[OK] $msg" -ForegroundColor Green
}

function Write-Error($msg) {
    Write-Host "[ERRO] $msg" -ForegroundColor Red
}

# URLs do Boost
$boostVersionUnderscore = $BoostVersion.Replace(".", "_")
$boostUrl = "https://boostorg.jfrog.io/artifactory/main/release/$BoostVersion/source/boost_$boostVersionUnderscore.7z"
$boostAltUrl = "https://archives.boost.io/release/$BoostVersion/source/boost_$boostVersionUnderscore.7z"

$boostArchive = Join-Path $InstallDir "boost_$boostVersionUnderscore.7z"
$extractDir = Join-Path $InstallDir "boost"

Write-Info "Boost Multiprecision Setup"
Write-Info "Versao: $BoostVersion"
Write-Info "Diretorio: $(Resolve-Path $InstallDir)"

# Verificar se ja existe
if (Test-Path (Join-Path $extractDir "boost\multiprecision\cpp_int.hpp")) {
    Write-Success "Boost ja instalado em: $extractDir"
    exit 0
}

# Criar diretorio temporario
$tempDir = Join-Path $env:TEMP "boost_setup_$(Get-Random)"
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

# Baixar Boost
try {
    Write-Info "Baixando Boost $BoostVersion..."
    Write-Info "URL: $boostUrl"
    
    # Verificar se tem 7z disponivel
    $sevenZip = $null
    $sevenZipPaths = @(
        "C:\Program Files\7-Zip\7z.exe",
        "C:\ProgramData\chocolatey\bin\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe",
        "7z"
    )
    
    foreach ($path in $sevenZipPaths) {
        if (Test-Path $path -ErrorAction SilentlyContinue) {
            $sevenZip = $path
            break
        }
        if (Get-Command $path -ErrorAction SilentlyContinue) {
            $sevenZip = $path
            break
        }
    }
    
    if (-not $sevenZip) {
        # Tentar usar Expand-Archive (mais lento, so funciona com .zip)
        Write-Info "7-Zip nao encontrado. Tentando download alternativo (.zip)..."
        
        $zipUrl = "https://github.com/boostorg/boost/releases/download/boost-$BoostVersion/boost-$BoostVersion.zip"
        $zipPath = Join-Path $tempDir "boost.zip"
        
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
        Write-Success "Download concluido: $([math]::Round((Get-Item $zipPath).Length / 1MB, 2)) MB"
        
        Write-Info "Extraindo (isso pode levar alguns minutos)..."
        Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force
        
        # Mover apenas os headers necessarios
        $sourceBoost = Join-Path $tempDir "boost-$BoostVersion\boost"
        if (Test-Path $sourceBoost) {
            Copy-Item -Path $sourceBoost -Destination $extractDir -Recurse -Force
        }
    } else {
        Write-Info "Usando 7-Zip: $sevenZip"
        
        # Download do .7z
        $archivePath = Join-Path $tempDir "boost.7z"
        
        try {
            Invoke-WebRequest -Uri $boostUrl -OutFile $archivePath -UseBasicParsing -TimeoutSec 300
        } catch {
            Write-Info "Primeira URL falhou, tentando alternativa..."
            Invoke-WebRequest -Uri $boostAltUrl -OutFile $archivePath -UseBasicParsing -TimeoutSec 300
        }
        
        Write-Success "Download concluido: $([math]::Round((Get-Item $archivePath).Length / 1MB, 2)) MB"
        
        # Extrair apenas os headers necessarios
        Write-Info "Extraindo headers necessarios..."
        
        # Extrair tudo primeiro
        & $sevenZip x "$archivePath" -o"$tempDir" -y -bso0
        
        if ($LASTEXITCODE -ne 0) {
            throw "Falha ao extrair com 7-Zip"
        }
        
        # Encontrar diretorio boost extraido
        $extractedDir = Get-ChildItem -Path $tempDir -Directory | Where-Object { $_.Name -like "boost_*" } | Select-Object -First 1
        
        if ($extractedDir) {
            $sourceBoost = Join-Path $extractedDir.FullName "boost"
            if (Test-Path $sourceBoost) {
                Copy-Item -Path $sourceBoost -Destination $extractDir -Recurse -Force
            }
        }
    }
    
    # Verificar se funcionou
    $multiprecisionHeader = Join-Path $extractDir "boost\multiprecision\cpp_int.hpp"
    if (Test-Path $multiprecisionHeader) {
        Write-Success "Boost Multiprecision instalado com sucesso!"
        Write-Info "Local: $(Resolve-Path $extractDir)"
        
        # Mostrar headers disponiveis
        $headers = @(
            "boost/multiprecision/cpp_int.hpp",
            "boost/multiprecision/cpp_dec_float.hpp",
            "boost/multiprecision/number.hpp"
        )
        
        Write-Info "Headers disponiveis:"
        foreach ($header in $headers) {
            $path = Join-Path $extractDir $header
            if (Test-Path $path) {
                Write-Host "  - $header" -ForegroundColor Green
            }
        }
        
        exit 0
    } else {
        throw "Header cpp_int.hpp nao encontrado apos extracao"
    }
    
} catch {
    Write-Error $_.Exception.Message
    Write-Error "Falha na instalacao do Boost"
    
    # Limpar
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    exit 1
    
} finally {
    # Limpeza
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
