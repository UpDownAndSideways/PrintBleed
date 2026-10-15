$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path $cmake)) {
    throw "CMake from Visual Studio Build Tools was not found at $cmake"
}

& $cmake -S $root -B (Join-Path $root "build") -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build (Join-Path $root "build") --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Built: $(Join-Path $root 'build\Release\feature_lab.exe')"
Write-Host "Built: $(Join-Path $root 'build\Release\platformer.exe')"
Write-Host "Built: $(Join-Path $root 'build\Release\cardpress.exe')"
