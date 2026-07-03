# Launch the OpenGL viewer.
# Usage:
#   scripts\view.ps1                                  # reference motion only
#   scripts\view.ps1 -Nn ..\nn\max.pt                 # torque-actuated policy
#   scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt   # muscle model
param(
  [string]$Meta = "",
  [string]$Nn = "",
  [string]$MuscleNn = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Meta -eq "") { $Meta = "$root\data\metadata.txt" }

# render.exe embeds Python; it needs torch/numpy/Model.py. Point the embedded
# interpreter at the venv site-packages (Model.py dir is added by Window.cpp).
$env:PYTHONPATH = "$root\Deps\venv\Lib\site-packages"
# torch bundles its own OpenMP (libiomp5md); the render/DART process already has an
# OpenMP runtime, so torch aborts on import without this. Allow the duplicate.
$env:KMP_DUPLICATE_LIB_OK = "TRUE"

$exe = "$root\build\render\Release\render.exe"
$args = @($Meta)
if ($Nn -ne "") { $args += $Nn }
if ($MuscleNn -ne "") { $args += $MuscleNn }

& $exe @args
