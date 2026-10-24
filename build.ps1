# Hyperarea - one click build script
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release"
)

$ErrorActionPreference = 'Stop'

$root   = $PSScriptRoot
$tools  = Join-Path $root '.tools'
$build  = Join-Path $root 'build'
$dist   = Join-Path $root 'dist'

$env:PATH = (Join-Path $tools 'mingw64\bin') + ';' +
            (Join-Path $tools 'cmake\bin')   + ';' +
            (Join-Path $tools 'ninja')       + ';' +
            $env:PATH

$cmake = Join-Path $tools 'cmake\bin\cmake.exe'

Write-Host "==> Configuring ($Config)" -ForegroundColor Cyan
& $cmake -S $root -B $build -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Config" `
    -DCMAKE_C_COMPILER=gcc `
    -DCMAKE_CXX_COMPILER=g++

Write-Host "==> Building" -ForegroundColor Cyan
& $cmake --build $build "--config" $Config

Write-Host ""
Write-Host "==> Copying artefacts to dist\" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Get-ChildItem (Join-Path $build 'Hyperarea_artefacts') -Recurse -Directory -Filter *.vst3 -ErrorAction SilentlyContinue | ForEach-Object {
    $target = Join-Path $dist $_.Name
    if (Test-Path $target) { Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue }
    try { Copy-Item $_.FullName $target -Recurse -Force -ErrorAction Stop; Write-Host "    " $target }
    catch { Write-Host "    COULD NOT COPY $target - plugin is open in your DAW" -ForegroundColor Red }
    Write-Host "    " $target
}

Get-ChildItem (Join-Path $build 'Hyperarea_artefacts') -Recurse -File -Filter *.exe -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $dist $_.Name) -Force
    Write-Host "    " (Join-Path $dist $_.Name)
}

Write-Host ""
Write-Host "==> Done." -ForegroundColor Green
