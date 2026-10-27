param([Parameter(Mandatory=$true)][string]$Compiler)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
Push-Location $taskRoot
try {
    New-Item -ItemType Directory -Force '.qa' | Out-Null
    & $Compiler -std=c11 -Wall -Wextra -Werror -Iinclude test/test_trevor.c src/trevor/core.c -o .qa/test-trevor.exe
    if ($LASTEXITCODE -ne 0) { throw 'C test compilation failed' }
    & ./.qa/test-trevor.exe
    if ($LASTEXITCODE -ne 0) { throw 'C behavior test failed' }
    & $Compiler -std=c11 -Wall -Wextra -Werror -DTR_TEST_STORAGE -Itest/host -Iinclude test/test_runtime.c test/host/shim.c src/trevor/core.c src/trevor/runtime.c -o .qa/test-runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Runtime test compilation failed' }
    & ./.qa/test-runtime.exe
    if ($LASTEXITCODE -ne 0) { throw 'Runtime integration test failed' }
} finally { Pop-Location }
