# Configure + build MASS (Windows / MSVC / vcpkg).
# Usage: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Config Release]
param([string]$Config = "Release")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$pyBase = "C:\Users\Hermes\AppData\Local\Programs\Python\Python310"

cmake -S $root -B "$root\build" -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$root\Deps\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DPython_EXECUTABLE="$pyBase\python.exe" `
  -DPython_INCLUDE_DIR="$pyBase\include" `
  -DPython_LIBRARY="$pyBase\libs\python310.lib"

cmake --build "$root\build" --config $Config
Write-Host "`nBuild done. pymss + render + mss compiled." -ForegroundColor Green
