param(
    [Parameter(Mandatory = $true)]
    [Alias("Host")]
    [string]$DeviceHost,

    [string]$Token,

    [string]$Bin = "build\info_dashboard.bin",

    [int]$Port = 8080
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$binPath = Resolve-Path -LiteralPath $Bin
$envPath = Join-Path -Path (Split-Path -Parent $PSScriptRoot) -ChildPath ".env"
if (-not $Token) {
    if (-not (Test-Path -LiteralPath $envPath)) {
        throw "Token was not provided and .env was not found at $envPath"
    }
    $tokenLine = Get-Content -LiteralPath $envPath -Encoding UTF8 |
        Where-Object { $_ -match '^\s*DASHBOARD_OTA_TOKEN\s*=' } |
        Select-Object -First 1
    if (-not $tokenLine) {
        throw "DASHBOARD_OTA_TOKEN was not found in $envPath"
    }
    $Token = ($tokenLine -replace '^\s*DASHBOARD_OTA_TOKEN\s*=\s*', '').Trim().Trim('"').Trim("'")
    if (-not $Token) {
        throw "DASHBOARD_OTA_TOKEN in $envPath is empty"
    }
}
$hash = (Get-FileHash -LiteralPath $binPath -Algorithm SHA256).Hash.ToLowerInvariant()
$uri = "http://${DeviceHost}:${Port}/ota"

$headers = @{
    "X-Dashboard-Token" = $Token
    "X-Firmware-SHA256" = $hash
}

Write-Host "Uploading $($binPath.Path)"
Write-Host "Target: $uri"
Write-Host "SHA256: $hash"

$curl = Get-Command curl.exe -ErrorAction SilentlyContinue
if (-not $curl) {
    throw "curl.exe was not found"
}
$response = & $curl.Source --noproxy "*" --fail-with-body -sS -X POST `
    -H "X-Dashboard-Token: $Token" `
    -H "X-Firmware-SHA256: $hash" `
    -H "Content-Type: application/octet-stream" `
    --data-binary "@$($binPath.Path)" `
    $uri
if ($LASTEXITCODE -ne 0) {
    Write-Error "OTA upload failed: curl exited with code $LASTEXITCODE"
} else {
    $response
}
