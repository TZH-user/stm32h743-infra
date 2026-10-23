$ErrorActionPreference = "Stop"

$templateRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $templateRoot "Template\APP\FpgaLink"
$buildDir = Join-Path $PSScriptRoot ".build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compiler = $env:CC
if (-not $compiler)
{
    $compiler = Get-Command gcc.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty Source
}
if (-not $compiler)
{
    $clionGcc = "E:\CLion 2026.1\bin\mingw\bin\gcc.exe"
    if (Test-Path -LiteralPath $clionGcc)
    {
        $compiler = $clionGcc
    }
}
if (-not $compiler)
{
    throw "Native Windows GCC was not found. Set CC to gcc.exe."
}

$compilerDir = Split-Path -Parent $compiler
$env:PATH = "$compilerDir;$env:PATH"

function Invoke-C-Test
{
    param(
        [string]$Name,
        [string[]]$Sources
    )

    $exe = Join-Path $buildDir "$Name.exe"
    & $compiler -std=c99 -Wall -Wextra -Werror -pedantic "-I$sourceDir" @Sources -o $exe
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
    & $exe
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

Invoke-C-Test "dual_spi_protocol_tests" @(
    (Join-Path $PSScriptRoot "dual_spi_protocol_tests.c"),
    (Join-Path $sourceDir "dual_spi_protocol.c")
)
Invoke-C-Test "dual_spi_session_tests" @(
    (Join-Path $PSScriptRoot "dual_spi_session_tests.c"),
    (Join-Path $sourceDir "dual_spi_session.c"),
    (Join-Path $sourceDir "dual_spi_protocol.c")
)
Invoke-C-Test "dual_spi_transport_tests" @(
    (Join-Path $PSScriptRoot "dual_spi_transport_tests.c"),
    (Join-Path $sourceDir "dual_spi_transport.c")
)

Write-Output "All dual-SPI host tests passed."
