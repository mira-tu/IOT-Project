[CmdletBinding()]
param(
  [string]$Root = '',
  [switch]$Background
)

$ErrorActionPreference = 'Stop'

if (-not $Root) {
  $Root = Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Write-Status {
  param(
    [string]$State,
    [string]$Message
  )

  $color = switch ($State) {
    'OK' { 'Green' }
    'WARN' { 'Yellow' }
    'FAIL' { 'Red' }
    default { 'White' }
  }

  Write-Host ("[{0}] {1}" -f $State, $Message) -ForegroundColor $color
}

function Invoke-Json {
  param(
    [string]$Uri,
    [int]$TimeoutSec = 4
  )

  try {
    $response = Invoke-WebRequest -UseBasicParsing -Uri $Uri -TimeoutSec $TimeoutSec
    return $response.Content | ConvertFrom-Json
  } catch {
    return $null
  }
}

function Wait-ForJson {
  param(
    [string]$Uri,
    [int]$Seconds = 20
  )

  $deadline = (Get-Date).AddSeconds($Seconds)
  do {
    $result = Invoke-Json $Uri 2
    if ($result) {
      return $result
    }
    Start-Sleep -Milliseconds 700
  } while ((Get-Date) -lt $deadline)

  return $null
}

function Start-HiddenProcess {
  param(
    [string]$FilePath,
    [string]$Arguments,
    [string]$WorkingDirectory
  )

  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $FilePath
  $psi.Arguments = $Arguments
  $psi.WorkingDirectory = $WorkingDirectory
  $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  return [System.Diagnostics.Process]::Start($psi)
}

function Save-Pid {
  param(
    [string]$Path,
    [System.Diagnostics.Process]$Process
  )

  Set-Content -LiteralPath $Path -Encoding ASCII -Value $Process.Id
}

function Get-WifiIpv4 {
  $lines = ipconfig
  $ips = @()
  foreach ($line in $lines) {
    if ($line -match 'IPv4 Address.*:\s*([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)') {
      $ips += $Matches[1]
    }
  }
  return $ips | Where-Object { $_ -notlike '127.*' } | Select-Object -First 1
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$stateDir = Join-Path $rootPath '.ecodex'
$pidMl = Join-Path $stateDir 'ecodex-ml.pid'
$pidHybrid = Join-Path $stateDir 'ecodex-hybrid.pid'
$python = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$pyServer = Join-Path $rootPath 'plant_cam_v4\ecodex_local\server.py'
$pyCwd = Join-Path $rootPath 'plant_cam_v4\ecodex_local'
$nodeServer = Join-Path $rootPath 'plantnet-api\hybrid-server.js'
$nodeCwd = Join-Path $rootPath 'plantnet-api'
$envPath = Join-Path $nodeCwd '.env'
$nodeModules = Join-Path $nodeCwd 'node_modules'
$configPath = Join-Path $rootPath 'plant_cam_plantnet\config.h'

New-Item -ItemType Directory -Force -Path $stateDir | Out-Null

Write-Host ''
Write-Host 'Starting EcoDex hybrid system' -ForegroundColor Cyan
Write-Host ('Project: {0}' -f $rootPath)
Write-Host ''

if (-not (Test-Path -LiteralPath $python)) {
  $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
  if (-not $pythonCommand) {
    throw 'Python was not found. Expected bundled Python or python on PATH.'
  }
  $python = $pythonCommand.Source
}

if (-not (Test-Path -LiteralPath $envPath)) {
  throw 'Missing plantnet-api\.env. Add PLANTNET_API_KEY before starting the hybrid API.'
}

if (-not (Test-Path -LiteralPath $nodeModules)) {
  throw 'Node dependencies are missing. Run: cd plantnet-api; npm.cmd install'
}

if (-not (Test-Path -LiteralPath $configPath)) {
  throw 'Missing plant_cam_plantnet\config.h. Copy config.example.h and set your laptop IP.'
}

$currentIp = Get-WifiIpv4
if ($currentIp) {
  Write-Status OK ("Laptop IPv4: {0}" -f $currentIp)
}

$mlHealth = Invoke-Json 'http://127.0.0.1:8090/health'
if ($mlHealth) {
  Write-Status OK 'EcoDex ML server already online.'
} else {
  Write-Host 'Starting EcoDex ML server on port 8090...'
  $mlProcess = Start-HiddenProcess $python ('"' + $pyServer + '"') $pyCwd
  Save-Pid $pidMl $mlProcess
  $mlHealth = Wait-ForJson 'http://127.0.0.1:8090/health' 30
  if (-not $mlHealth) {
    throw 'EcoDex ML server did not become healthy on http://127.0.0.1:8090/health.'
  }
  Write-Status OK ("EcoDex ML server online. PID {0}" -f $mlProcess.Id)
}

$hybridHealth = Invoke-Json 'http://127.0.0.1:3000/health'
if ($hybridHealth) {
  Write-Status OK 'Hybrid API already online.'
} else {
  $nodeCommand = Get-Command node -ErrorAction Stop
  Write-Host 'Starting hybrid API server on port 3000...'
  $hybridProcess = Start-HiddenProcess $nodeCommand.Source ('"' + $nodeServer + '"') $nodeCwd
  Save-Pid $pidHybrid $hybridProcess
  $hybridHealth = Wait-ForJson 'http://127.0.0.1:3000/health' 30
  if (-not $hybridHealth) {
    throw 'Hybrid API did not become healthy on http://127.0.0.1:3000/health.'
  }
  Write-Status OK ("Hybrid API online. PID {0}" -f $hybridProcess.Id)
}

$trained = Invoke-Json 'http://127.0.0.1:3000/api/trained-plants'
if ($trained) {
  Write-Status OK ('Trained labels: {0}' -f (($trained.plants | ForEach-Object { $_.name }) -join ', '))
}

Write-Host ''
Write-Host 'EcoDex is ready.' -ForegroundColor Green
Write-Host 'Open the ESP32 IP in your browser, then press Capture & Identify.'
Write-Host 'If the ESP32 cannot reach the laptop, run .\check_ecodex.ps1 and compare the IP in plant_cam_plantnet\config.h.'
Write-Host ''

if (-not $Background) {
  Write-Host 'Leave this window open while scanning plants. Press Ctrl+C to stop EcoDex.' -ForegroundColor Cyan
  try {
    while ($true) {
      Start-Sleep -Seconds 5
    }
  } finally {
    & (Join-Path $rootPath 'stop_ecodex.ps1') -Root $rootPath
  }
}
