[CmdletBinding()]
param(
  [string]$Root = ''
)

$ErrorActionPreference = 'Stop'

if (-not $Root) {
  $Root = Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Stop-PidFile {
  param([string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    return
  }

  $pidText = (Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue | Select-Object -First 1)
  if ($pidText -match '^\d+$') {
    $process = Get-Process -Id ([int]$pidText) -ErrorAction SilentlyContinue
    if ($process) {
      Write-Host ("Stopping {0} PID {1}" -f $process.ProcessName, $process.Id)
      Stop-Process -Id $process.Id -Force
    }
  }

  Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$stateDir = Join-Path $rootPath '.ecodex'

Stop-PidFile (Join-Path $stateDir 'ecodex-hybrid.pid')
Stop-PidFile (Join-Path $stateDir 'ecodex-ml.pid')

Write-Host 'EcoDex background processes stopped if they were started by start_ecodex.ps1.' -ForegroundColor Green
