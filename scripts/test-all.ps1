param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot/test-trevor.ps1" -Compiler $Compiler
Push-Location $projectRoot
$previousCompiler = $env:CC
$previousBuild = $env:TEST_BUILD
try {
    & $Compiler -std=c11 -Wall -Wextra -Werror -Iinclude test/test_entrance.c components/entrance/core.c -o .qa/test-entrance.exe
    if ($LASTEXITCODE -ne 0) { throw 'Entrance test compilation failed' }
    & ./.qa/test-entrance.exe
    if ($LASTEXITCODE -ne 0) { throw 'Entrance tests failed' }
    & $Compiler -std=c11 -Wall -Wextra -Werror -Itest/host -Iinclude test/test_actuator.c test/host/shim.c components/actuator/runtime.c -o .qa/test-actuator.exe
    if ($LASTEXITCODE -ne 0) { throw 'Door test compilation failed' }
    & ./.qa/test-actuator.exe
    if ($LASTEXITCODE -ne 0) { throw 'Door tests failed' }
    $env:CC = $Compiler
    $env:TEST_BUILD = Join-Path $projectRoot '.qa'
    & $Python ./test/test_regressions.py
    if ($LASTEXITCODE -ne 0) { throw 'Management/storage regressions failed' }
    & $Python ./test/test_rfid_poll.py
    if ($LASTEXITCODE -ne 0) { throw 'RFID polling regressions failed' }
} finally {
    $env:CC = $previousCompiler
    $env:TEST_BUILD = $previousBuild
    Pop-Location
}
