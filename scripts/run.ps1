param(
    [string]$Source = 'examples/sum.skill',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',
    [string]$Clang
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($Source)) { $Source = Join-Path $projectRoot $Source }
if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "Source file not found: $Source" }
if (-not $Clang) {
    $portableClang = Join-Path $projectRoot '.tools\llvm\bin\clang++.exe'
    if (Test-Path -LiteralPath $portableClang) { $Clang = $portableClang }
    else {
        $clangCommand = Get-Command clang++ -ErrorAction SilentlyContinue
        if (-not $clangCommand) { throw 'clang++ is required for native programs. Pass -Clang with its path.' }
        $Clang = $clangCommand.Source
    }
}
& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Clang $Clang
$buildDirectory = Join-Path $projectRoot 'build'
$compiler = Join-Path $buildDirectory "$Configuration\skillc.exe"
if (-not (Test-Path -LiteralPath $compiler)) { $compiler = Join-Path $buildDirectory 'skillc.exe' }
$programDirectory = Join-Path $buildDirectory 'programs'
New-Item -ItemType Directory -Path $programDirectory -Force | Out-Null
$programName = [IO.Path]::GetFileNameWithoutExtension($Source)
$irPath = Join-Path $programDirectory ($programName + '.ll')
$executablePath = Join-Path $programDirectory ($programName + '.exe')
& $compiler $Source -o $irPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Clang -Wno-override-module $irPath (Join-Path $projectRoot 'runtime\skill_runtime.cpp') -o $executablePath
if ($LASTEXITCODE -ne 0) { throw 'Native program compilation failed.' }
& $executablePath
exit $LASTEXITCODE
