param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',
    [switch]$Test,
    [switch]$Demo,
    [string]$Clang
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDirectory = Join-Path $projectRoot 'build'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath)) {
        throw 'CMake is required. Install CMake or the Visual Studio C++ CMake tools.'
    }
    $vsInstall = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstall) { throw 'Visual Studio C++ build tools were not found.' }
    $cmakePath = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'CMake was not found in Visual Studio.' }
}
$configureArguments = @('-S', $projectRoot, '-B', $buildDirectory)
if (-not (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt'))) {
    $configureArguments += @('-G', 'Visual Studio 17 2022', '-A', 'x64')
}
if ($Clang) { $configureArguments += "-DSKILLISSUE_CLANG=$Clang" }
& $cmakePath @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmakePath --build $buildDirectory --config $Configuration --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
if ($Test) {
    $ctestPath = Join-Path (Split-Path $cmakePath -Parent) 'ctest.exe'
    & $ctestPath --test-dir $buildDirectory -C $Configuration --output-on-failure --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'SkillIssue tests failed.' }
}
if ($Demo) {
    & $cmakePath --build $buildDirectory --config $Configuration --target demo
    if ($LASTEXITCODE -ne 0) { throw 'The native demo failed.' }
}
