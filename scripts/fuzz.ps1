param(
    [ValidateRange(1, 10000000)]
    [int]$Runs = 25000,
    [ValidateRange(1, 3600)]
    [int]$Seconds = 30,
    [ValidateRange(1, 2147483647)]
    [int]$Seed = 1337,
    [string]$Clang
)
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
$fuzzDirectory = Join-Path $projectRoot 'build\fuzz'
$corpusDirectory = Join-Path $fuzzDirectory 'corpus'
$artifactDirectory = Join-Path $fuzzDirectory 'artifacts'
New-Item -ItemType Directory -Path $corpusDirectory, $artifactDirectory -Force | Out-Null
$fuzzExecutable = Join-Path $fuzzDirectory 'mutation_fuzz.exe'
$sourceFiles = @('src/compiler.cpp', 'src/lexer.cpp', 'src/parser.cpp', 'src/semantic.cpp',
    'src/constant_fold.cpp', 'src/codegen.cpp', 'fuzz/compiler_fuzz.cpp', 'fuzz/mutation_runner.cpp') |
    ForEach-Object { Join-Path $projectRoot $_ }
$clangArguments = @('-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
    '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
    '-D_DISABLE_VECTOR_ANNOTATION', '-D_DISABLE_STRING_ANNOTATION',
    '-Wl,/INCREMENTAL:NO', '-I', (Join-Path $projectRoot 'include')) +
    $sourceFiles + @('-o', $fuzzExecutable)
& $Clang @clangArguments
if ($LASTEXITCODE -ne 0) { throw 'Compiler fuzz target build failed.' }
foreach ($example in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'examples') -Filter '*.skill') {
    Copy-Item -LiteralPath $example.FullName -Destination (Join-Path $corpusDirectory $example.Name) -Force
}
$utf8WithoutBom = [Text.UTF8Encoding]::new($false)
$prefix = 'mission main() -> int { yap('
[IO.File]::WriteAllText((Join-Path $corpusDirectory 'deep-expression.skill'),
    ($prefix + ('(' * 140) + '1' + (')' * 140) + '); gg 0; }'), $utf8WithoutBom)
[IO.File]::WriteAllText((Join-Path $corpusDirectory 'source-limit.skill'),
    (' ' * 65537), $utf8WithoutBom)
[IO.File]::WriteAllText((Join-Path $corpusDirectory 'eof.skill'),
    'mission main() -> int { yap(1);', $utf8WithoutBom)
$runtimeDirectory = & $Clang -print-runtime-dir
if ($LASTEXITCODE -ne 0) { throw 'Could not locate the Clang sanitizer runtime.' }
$previousProcessPath = $env:Path
$fuzzerArguments = @($corpusDirectory, $artifactDirectory,
    (Join-Path $projectRoot 'fuzz\skillissue.dict'), $Runs, $Seconds, $Seed)
try {
    $env:Path = $runtimeDirectory + [IO.Path]::PathSeparator + $previousProcessPath
    & $fuzzExecutable @fuzzerArguments 2>&1 | Tee-Object -FilePath (Join-Path $fuzzDirectory 'last-run.log')
    if ($LASTEXITCODE -ne 0) { throw 'Fuzzing failed. Inspect build/fuzz/last-run.log and build/fuzz/artifacts.' }
} finally {
    $env:Path = $previousProcessPath
}
