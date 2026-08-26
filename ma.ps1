[CmdletBinding()]
param(
    [Alias("c")]
    [switch]$Clear,

    [Alias("b")]
    [switch]$Benchmark,

    [Alias("r")]
    [switch]$Release,

    [Alias("t")]
    [switch]$Test,

    [Alias("f")]
    [string]$Filter = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

# MSYS2 ucrt64 のパス定義
$GxxPath = "C:/msys64/ucrt64/bin/g++.exe"
$GccPath = "C:/msys64/ucrt64/bin/gcc.exe"

# 1. -Clear: ビルドフォルダの削除と再生成
if ($Clear) {
    if (Test-Path $BuildDir) {
        Write-Host "[INFO] Clearing build directory: $BuildDir" -ForegroundColor Yellow
        Remove-Item -Path $BuildDir -Recurse -Force
    }
}

if (-not (Test-Path $BuildDir)) {
    New-Item -Path $BuildDir -ItemType Directory | Out-Null
}

Push-Location $BuildDir
try {
    # 2. CMake 構成 (Configure)
    $BuildType = if ($Release -or $Benchmark) { "Release" } else { "Debug" }
    Write-Host "[INFO] Configuring CMake with GCC (BuildType: $BuildType)..." -ForegroundColor Cyan

    # Generator に MinGW Makefiles を指定（Ninja がインストールされている場合は "Ninja" でも可）
    $CmakeArgs = @(
        "-S", $ProjectRoot,
        "-B", $BuildDir,
        "-G", "MinGW Makefiles",
        "-DCMAKE_CXX_COMPILER=$GxxPath",
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )
    cmake @CmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

    # 3. ビルド (Build)
    Write-Host "[INFO] Building project..." -ForegroundColor Green
    cmake --build .
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }

    # 4. -Test (またはベンチマーク指定なし時) のユニットテスト実行
    if ($Test -or (-not $Benchmark)) {
        Write-Host "[INFO] Running unit tests via CTest..." -ForegroundColor Cyan
        $CTestArgs = @("--output-on-failure")
        if ($Filter -ne "") {
            $CTestArgs += @("-R", $Filter)
        }
        ctest @CTestArgs
        if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
    }

    # 5. -Benchmark: ベンチマークバイナリの実行
    if ($Benchmark) {
        $BenchExe = Join-Path $BuildDir "benchmark_ConditionalRange.exe"

        if (Test-Path $BenchExe) {
            Write-Host "[INFO] Running benchmark: $BenchExe" -ForegroundColor Magenta
            $BenchArgs = @()
            if ($Filter -ne "") {
                $BenchArgs += "--benchmark_filter=$Filter"
            }
            & $BenchExe @BenchArgs
            if ($LASTEXITCODE -ne 0) { throw "Benchmark run failed." }
        } else {
            Write-Host "[WARN] Benchmark executable not found: $BenchExe" -ForegroundColor Red
        }
    }
}
finally {
    Pop-Location
}