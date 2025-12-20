param(
    [string]$BuildType = "Release",
    [string]$VcpkgToolchain = "",
    [string]$VcpkgTriplet = ""
)

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build
Write-Host "Configuring CMake..." -ForegroundColor Blue

$vcpkgToolchainPath = $VcpkgToolchain
if ($vcpkgToolchainPath -eq "") {
    if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
        $vcpkgToolchainPath = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
        Write-Host "Found vcpkg via VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green
    }
    elseif (Get-Command vcpkg -ErrorAction SilentlyContinue) {
        $vcpkgExe = (Get-Command vcpkg).Source
        $vcpkgRoot = Split-Path -Parent $vcpkgExe
        $vcpkgToolchainPath = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

        if (Test-Path $vcpkgToolchainPath) {
            Write-Host "Found vcpkg via PATH: $vcpkgRoot" -ForegroundColor Green
        } else {
            $vcpkgToolchainPath = ""
        }
    }
}

if ($vcpkgToolchainPath -ne "") {
    Write-Host "Using vcpkg toolchain: $vcpkgToolchainPath" -ForegroundColor Yellow
    $cmakeArgs = @("..", "-DCMAKE_TOOLCHAIN_FILE=`"$vcpkgToolchainPath`"")

    if ($VcpkgTriplet -ne "") {
        Write-Host "Using vcpkg triplet: $VcpkgTriplet" -ForegroundColor Yellow
        $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=$VcpkgTriplet"
    }

    & cmake $cmakeArgs
} else {
    Write-Host "vcpkg not found, configuring without it..." -ForegroundColor Yellow
    Write-Host "To use vcpkg, either:" -ForegroundColor Yellow
    Write-Host "  - Set VCPKG_ROOT environment variable" -ForegroundColor White
    Write-Host "  - Add vcpkg to your PATH" -ForegroundColor White
    Write-Host "  - Use -VcpkgToolchain parameter" -ForegroundColor White
    Write-Host ""
    cmake ..
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Blue
cmake --build . --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Run the application with:" -ForegroundColor Cyan
Write-Host "  .\build\bin\$BuildType\Discove.exe" -ForegroundColor Blue
Write-Host ""

Set-Location ..
