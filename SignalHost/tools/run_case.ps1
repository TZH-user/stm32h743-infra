[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$CaseName = "dual_1k_10k",

    [string]$CoreDir = "F:\STM32\stm32project\Function\Algorithm\SignalCore",

    [ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$BuildName = "host_msvc"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$BuildDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot "build\$BuildName"))
$ResultsRoot = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot "build\results\$BuildName"))
$CoreDir = [System.IO.Path]::GetFullPath($CoreDir)

if (-not $BuildDir.StartsWith((Join-Path $ProjectRoot "build"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir 越界: $BuildDir"
}
if (-not (Test-Path -LiteralPath (Join-Path $CoreDir "CMakeLists.txt"))) {
    throw "CoreDir 不是有效 SignalCore: $CoreDir"
}

function Resolve-ExistingTool {
    param([string]$Name, [string[]]$Fallbacks)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    foreach ($candidate in $Fallbacks) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "未找到工具: $Name"
}

function Import-VsEnvironment {
    $vsDevCmdCandidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    )
    $vsDevCmd = $vsDevCmdCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($null -eq $vsDevCmd) {
        throw "未找到 Visual Studio 2022 开发环境脚本"
    }

    $cmdLine = "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    $environmentLines = & "$env:SystemRoot\System32\cmd.exe" /d /s /c $cmdLine
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio 开发环境初始化失败: $vsDevCmd"
    }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=][^=]*)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
    return $vsDevCmd
}

function Select-Compiler {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -ne $gcc) {
        return [pscustomobject]@{ Kind = "gcc"; Path = $gcc.Source; NeedsVs = $false }
    }

    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if ($null -ne $clang) {
        return [pscustomobject]@{ Kind = "clang"; Path = $clang.Source; NeedsVs = $true }
    }

    return [pscustomobject]@{ Kind = "msvc"; Path = "cl"; NeedsVs = $true }
}

function Assert-LastExitCode {
    param([string]$Stage)
    if ($LASTEXITCODE -ne 0) {
        throw "$Stage 失败，退出码 $LASTEXITCODE"
    }
}

function Write-CoreManifest {
    param([string]$OutputPath)
    $manifest = Get-ChildItem -LiteralPath $CoreDir -Recurse -File |
        Sort-Object FullName |
        ForEach-Object {
            $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
            [pscustomobject]@{
                relative_path = [System.IO.Path]::GetRelativePath($CoreDir, $_.FullName)
                sha256 = $hash.Hash
                length = $_.Length
            }
        }
    $manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $OutputPath -Encoding utf8
}

$CMake = Resolve-ExistingTool -Name "cmake" -Fallbacks @(
    "F:\STM32\stm32toll\CMake\bin\cmake.exe"
)
$Ninja = Resolve-ExistingTool -Name "ninja" -Fallbacks @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
)
$Python = Resolve-ExistingTool -Name "python" -Fallbacks @()
$Compiler = Select-Compiler
$VsEnvironment = $null
if ($Compiler.NeedsVs) {
    $VsEnvironment = Import-VsEnvironment
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
New-Item -ItemType Directory -Path $ResultsRoot -Force | Out-Null
Write-CoreManifest -OutputPath (Join-Path $BuildDir "core_source_manifest_UTF-8.json")

$configureArguments = @(
    "-S", $ProjectRoot,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DSIGNALHOST_CORE_DIR=$CoreDir"
)
if ($Compiler.Kind -in @("gcc", "clang")) {
    $configureArguments += "-DCMAKE_C_COMPILER=$($Compiler.Path)"
}
elseif ($Compiler.Kind -eq "msvc") {
    $configureArguments += "-DCMAKE_C_COMPILER=cl"
}

Write-Host "[1/4] Configure: compiler=$($Compiler.Kind) path=$($Compiler.Path) core=$CoreDir"
& $CMake @configureArguments
Assert-LastExitCode "CMake configure"

Write-Host "[2/4] Build: $BuildDir"
& $CMake --build $BuildDir --parallel
Assert-LastExitCode "CMake build"

$Executable = Join-Path $BuildDir "bin\signalhost.exe"
if (-not (Test-Path -LiteralPath $Executable)) {
    throw "构建后未找到可执行文件: $Executable"
}

$BuildEvidence = [ordered]@{
    generated_at = (Get-Date).ToString("o")
    project_root = $ProjectRoot
    build_dir = $BuildDir
    compiler_kind = $Compiler.Kind
    compiler_path = $Compiler.Path
    vs_environment = $VsEnvironment
    core_dir = $CoreDir
    executable = $Executable
}
$BuildEvidence | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $BuildDir "build_evidence_UTF-8.json") -Encoding utf8

function Invoke-OneCase {
    param([string]$Name, [bool]$StrictVerify)

    $CasePath = Join-Path $ProjectRoot "cases\$Name.json"
    if (-not (Test-Path -LiteralPath $CasePath)) {
        throw "未找到用例: $CasePath"
    }
    $CaseConfig = Get-Content -LiteralPath $CasePath -Raw -Encoding utf8 | ConvertFrom-Json
    $ResultDir = Join-Path $ResultsRoot $Name
    New-Item -ItemType Directory -Path $ResultDir -Force | Out-Null
    $InputCsv = Join-Path $ResultDir "input.csv"
    $TruthJson = Join-Path $ResultDir "truth.json"
    $ResultCsv = Join-Path $ResultDir "result.csv"
    $ResultJson = Join-Path $ResultDir "result.json"
    $PlotPng = Join-Path $ResultDir "result.png"
    $MetricsJson = Join-Path $ResultDir "verification.json"

    Write-Host "[3/4] Generate + analyze: $Name"
    & $Python (Join-Path $ProjectRoot "tools\gen_signal.py") --case $CasePath --output $InputCsv --truth $TruthJson | Out-Host
    Assert-LastExitCode "gen_signal $Name"
    & $Executable --input $InputCsv --output-csv $ResultCsv --output-json $ResultJson | Out-Host
    Assert-LastExitCode "signalhost $Name"
    & $Python (Join-Path $ProjectRoot "tools\plot_result.py") --input $InputCsv --result-csv $ResultCsv --result-json $ResultJson --truth $TruthJson --output $PlotPng | Out-Host
    Assert-LastExitCode "plot_result $Name"

    if ($StrictVerify) {
        Write-Host "[4/4] Verify: $Name"
        & $Python (Join-Path $ProjectRoot "tools\verify_results.py") --case $CasePath --truth $TruthJson --result-csv $ResultCsv --result-json $ResultJson --metrics $MetricsJson | Out-Host
        Assert-LastExitCode "verify_results $Name"
    }
    return $ResultJson
}

if ($CaseName -eq "snr_degradation") {
    $Result40 = Invoke-OneCase -Name "dual_snr_40" -StrictVerify $false
    $Result20 = Invoke-OneCase -Name "dual_snr_20" -StrictVerify $false
    $Result10 = Invoke-OneCase -Name "dual_snr_10" -StrictVerify $false
    $SummaryDir = Join-Path $ResultsRoot "snr_degradation"
    New-Item -ItemType Directory -Path $SummaryDir -Force | Out-Null
    & $Python (Join-Path $ProjectRoot "tools\summarize_snr.py") `
        --results $Result40 $Result20 $Result10 `
        --snr 40 20 10 `
        --output-csv (Join-Path $SummaryDir "snr_degradation.csv") `
        --output-json (Join-Path $SummaryDir "snr_degradation.json") `
        --output-plot (Join-Path $SummaryDir "snr_degradation.png")
    Assert-LastExitCode "summarize_snr"
    Write-Host "PASS: snr_degradation -> $SummaryDir"
}
else {
    $strict = $CaseName -in @("dual_1k_10k", "sweep_100_50k")
    $Result = Invoke-OneCase -Name $CaseName -StrictVerify $strict
    Write-Host "PASS: $CaseName -> $Result"
}
