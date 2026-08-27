<#
.SYNOPSIS
    Packages an installed MaNGOS Three Windows build into a self-contained zip.

.DESCRIPTION
    Copies the `cmake --install` tree, then makes it run on a clean machine:
      * every DLL any exe/DLL imports is resolved with `dumpbin /dependents` and copied
        beside the importing binary (OpenSSL, MySQL client, VC++ runtime) -- an unresolved
        import fails the build instead of shipping a zip that dies with "DLL not found";
      * the OpenSSL legacy provider is placed at ossl-modules\legacy.dll, where
        src/shared/Auth/OpenSSLProvider.cpp looks for it (mangosd refuses to start without it);
      * PDBs stay beside the exes so crash logs written to Crashes\ are symbolized;
      * a smoke test starts mangosd from the package with OPENSSL_MODULES cleared and requires
        the "OpenSSL 3.x providers loaded successfully" line (the DB connection that follows is
        expected to fail; `--version` alone exits before the providers are initialised).
    Runs inside the MSVC developer environment (dumpbin, VCToolsRedistDir).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $InstallDir,
    [Parameter(Mandatory)] [string] $BuildDir,
    [Parameter(Mandatory)] [string] $Version,
    [Parameter(Mandatory)] [string] $OutDir,
    [string] $OpenSslBin = 'C:\Program Files\OpenSSL-Win64\bin',
    [int] $SmokeTimeoutSec = 90
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Step([string] $Message) { Write-Host "==> $Message" }

$name = "MangosThree-$Version-win64"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$staging = Join-Path $OutDir $name
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

# ---------------------------------------------------------------- install tree
Write-Step "Copy install tree from $InstallDir"
Copy-Item -Path (Join-Path $InstallDir '*') -Destination $staging -Recurse -Force
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Copy-Item -Path (Join-Path $repoRoot 'LICENSE') -Destination $staging
# Import libraries and export files are build-time artefacts the install tree drags along.
Get-ChildItem -Path $staging -Recurse -Include *.lib, *.exp | Remove-Item -Force

# ---------------------------------------------------------------- DLL search dirs
$cacheLines = Get-Content (Join-Path $BuildDir 'CMakeCache.txt')
function Get-CacheValue([string] $Key) {
    $line = $cacheLines | Where-Object { $_ -like "$Key`:*" } | Select-Object -First 1
    if ($line) { return ($line -replace '^[^=]*=', '') }
    return $null
}
$mysqlLib = Get-CacheValue 'MySQL_LIBRARY'
$mysqlDir = if ($mysqlLib) { Split-Path -Parent $mysqlLib } else { $null }
$vcRedist = if ($env:VCToolsRedistDir) { Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT' } else { $null }
# Highest priority first. MySQL 8 ships its own OpenSSL 3 DLLs under the same names; the
# build's OpenSSL wins (3.x is ABI-compatible across minor versions).
$searchDirs = @($OpenSslBin, $mysqlDir, $vcRedist) | Where-Object { $_ -and (Test-Path $_) }
$system32 = Join-Path $env:SystemRoot 'System32'
Write-Step "DLL search directories: $($searchDirs -join ' ; ')"

function Get-Dependents([string] $File) {
    $lines = & dumpbin /nologo /dependents $File 2>&1
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed on $File`n$lines" }
    return $lines | ForEach-Object { "$_".Trim() } | Where-Object { $_ -match '^[\w\-\.]+\.dll$' }
}

# ---------------------------------------------------------------- resolve imports
Write-Step 'Resolve runtime dependencies'
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem -Path $staging -Recurse -Include *.exe, *.dll | ForEach-Object { $queue.Enqueue($_.FullName) }
$seen = @{}
$report = [System.Collections.Generic.List[string]]::new()
while ($queue.Count -gt 0) {
    $bin = $queue.Dequeue()
    $binDir = Split-Path -Parent $bin
    foreach ($dep in Get-Dependents $bin) {
        $key = "$binDir|$($dep.ToLowerInvariant())"
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        if ($dep -match '^(api-ms-win-|ext-ms-)') { continue }          # Windows API sets
        if (Test-Path (Join-Path $binDir $dep)) { continue }              # already beside the importer
        $found = $null
        foreach ($dir in $searchDirs) {
            $candidate = Join-Path $dir $dep
            if (Test-Path $candidate) { $found = $candidate; break }
        }
        if ($found) {
            Copy-Item -Path $found -Destination (Join-Path $binDir $dep)
            $queue.Enqueue((Join-Path $binDir $dep))
            $report.Add("$dep  <- $found  (for $(Split-Path -Leaf $bin))")
        }
        elseif (Test-Path (Join-Path $system32 $dep)) {
            $report.Add("$dep  (Windows)")
        }
        else {
            throw "Unresolved dependency '$dep' imported by $bin"
        }
    }
}
$report | Sort-Object -Unique | ForEach-Object { Write-Host "    $_" }

# ---------------------------------------------------------------- OpenSSL legacy provider
Write-Step 'OpenSSL legacy provider'
$legacy = Join-Path $OpenSslBin 'legacy.dll'
if (-not (Test-Path $legacy)) { throw "legacy.dll not found in $OpenSslBin" }
New-Item -ItemType Directory -Force -Path (Join-Path $staging 'ossl-modules') | Out-Null
Copy-Item -Path $legacy -Destination (Join-Path $staging 'ossl-modules\legacy.dll')

# ---------------------------------------------------------------- RELEASE.txt
$openssl = & (Join-Path $OpenSslBin 'openssl.exe') version
@(
    "MaNGOS Three (Cataclysm 4.3.4) Windows x64 build"
    "Version:   $Version"
    "Commit:    $($env:GITHUB_SHA)"
    "Built:     $((Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm')) UTC"
    "Compiler:  MSVC $(Get-CacheValue 'CMAKE_CXX_COMPILER_VERSION'), $(Get-CacheValue 'CMAKE_BUILD_TYPE')"
    "OpenSSL:   $openssl"
    "MySQL:     $(Get-CacheValue 'MySQL_INCLUDE_DIR')"
    ""
    "Copy mangosd.conf.dist to mangosd.conf and realmd.conf.dist to realmd.conf, set the"
    "*DatabaseInfo lines, extract the client data with tools\mangos-extractor.exe, run realmd.exe"
    "and mangosd.exe. No Visual C++ redistributable is needed. Crash logs are written to Crashes\."
) | Set-Content -Path (Join-Path $staging 'RELEASE.txt') -Encoding utf8

# ---------------------------------------------------------------- smoke test
Write-Step 'Smoke test'
Remove-Item Env:OPENSSL_MODULES -ErrorAction SilentlyContinue
foreach ($exe in 'realmd.exe', 'mangosd.exe') {
    $out = & (Join-Path $staging $exe) --version 2>&1
    if ($LASTEXITCODE -ne 0 -or -not $out) { throw "$exe --version failed (exit $LASTEXITCODE): $out" }
    Write-Host "    $exe --version -> $out"
}
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = Join-Path $staging 'mangosd.exe'
$psi.Arguments = '-c mangosd.conf.dist'
$psi.WorkingDirectory = $staging
$psi.UseShellExecute = $false
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$proc = [System.Diagnostics.Process]::Start($psi)
$proc.StandardInput.Close()
$stdout = $proc.StandardOutput.ReadToEndAsync()
$stderr = $proc.StandardError.ReadToEndAsync()
if (-not $proc.WaitForExit($SmokeTimeoutSec * 1000)) { $proc.Kill(); $proc.WaitForExit() }
$output = $stdout.Result + $stderr.Result
if ($output -notmatch 'OpenSSL 3\.x providers loaded successfully') {
    Write-Host $output
    throw 'mangosd did not report "OpenSSL 3.x providers loaded successfully" -- the package is incomplete'
}
Write-Host "    mangosd: OpenSSL providers loaded from the package (exit code $($proc.ExitCode) ignored)"
Get-ChildItem -Path $staging -Recurse -Include Crashes, *.log | Remove-Item -Recurse -Force

# ---------------------------------------------------------------- zip
Write-Step 'Zip'
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
if (Get-Command 7z -ErrorAction SilentlyContinue) {
    Push-Location $OutDir
    try {
        & 7z a -tzip -mx=6 -bd $zip $name | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "7z exited with $LASTEXITCODE" }
    }
    finally { Pop-Location }
}
else {
    Compress-Archive -Path $staging -DestinationPath $zip -CompressionLevel Optimal
}
$sha = (Get-FileHash -Path $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host "    $zip ($sizeMb MB) sha256=$sha"
if ($env:GITHUB_OUTPUT) {
    "sha256=$sha" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
    "zip=$zip"    | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
}
