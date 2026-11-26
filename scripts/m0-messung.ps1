# M0-Messreihe fuer step Brother
#
# Fuehrt alle M0-Messungen nacheinander aus und legt die Ergebnisse in einem Ordner ab.
# Die JSON-Dateien enthalten keine Datei- oder Bauteilnamen.
#
# Aufruf (PowerShell, im Ordner mit den entpackten Varianten):
#   powershell -ExecutionPolicy Bypass -File .\m0-messung.ps1 -Step "C:\Pfad\anlage.stp"
#
# Erwartet die entpackten Ordner step-brother-m0-windows-msvc, -clang-cl und -mingw im
# selben Ordner wie dieses Skript (fehlende Varianten werden uebersprungen).

param(
  [Parameter(Mandatory = $true)][string]$Step,
  [int]$Wiederholungen = 2
)

$ErrorActionPreference = 'Stop'
$basis = $PSScriptRoot
if (-not (Test-Path $Step)) { throw "STEP-Datei nicht gefunden: $Step" }

$ergebnisse = Join-Path $basis ("m0-ergebnisse-" + (Get-Date -Format 'yyyyMMdd-HHmm'))
New-Item -ItemType Directory -Force $ergebnisse | Out-Null

function Messe([string]$variante, [string]$name, [string[]]$optionen) {
  $exe = Join-Path $basis "step-brother-m0-windows-$variante\sb-bench.exe"
  if (-not (Test-Path $exe)) {
    Write-Host "-- uebersprungen ($variante fehlt): $name" -ForegroundColor Yellow
    return
  }
  $json = Join-Path $ergebnisse "$variante-$name.bench.json"
  Write-Host "== $variante / $name" -ForegroundColor Cyan
  $start = Get-Date
  & $exe $Step @optionen --json $json
  $dauer = [int]((Get-Date) - $start).TotalSeconds
  Write-Host "   fertig nach $dauer s (Exit-Code $LASTEXITCODE)"
}

# 1. Qualitaet ohne Healing (mit Vernetzung)
Messe 'msvc' 'healing-aus-netz' @('--healing', 'off', '--deflection', '0.01,0.003')

# 2. Compiler-Vergleich: nur Lesen, ohne Healing, mehrfach
for ($i = 1; $i -le $Wiederholungen; $i++) {
  foreach ($v in 'msvc', 'clang-cl', 'mingw') {
    Messe $v "lesen-$i" @('--healing', 'off', '--no-mesh')
  }
}

Write-Host ""
Write-Host "Alle Messungen fertig. Ergebnisse in:" -ForegroundColor Green
Write-Host "  $ergebnisse"
Write-Host "Bitte die .bench.json-Dateien daraus an Claude schicken."
