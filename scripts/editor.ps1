# Launch the MASS unified 3D editor.
# Usage:
#   scripts\editor.ps1                    # opens data\human.mass if present
#   scripts\editor.ps1 -Mass path.mass    # opens a specific project
param([string]$Mass = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($Mass -eq "" -and (Test-Path "$root\data\human.mass")) { $Mass = "$root\data\human.mass" }

# torch (used by the live-sim reuse of the training stack) bundles its own OpenMP;
# DART also loads one. Allow the duplicate to avoid an abort.
$env:KMP_DUPLICATE_LIB_OK = "TRUE"

$exe = "$root\build\editor\Release\mass_editor.exe"
if (-not (Test-Path $exe)) { $exe = "$root\Dist\x64\Release\mass_editor.exe" }

if ($Mass -ne "") { & $exe $Mass } else { & $exe }
