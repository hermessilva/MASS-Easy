# Run PPO training.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\train.ps1 [-Meta path] [-Model name]
param(
  [string]$Meta = "",
  [string]$Model = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Meta -eq "") { $Meta = "$root\data\metadata.txt" }

$py = "$root\Deps\venv\Scripts\python.exe"
# pymss.pyd lives in python\Release; torch/numpy live in the venv (already on path).
$env:PYTHONPATH = "$root\python\Release"
# pymss (OpenMP) + torch (bundled libiomp5md) load two OpenMP runtimes; allow it.
$env:KMP_DUPLICATE_LIB_OK = "TRUE"

Set-Location "$root\python"
if ($Model -ne "") {
  & $py main.py -d $Meta -m $Model
} else {
  & $py main.py -d $Meta
}
