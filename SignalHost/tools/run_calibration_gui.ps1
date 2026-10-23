[CmdletBinding()]
param(
    [switch]$AllowRealSerial,
    [switch]$AllowInstruments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$arguments = @("-m", "calibration_host.app")
if ($AllowRealSerial) {
    $arguments += "--allow-real-serial"
}
if ($AllowInstruments) {
    $arguments += "--allow-instruments"
}

Push-Location $ProjectRoot
try {
    & python @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "校准工作台退出码: $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
