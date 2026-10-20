# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string] $Tools, [string] $Tool, [string] $WorkDirectory)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
. $Tools
[Console]::InputEncoding = [Text.UTF8Encoding]::new($true)
[void] [IO.Directory]::CreateDirectory($WorkDirectory)
$env:PNEWS_TEST_DIRECTORY = $WorkDirectory
$env:PNEWS_TEST_MODE = 'streams'
$inputPath = Join-Path $WorkDirectory 'input'
$outputPath = Join-Path $WorkDirectory 'output'
[byte[]] $bytes = 0..255
[IO.File]::WriteAllBytes($inputPath, $bytes)
Invoke-TrnTool ('"' + $Tool + '" poster') @('-h') $inputPath
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $WorkDirectory 'posted'))) -cne
    [Convert]::ToBase64String($bytes)) {
    throw 'Input-only tool did not receive the original bytes.'
}
Invoke-TrnTool ('"' + $Tool + '" pgp') @() $inputPath $outputPath
if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($outputPath)) -cne
    [Convert]::ToBase64String($bytes)) {
    throw 'Streamed tool did not preserve the original bytes.'
}
if ([Console]::InputEncoding.GetPreamble().Length -ne 3) {
    throw 'Tool launch did not restore the console input encoding.'
}
