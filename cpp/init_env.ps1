# Environment init for FileTimeFixer C++ build (Windows):
# - Exiv2: via vcpkg (clone + bootstrap if needed, then vcpkg install exiv2:x64-windows)
# - FFmpeg: download prebuilt zip to cpp/deps/ffmpeg and add bin to PATH
# Run from repo root or from cpp/: .\cpp\init_env.ps1  or  .\init_env.ps1
# Requires: PowerShell 5+, Git (for vcpkg), CMake (optional; winget install Kitware.CMake)

$ErrorActionPreference = "Stop"
$CppDir = $PSScriptRoot
$RepoRoot = (Split-Path $CppDir -Parent)
$DepsDir = Join-Path $CppDir "deps"
$VcpkgDir = Join-Path $CppDir "vcpkg"

function Ensure-DepsDir {
    if (-not (Test-Path $DepsDir)) {
        New-Item -ItemType Directory -Path $DepsDir -Force | Out-Null
        Write-Host "Created $DepsDir"
    }
}

# --- vcpkg + Exiv2 ---
function Get-VcpkgExiv2 {
    $vcpkgExe = $null
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT "vcpkg.exe"))) {
        $vcpkgExe = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
        Write-Host "Using existing vcpkg: $vcpkgExe"
    }
    if (-not $vcpkgExe -and (Test-Path (Join-Path $VcpkgDir "vcpkg.exe"))) {
        $vcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
        $env:VCPKG_ROOT = $VcpkgDir
        Write-Host "Using repo-local vcpkg: $vcpkgExe"
    }
    if (-not $vcpkgExe) {
        Write-Host "Cloning vcpkg into $VcpkgDir ..."
        if (Test-Path $VcpkgDir) { Remove-Item -Recurse -Force $VcpkgDir }
        git clone --depth 1 "https://github.com/microsoft/vcpkg.git" $VcpkgDir
        $bootstrapBat = Join-Path $VcpkgDir "bootstrap-vcpkg.bat"
        if (-not (Test-Path $bootstrapBat)) { throw "vcpkg bootstrap script not found: $bootstrapBat" }
        Write-Host "Bootstrapping vcpkg ..."
        & cmd /c "`"$bootstrapBat`" -disableMetrics"
        if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
        $vcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
        $env:VCPKG_ROOT = $VcpkgDir
    }
    $triplet = "x64-windows"
    $prevErr = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $list = & $vcpkgExe list 2>$null
    $ErrorActionPreference = $prevErr
    if ($list -match "exiv2:$triplet") {
        Write-Host "Exiv2 already installed (exiv2:$triplet)."
    } else {
        Write-Host "Installing exiv2:$triplet (this may take a few minutes) ..."
        & $vcpkgExe install "exiv2:$triplet"
        if ($LASTEXITCODE -ne 0) { throw "vcpkg install exiv2 failed" }
    }
    $env:CMAKE_TOOLCHAIN_FILE = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
    Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
    Write-Host "CMAKE_TOOLCHAIN_FILE=$env:CMAKE_TOOLCHAIN_FILE"
}

# --- FFmpeg prebuilt ---
$FfmpegZipUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
$FfmpegZip = Join-Path $DepsDir "ffmpeg.zip"
$FfmpegExtract = Join-Path $DepsDir "ffmpeg"
$FfmpegBin = Join-Path $FfmpegExtract "bin"

function Get-FFmpeg {
    if (Get-Command ffmpeg -ErrorAction SilentlyContinue) {
        $p = (Get-Command ffmpeg).Source
        Write-Host "FFmpeg already on PATH: $p"
        return
    }
    if (Test-Path (Join-Path $FfmpegBin "ffmpeg.exe")) {
        $env:Path = "$FfmpegBin;$env:Path"
        Write-Host "FFmpeg found in deps: $FfmpegBin (added to PATH for this session)"
        return
    }
    Ensure-DepsDir
    Write-Host "Downloading FFmpeg (shared GPL build) to $FfmpegZip ..."
    try {
        Invoke-WebRequest -Uri $FfmpegZipUrl -OutFile $FfmpegZip -UseBasicParsing
    } catch {
        Write-Warning "Download failed. Try manually: $FfmpegZipUrl -> $FfmpegZip"
        Write-Host "Or install FFmpeg: winget install Gyan.FFmpeg"
        return
    }
    Write-Host "Extracting to $DepsDir ..."
    Expand-Archive -Path $FfmpegZip -DestinationPath $DepsDir -Force
    $sub = Get-ChildItem $DepsDir -Directory | Where-Object { Test-Path (Join-Path $_.FullName "bin\ffmpeg.exe") } | Select-Object -First 1
    if ($sub) {
        $script:FfmpegBin = Join-Path $sub.FullName "bin"
    } else {
        Write-Warning "FFmpeg bin not found under $DepsDir after extract."
        return
    }
    $env:Path = "$FfmpegBin;$env:Path"
    Write-Host "FFmpeg installed at $($sub.FullName) (bin added to PATH for this session)"
}

# --- Main ---
Write-Host "=== FileTimeFixer env init (Windows) ===" -ForegroundColor Cyan
Ensure-DepsDir
Get-VcpkgExiv2
Get-FFmpeg
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Green
Write-Host "  cd cpp\build"
Write-Host "  cmake -DCMAKE_TOOLCHAIN_FILE=`"$env:CMAKE_TOOLCHAIN_FILE`" .."
Write-Host "  cmake --build . --config Release"
Write-Host ""
Write-Host "To persist PATH for FFmpeg in this terminal, run:"
Write-Host "  `$env:Path = `"$FfmpegBin;`$env:Path`""
Write-Host "Or add $FfmpegBin to your system/user PATH."
Write-Host "=== Done ===" -ForegroundColor Cyan
