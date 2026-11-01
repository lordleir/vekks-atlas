param(
    [Parameter(Mandatory = $true)]
    [string]$DllPath
)

if (-not (Test-Path -LiteralPath $DllPath)) {
    throw "Expected addon DLL was not built: $DllPath"
}

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($null -eq $dumpbin) {
    throw "dumpbin.exe is required to verify the GetAddonDef export."
}

$exports = & $dumpbin.Source /exports $DllPath 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $exports -notmatch '\bGetAddonDef\b') {
    throw "The addon DLL does not export GetAddonDef."
}
