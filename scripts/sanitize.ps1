param([string]$Clang)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Clang) {
    $portableClang = Join-Path $projectRoot '.tools\llvm\bin\clang++.exe'
    if (Test-Path -LiteralPath $portableClang) { $Clang = $portableClang }
    else {
        $clangCommand = Get-Command clang++ -ErrorAction SilentlyContinue
        if (-not $clangCommand) { throw 'Clang with sanitizer runtimes is required.' }
        $Clang = $clangCommand.Source
    }
}
$sanitizerDirectory = Join-Path $projectRoot 'build\sanitizers'
New-Item -ItemType Directory -Path $sanitizerDirectory -Force | Out-Null
$sanitizerExecutable = Join-Path $sanitizerDirectory 'skillissue_tests.exe'
$sourceFiles = @('src/compiler.cpp', 'src/lexer.cpp', 'src/parser.cpp', 'src/semantic.cpp',
    'src/constant_fold.cpp', 'src/codegen.cpp', 'tests/compiler_tests.cpp') |
    ForEach-Object { Join-Path $projectRoot $_ }
$clangArguments = @('-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
    '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
    '-D_DISABLE_VECTOR_ANNOTATION', '-D_DISABLE_STRING_ANNOTATION',
    '-I', (Join-Path $projectRoot 'include')) +
    $sourceFiles + @('-o', $sanitizerExecutable)
& $Clang @clangArguments
if ($LASTEXITCODE -ne 0) { throw 'Sanitizer test build failed.' }
$sanitizerRuntime = & $Clang -print-runtime-dir
if ($LASTEXITCODE -ne 0) { throw 'Could not locate the Clang sanitizer runtime.' }
$previousProcessPath = $env:Path
try {
    $env:Path = $sanitizerRuntime + [IO.Path]::PathSeparator + $previousProcessPath
    & $sanitizerExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Sanitizer tests failed.' }
} finally {
    $env:Path = $previousProcessPath
}
