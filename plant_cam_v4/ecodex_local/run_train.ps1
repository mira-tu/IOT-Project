$python = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) {
  $pythonCommand = Get-Command python -ErrorAction Stop
  $python = $pythonCommand.Source
}

$trainer = Join-Path $PSScriptRoot 'train_baseline.py'
& $python $trainer @args
