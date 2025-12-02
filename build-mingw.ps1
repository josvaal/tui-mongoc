Write-Host "MongoDB TUI - MinGW Build" -ForegroundColor Cyan
Write-Host "==========================" -ForegroundColor Cyan
Write-Host ""

$env:VCPKG_ROOT = "C:\Users\josval\vcpkg"
Write-Host "VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green

$gccPath = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gccPath) {
    Write-Host "ERROR: GCC not found in PATH" -ForegroundColor Red
    exit 1
}
Write-Host "GCC found: $($gccPath.Source)" -ForegroundColor Green

$makePath = Get-Command mingw32-make -ErrorAction SilentlyContinue
if (-not $makePath) {
    Write-Host "WARNING: mingw32-make not found. Looking for alternatives..." -ForegroundColor Yellow

    $possibleMakes = @(
        "C:\msys64\mingw64\bin\mingw32-make.exe",
        "C:\msys64\usr\bin\make.exe",
        "C:\Program Files\Git\usr\bin\make.exe"
    )

    foreach ($makeTry in $possibleMakes) {
        if (Test-Path $makeTry) {
            $env:PATH = "$(Split-Path $makeTry);$env:PATH"
            Write-Host "Found make at: $makeTry" -ForegroundColor Green
            break
        }
    }
}

if (Test-Path "_build") {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "_build"
}

$toolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
Write-Host "Toolchain: $toolchain" -ForegroundColor Cyan
Write-Host ""

Write-Host "Configuring with CMake (MinGW Makefiles)..." -ForegroundColor Yellow
cmake -S . -B _build `
    -G "MinGW Makefiles" `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=gcc `
    -DCMAKE_MAKE_PROGRAM=mingw32-make

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Configuration failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Try using MSYS2 terminal instead:" -ForegroundColor Yellow
    Write-Host "  1. Open MSYS2 MinGW 64-bit terminal" -ForegroundColor White
    Write-Host "  2. cd /c/Users/josva/projects/bdd-mongoc" -ForegroundColor White
    Write-Host "  3. make" -ForegroundColor White
    exit 1
}

# Build
Write-Host ""
Write-Host "Building..." -ForegroundColor Yellow
cmake --build _build --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "SUCCESS! Build completed." -ForegroundColor Green
Write-Host ""
Write-Host "Executable: _build\mongodb-tui.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "Run with:" -ForegroundColor Yellow
Write-Host "  cd _build" -ForegroundColor White
Write-Host "  .\mongodb-tui.exe" -ForegroundColor White
Write-Host ""
