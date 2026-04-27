[CmdletBinding()]
param(
  [string]$Root = ''
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

function Get-EnvMap {
  param([string]$Path)

  $map = @{}
  if (-not (Test-Path -LiteralPath $Path)) {
    return $map
  }

  foreach ($line in Get-Content -LiteralPath $Path) {
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith('#')) {
      continue
    }
    if ($trimmed -match '^([^=]+)=(.*)$') {
      $map[$Matches[1].Trim()] = $Matches[2].Trim()
    }
  }
  return $map
}

function Get-ConfiguredApiUrl {
  param([string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    return $null
  }

  $match = Select-String -LiteralPath $Path -Pattern '#define\s+ECODEX_HYBRID_API_URL\s+"([^"]+)"' | Select-Object -First 1
  if ($match -and $match.Matches.Count -gt 0) {
    return $match.Matches[0].Groups[1].Value
  }
  return $null
}

function Get-DatasetLabels {
  param([string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    return @()
  }

  return @(Get-ChildItem -Directory -LiteralPath $Path | Sort-Object Name | ForEach-Object { $_.Name })
}

$rootPath = Resolve-Path -LiteralPath $Root
$envPath = Join-Path $rootPath 'plantnet-api\.env'
$nodeModules = Join-Path $rootPath 'plantnet-api\node_modules'
$datasetPath = Join-Path $rootPath 'Ecodex\dataset'
$modelPath = Join-Path $rootPath 'plant_cam_v4\ecodex_local\model\ecodex_baseline.npz'
$metricsPath = Join-Path (Split-Path -Parent $modelPath) 'ecodex_baseline.metrics.json'
$configPath = Join-Path $rootPath 'plant_cam_plantnet\config.h'
$currentIp = Get-WifiIpv4
$configuredApiUrl = Get-ConfiguredApiUrl $configPath
$envMap = Get-EnvMap $envPath
$datasetLabels = Get-DatasetLabels $datasetPath

Write-Host ''
Write-Host 'EcoDex System Check' -ForegroundColor Cyan
Write-Host ('Project: {0}' -f $rootPath)
Write-Host ''

if ($currentIp) {
  Write-Status OK ("Laptop IPv4: {0}" -f $currentIp)
} else {
  Write-Status WARN 'Could not detect laptop IPv4 from ipconfig.'
}

if ($configuredApiUrl) {
  Write-Status OK ("ESP32 API URL: {0}" -f $configuredApiUrl)
  if ($currentIp -and $configuredApiUrl -notmatch [regex]::Escape($currentIp)) {
    Write-Status WARN ("config.h uses a different IP than this laptop currently reports. Expected IP contains {0}." -f $currentIp)
  }
} else {
  Write-Status FAIL 'Missing plant_cam_plantnet\config.h or ECODEX_HYBRID_API_URL.'
}

if (Test-Path -LiteralPath $envPath) {
  Write-Status OK 'plantnet-api\.env exists.'
} else {
  Write-Status FAIL 'Missing plantnet-api\.env.'
}

if ($envMap.ContainsKey('PLANTNET_API_KEY') -and $envMap['PLANTNET_API_KEY'] -and $envMap['PLANTNET_API_KEY'] -ne 'YOUR_API_KEY_HERE') {
  Write-Status OK 'Pl@ntNet API key is loaded.'
} else {
  Write-Status FAIL 'Pl@ntNet API key is missing or still set to the placeholder.'
}

if ($envMap.ContainsKey('LOCAL_MODEL_URL')) {
  Write-Status OK ("LOCAL_MODEL_URL: {0}" -f $envMap['LOCAL_MODEL_URL'])
} else {
  Write-Status WARN 'LOCAL_MODEL_URL is not set; hybrid server will default to http://127.0.0.1:8090.'
}

if (Test-Path -LiteralPath $nodeModules) {
  Write-Status OK 'Node dependencies are installed.'
} else {
  Write-Status FAIL 'Node dependencies are missing. Run: cd plantnet-api; npm.cmd install'
}

if ($datasetLabels.Count -gt 0) {
  Write-Status OK ('Dataset has {0} class folder(s): {1}' -f $datasetLabels.Count, ($datasetLabels -join ', '))
} else {
  Write-Status FAIL ("No dataset class folders found in {0}." -f $datasetPath)
}

if (Test-Path -LiteralPath $modelPath) {
  Write-Status OK 'EcoDex model file exists.'
} else {
  Write-Status FAIL 'EcoDex model file is missing.'
}

if (Test-Path -LiteralPath $metricsPath) {
  try {
    $metrics = Get-Content -Raw -LiteralPath $metricsPath | ConvertFrom-Json
    $metricDataset = [string]$metrics.dataset
    $normalizedMetricDataset = $metricDataset.Replace('/', '\')
    if ($metricDataset -and
        $metricDataset -ne $datasetPath -and
        $normalizedMetricDataset -ne 'Ecodex\dataset' -and
        -not $normalizedMetricDataset.EndsWith('\Ecodex\dataset')) {
      Write-Status WARN ("Model metrics were trained from a different dataset path: {0}" -f $metrics.dataset)
    }
  } catch {
    Write-Status WARN 'Could not read EcoDex metrics JSON.'
  }
}

$mlHealth = Invoke-Json 'http://127.0.0.1:8090/health'
if ($mlHealth) {
  $modelLabels = @($mlHealth.labels | Where-Object { $_ })
  Write-Status OK ('EcoDex ML server is online. Labels: {0}' -f (($modelLabels | Where-Object { $_ -ne 'unknown' }) -join ', '))
  if ($datasetLabels.Count -gt 0) {
    $missingInModel = @($datasetLabels | Where-Object { $modelLabels -notcontains $_ })
    $notInDataset = @($modelLabels | Where-Object { $datasetLabels -notcontains $_ })
    if ($missingInModel.Count -gt 0) {
      Write-Status WARN ('Dataset labels missing from running model: {0}. Retrain, then restart EcoDex.' -f ($missingInModel -join ', '))
    }
    if ($notInDataset.Count -gt 0) {
      Write-Status WARN ('Running model has labels not found in dataset folders: {0}' -f ($notInDataset -join ', '))
    }
  }
} else {
  Write-Status WARN 'EcoDex ML server is offline. Start it with .\start_ecodex.ps1.'
}

$hybridHealth = Invoke-Json 'http://127.0.0.1:3000/health'
if ($hybridHealth) {
  Write-Status OK ('Hybrid API is online: {0}' -f $hybridHealth.service)
} else {
  Write-Status WARN 'Hybrid API is offline. Start it with .\start_ecodex.ps1.'
}

$trained = Invoke-Json 'http://127.0.0.1:3000/api/trained-plants'
if ($trained) {
  Write-Status OK ('Hybrid API sees {0} trained plant label(s): {1}' -f $trained.count, (($trained.plants | ForEach-Object { $_.name }) -join ', '))
}

Write-Host ''
Write-Host 'Daily command: .\start_ecodex.ps1  (leave that window open)' -ForegroundColor Cyan
Write-Host 'Stop command:  .\stop_ecodex.ps1' -ForegroundColor Cyan
