<#
.SYNOPSIS
Formats project C++ files, or checks them without making changes.
.EXAMPLE
./tools/format.ps1 -Check
.EXAMPLE
./tools/format.ps1 -ClangFormat 'C:/path/to/clang-format.exe'
#>
[CmdletBinding()]
param(
    [switch]$Check,
    [string]$ClangFormat
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot

if (-not $ClangFormat) {
    $command = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($command) {
        $ClangFormat = $command.Source
    } else {
        # Visual Studio's optional C++ Clang tools are enough; no runtime dependency.
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $installations = & $vswhere -products '*' -property installationPath
            foreach ($installation in $installations) {
                $candidate = Join-Path $installation 'VC/Tools/Llvm/bin/clang-format.exe'
                if (Test-Path -LiteralPath $candidate) {
                    $candidateVersion = & $candidate --version
                    if ($candidateVersion -match 'version 19\.') {
                        $ClangFormat = $candidate
                        break
                    }
                }
            }
        }
    }
}

if (-not $ClangFormat) {
    throw 'clang-format 19 is required. Add it to PATH or pass -ClangFormat with its executable path.'
}
$version = & $ClangFormat --version
if ($LASTEXITCODE -ne 0 -or $version -notmatch 'version 19\.') {
    throw "Use clang-format 19 for reproducible formatting. Found: $version"
}

$directories = 'src', 'include', 'tests', 'tools' | ForEach-Object { Join-Path $repositoryRoot $_ }
$files = @(Get-ChildItem -LiteralPath $directories -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.h' } |
    Sort-Object FullName |
    ForEach-Object { $_.FullName })

# Windows PowerShell treats redirected native stderr as ErrorRecords. Let the
# formatter report all violations, then fail on its exit status below.
$ErrorActionPreference = 'Continue'
if ($Check) {
    & $ClangFormat --style=file --dry-run --Werror @files
} else {
    & $ClangFormat --style=file -i @files
}
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { throw "clang-format failed with exit code $LASTEXITCODE." }
Write-Output "C++ format $(if ($Check) { 'check' } else { 'update' }) passed ($($files.Count) files)."
