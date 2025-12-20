param(
    [string]$VcpkgTriplet = "x64-windows-static"
)

Write-Host "Building in Debug mode..." -ForegroundColor Blue
& .\build.ps1 -BuildType Debug -VcpkgTriplet $VcpkgTriplet

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

$exePath = ".\build\bin\Debug\Discove.exe"
if (Test-Path $exePath) {
    & $exePath
    $exitCode = $LASTEXITCODE

    Write-Host ""
    if ($exitCode -eq 0) {
        Write-Host "Application exited successfully." -ForegroundColor Green
    } else {
        Write-Host "Application exited with code: $exitCode" -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Executable not found at: $exePath" -ForegroundColor Red
    exit 1
}
