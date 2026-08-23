[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$NoExample,
    [switch]$VerboseBuild,
    [switch]$Benchmark
)

$ErrorActionPreference = "Stop"

# ============================================================
# Paths
# ============================================================

$ProjectRoot = $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "build"

$MsysUcrt64Bin = "C:\msys64\ucrt64\bin"
$GxxPath       = Join-Path $MsysUcrt64Bin "g++.exe"

# ============================================================
# Helper functions
# ============================================================

function Write-Step {
    param(
        [string]$Message
    )

    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host $Message -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Fail {
    param(
        [string]$Message
    )

    Write-Host ""
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments

    if ($LASTEXITCODE -ne 0) {
        Fail "Command failed with exit code $LASTEXITCODE : $FilePath $($Arguments -join ' ')"
    }
}

# ============================================================
# Move to project root
# ============================================================

Set-Location $ProjectRoot

Write-Host ""
Write-Host "ConditionalRange build script" -ForegroundColor Green
Write-Host "Project root: $ProjectRoot"

# ============================================================
# Check required tools
# ============================================================

Write-Step "Checking tools"

if (-not (Test-Path $GxxPath)) {
    Fail "g++ was not found: $GxxPath"
}

$CMakeCommand = Get-Command cmake -ErrorAction SilentlyContinue

if ($null -eq $CMakeCommand) {
    Fail "cmake was not found in PATH."
}

Write-Host "g++   : $GxxPath"
Write-Host "cmake : $($CMakeCommand.Source)"

# ============================================================
# Add MSYS2 UCRT64 bin to PATH
# ============================================================

if (-not (Test-Path $MsysUcrt64Bin)) {
    Fail "MSYS2 UCRT64 bin directory was not found: $MsysUcrt64Bin"
}

$env:PATH = "$MsysUcrt64Bin;$env:PATH"

Write-Host "PATH  : MSYS2 UCRT64 bin added"

# ============================================================
# Clean
# ============================================================

if ($Clean) {
    Write-Step "Cleaning build directory"

    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Host "Removed: $BuildDir"
    }
    else {
        Write-Host "Build directory does not exist. Nothing to clean."
    }
}

# ============================================================
# Configure
# ============================================================

Write-Step "Configuring CMake"

$CMakeConfigureArgs = @(
    "-S", $ProjectRoot,
    "-B", $BuildDir,
    "-G", "MinGW Makefiles",
    "-DCMAKE_CXX_COMPILER=$GxxPath",
    "-DBUILD_TESTING=ON",
    "-DCMAKE_BUILD_TYPE=Release"
)

Invoke-Checked "cmake" $CMakeConfigureArgs

# ============================================================
# Build
# ============================================================

Write-Step "Building"

$BuildArgs = @(
    "--build", $BuildDir,
    "--parallel"
)

if ($VerboseBuild) {
    $BuildArgs += "--verbose"
}

Invoke-Checked "cmake" $BuildArgs

# ============================================================
# Run tests
# ============================================================

Write-Step "Running tests"

Invoke-Checked "ctest" @(
    "--test-dir", $BuildDir,
    "--output-on-failure"
)

# ============================================================
# Run benchmark
# ============================================================

if ($Benchmark) {
    Write-Step "Running benchmark"

    $BenchmarkExe = Join-Path $BuildDir "conditional_range_benchmark.exe"

    if (-not (Test-Path $BenchmarkExe)) {
        Fail "Benchmark executable was not found: $BenchmarkExe"
    }

    & $BenchmarkExe

    if ($LASTEXITCODE -ne 0) {
        Fail "Benchmark exited with code $LASTEXITCODE"
    }
}

# ============================================================
# Run example
# ============================================================

if (-not $NoExample) {
    Write-Step "Running example"

    $ExampleExe = Join-Path $BuildDir "conditional_range_example.exe"

    if (-not (Test-Path $ExampleExe)) {
        Fail "Example executable was not found: $ExampleExe"
    }

    & $ExampleExe

    if ($LASTEXITCODE -ne 0) {
        Fail "Example exited with code $LASTEXITCODE"
    }
}

# ============================================================
# Finished
# ============================================================

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""

Write-Host "Build directory:"
Write-Host "  $BuildDir"

Write-Host ""
Write-Host "All tests passed." -ForegroundColor Green