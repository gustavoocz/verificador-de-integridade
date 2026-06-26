# stress.ps1 — Teste de fogo automatizado do verificador de integridade
#
# Fluxo:
#   1. Cria imagem aleatória (N blocos x BSZ bytes)
#   2. Constrói a Merkle tree (mkverity)
#   3. Verifica todos os blocos antes da corrupção (devem passar)
#   4. Corrompe 1 byte de 1 bloco escolhido
#   5. Verifica bloco corrompido (deve FALHAR)
#   6. Verifica todos os outros blocos (devem PASSAR)
#   7. Reporta resultado final: PASSOU / FALHOU
#
# Uso:
#   .\scripts\stress.ps1
#   .\scripts\stress.ps1 -Blocks 128 -BlockSize 4096 -CorruptBlock 50
#
# Tema 14 — Verificador de Integridade de Disco
# Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro

param(
    [int]$Blocks       = 64,
    [int]$BlockSize    = 4096,
    [int]$CorruptBlock = 10,
    [int]$CorruptOffset = 42
)

$img = "build\_stress.img"
$vrt = "build\_stress.verity"
$passed = $true

function Log($msg, $color = "White") {
    Write-Host $msg -ForegroundColor $color
}

Log "========================================" Cyan
Log " TESTE DE FOGO — Verificador de Disco  " Cyan
Log "========================================" Cyan
Log "Blocos: $Blocks | BlockSize: ${BlockSize}B | Corrompendo bloco: $CorruptBlock`n"

# ── 1. Criar imagem ─────────────────────────────────────────────────────────
Log "[1/6] Criando imagem aleatoria ($Blocks x $BlockSize bytes)..."
python -c "import os; open('$img','wb').write(os.urandom($Blocks * $BlockSize))"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $img)) {
    Log "ERRO: falha ao criar imagem (python disponivel?)" Red
    exit 1
}
$size_kb = [math]::Round(($Blocks * $BlockSize) / 1024, 1)
Log "    Imagem: $img ($size_kb KB)" Green

# ── 2. Construir árvore ──────────────────────────────────────────────────────
Log "[2/6] Construindo arvore de hash (mkverity)..."
.\build\mkverity $img $vrt $BlockSize | Out-Null
if ($LASTEXITCODE -ne 0) {
    Log "ERRO: mkverity falhou" Red; $passed = $false
} else {
    Log "    Arvore: $vrt" Green
}

# ── 3. Verificar antes da corrupção ─────────────────────────────────────────
Log "[3/6] Verificando todos os blocos (pre-corrupcao)..."
$pre_fail = 0
for ($b = 0; $b -lt $Blocks; $b++) {
    .\build\verify_block $img $vrt $b 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { $pre_fail++ }
}
if ($pre_fail -eq 0) {
    Log "    $Blocks/$Blocks blocos OK" Green
} else {
    Log "    ERRO: $pre_fail blocos falharam antes da corrupcao!" Red
    $passed = $false
}

# ── 4. Corromper 1 byte ──────────────────────────────────────────────────────
Log "[4/6] Corrompendo bloco $CorruptBlock, byte $CorruptOffset..."
$corrupt_out = .\build\corrupt $img $CorruptBlock $CorruptOffset FF
Log "    $corrupt_out" Yellow

# ── 5. Verificar bloco corrompido ────────────────────────────────────────────
Log "[5/6] Verificando bloco corrompido ($CorruptBlock)..."
.\build\verify_block $img $vrt $CorruptBlock 2>&1 | Out-Null
if ($LASTEXITCODE -eq 1) {
    Log "    [DETECTADO] Corrupcao detectada corretamente" Green
} else {
    Log "    [FALHOU] Corrupcao NAO detectada!" Red
    $passed = $false
}

# ── 6. Verificar demais blocos ───────────────────────────────────────────────
Log "[6/6] Verificando demais blocos pos-corrupcao..."
$post_fail = 0
for ($b = 0; $b -lt $Blocks; $b++) {
    if ($b -eq $CorruptBlock) { continue }
    .\build\verify_block $img $vrt $b 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { $post_fail++ }
}
$expected_ok = $Blocks - 1
if ($post_fail -eq 0) {
    Log "    $expected_ok/$expected_ok blocos restantes OK" Green
} else {
    Log "    ERRO: $post_fail blocos falharam apos corrupcao de outro bloco!" Red
    $passed = $false
}

# ── Limpeza ──────────────────────────────────────────────────────────────────
Remove-Item $img, $vrt -Force -ErrorAction SilentlyContinue

# ── Resultado ────────────────────────────────────────────────────────────────
Log ""
if ($passed) {
    Log "========================================" Green
    Log " RESULTADO: PASSOU                     " Green
    Log "========================================" Green
    exit 0
} else {
    Log "========================================" Red
    Log " RESULTADO: FALHOU                     " Red
    Log "========================================" Red
    exit 1
}
