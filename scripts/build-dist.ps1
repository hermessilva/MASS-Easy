# Build MASS and package the viewer into Dist\x64\<Config>.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1              # Release
#   powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1 -Config Debug
#
# Produces Dist\x64\<Config>\ containing render.exe, its runtime DLLs and pymss.pyd.
param([ValidateSet("Release","Debug")][string]$Config = "Release")

$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$pyBase = "C:\Users\Hermes\AppData\Local\Programs\Python\Python310"
$build  = "$root\build"

# --- Configure (only if the cache is missing) ---
if (-not (Test-Path "$build\CMakeCache.txt")) {
    Write-Host "[configure] generating build/" -ForegroundColor Cyan
    cmake -S $root -B $build -G "Visual Studio 18 2026" -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="$root\Deps\vcpkg\scripts\buildsystems\vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET=x64-windows `
        -DPython_EXECUTABLE="$pyBase\python.exe" `
        -DPython_INCLUDE_DIR="$pyBase\include" `
        -DPython_LIBRARY="$pyBase\libs\python310.lib"
}

# --- Build ---
Write-Host "[build] $Config" -ForegroundColor Cyan
cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { throw "build failed" }

# --- Package ---
$dist = "$root\Dist\x64\$Config"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist -Force | Out-Null

$renderOut = "$build\render\$Config"
$pymssOut  = "$root\python\$Config"
$editorOut = "$build\editor\$Config"

Copy-Item "$renderOut\render.exe" $dist -Force
Copy-Item "$renderOut\*.dll"      $dist -Force
if (Test-Path "$pymssOut\pymss.cp310-win_amd64.pyd") {
    Copy-Item "$pymssOut\pymss.cp310-win_amd64.pyd" $dist -Force
}
if (Test-Path "$editorOut\arena.exe") {
    Copy-Item "$editorOut\arena.exe" $dist -Force
    Copy-Item "$editorOut\*.dll" $dist -Force -ErrorAction SilentlyContinue
}

Write-Host "`n[done] packaged to $dist" -ForegroundColor Green
Get-ChildItem $dist | Select-Object Name, Length | Format-Table -AutoSize

Write-Host "Run the viewer with:" -ForegroundColor Yellow
Write-Host "  `$env:PYTHONPATH='$root\Deps\venv\Lib\site-packages'"
Write-Host "  $dist\render.exe $root\data\metadata.txt"
