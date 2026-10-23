[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$TestTemp = Join-Path $ProjectRoot "build\calibration_test_temp"
New-Item -ItemType Directory -Path $TestTemp -Force | Out-Null
$env:TEMP = $TestTemp
$env:TMP = $TestTemp

Push-Location $ProjectRoot
try {
    python -m unittest discover -s tests -p "test_*.py" -v
    if ($LASTEXITCODE -ne 0) {
        throw "校准自动测试失败，退出码 $LASTEXITCODE"
    }
    Write-Host "HOST_UI_TEST_PASS"
    python (Join-Path $PSScriptRoot "protocol_static_check.py")
    if ($LASTEXITCODE -ne 0) {
        throw "协议静态检查失败，退出码 $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
