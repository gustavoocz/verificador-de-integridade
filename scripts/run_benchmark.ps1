# run_benchmark.ps1 — Coleta dados de desempenho com diferentes tamanhos de cache
#
# Cria uma imagem de teste, constrói a árvore e executa `bench` com
# capacidades de cache variando de 0 a 128. Imprime tabela comparativa
# e salva resultado em build/benchmark_results.csv.
#
# Uso:
#   .\scripts\run_benchmark.ps1
#   .\scripts\run_benchmark.ps1 -Blocks 256 -BlockSize 4096
#
# Tema 14 — Verificador de Integridade de Disco
# Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro

param(
    [int]$Blocks    = 128,
    [int]$BlockSize = 4096,
    [int]$Passes    = 2
)

$img = "build\_bench.img"
$vrt = "build\_bench.verity"
$csv = "build\benchmark_results.csv"

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " BENCHMARK — verity_read com cache LRU         " -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Imagem: $Blocks blocos x $BlockSize bytes = $([math]::Round($Blocks*$BlockSize/1024))KB"
Write-Host "Passes por configuracao: $Passes`n"

# ── Gerar imagem ─────────────────────────────────────────────────────────────
Write-Host "Gerando imagem..." -NoNewline
python -c "import os; open('$img','wb').write(os.urandom($Blocks * $BlockSize))"
Write-Host " OK"

# ── Construir árvore ─────────────────────────────────────────────────────────
Write-Host "Construindo arvore..." -NoNewline
.\build\mkverity $img $vrt $BlockSize | Out-Null
Write-Host " OK`n"

# ── Cabeçalho CSV ─────────────────────────────────────────────────────────────
$header = "pass,cache_cap,blocks,hash_comp,hit_ratio,reads_ok,reads_failed,time_ms"
$header | Out-File -FilePath $csv -Encoding utf8

# ── Capacidades a testar ──────────────────────────────────────────────────────
$capacities = @(0, 8, 16, 32, 64, 128)

Write-Host ("{0,-6} {1,-10} {2,-5} {3,-12} {4,-10} {5,-10}" -f `
    "Cache", "Pass", "Blocos", "Hashes", "HitRatio", "Tempo(ms)")
Write-Host ("-" * 56)

foreach ($cap in $capacities) {
    $lines = .\build\bench $img $vrt $cap $Passes 2>&1
    foreach ($line in $lines) {
        if ($line -match "^[0-9]") {
            $line | Out-File -FilePath $csv -Append -Encoding utf8
            $parts = $line -split ","
            $pass      = $parts[0]
            $cache_c   = $parts[1]
            $hashes    = $parts[3]
            $hit_ratio = [double]$parts[4]
            $time_ms   = [double]$parts[7]
            $label     = if ($cap -eq 0) { "sem cache" } else { "cap=$cap" }
            Write-Host ("{0,-10} {1,-5} {2,-10} {3,-10} {4,-10} {5,-10}" -f `
                $label, "p$pass", $Blocks, $hashes, ("{0:P1}" -f $hit_ratio), ("{0:F1}" -f $time_ms))
        }
    }
    Write-Host ""
}

Remove-Item $img, $vrt -Force -ErrorAction SilentlyContinue
Write-Host "Resultados salvos em: $csv" -ForegroundColor Green
