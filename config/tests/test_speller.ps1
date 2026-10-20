# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

param([string] $Helper, [string] $WorkDirectory, [string] $Case)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[void] [IO.Directory]::CreateDirectory($WorkDirectory)
$directory = Join-Path $WorkDirectory ('helper & [literal] ! ' + [char] 233)
[void] [IO.Directory]::CreateDirectory($directory)
Copy-Item -LiteralPath $Helper -Destination $directory
Copy-Item -LiteralPath (Join-Path (Split-Path $Helper) 'Speller.ps1') -Destination $directory
$Helper = Join-Path $directory 'Speller.cmd'
$checker = Join-Path $directory 'checker.ps1'
$encoding = [Text.Encoding]::GetEncoding(28591)
[IO.File]::WriteAllText($checker, @'
$encoding = [Text.Encoding]::GetEncoding(28591)
$path = $args[$args.Count - 1]
$text = $encoding.GetString([IO.File]::ReadAllBytes($path))
[IO.File]::WriteAllBytes($path, $encoding.GetBytes($text.Replace('mispelled', 'corrected')))
'@, [Text.ASCIIEncoding]::new())
$draft = Join-Path $directory 'draft & [literal] !.txt'
# Keep arbitrary bytes inside protected signature text, including NUL and CR/LF.
$original = "From: protected`r`nSubject: mispelled`r`n`r`nmispelled body`r`n> quoted`r`n-- `r`n" +
    $encoding.GetString([byte[]] (0..255))
[IO.File]::WriteAllBytes($draft, $encoding.GetBytes($original))
$expected = $original.Replace('mispelled', 'corrected')
$env:ISPELL_PROGRAM = $checker
$env:ISPELL_OPTIONS = ''
$env:QUOTECHARS = '>'
$env:TMP = Join-Path $WorkDirectory 'temporary'
[void] [IO.Directory]::CreateDirectory($env:TMP)
$arguments = @($draft)
$expectFailure = $false
$lock = $null
try {
    switch ($Case) {
        'arity' { $arguments = @(); $expectFailure = $true }
        'missing_input' { $arguments = @($draft + '.missing'); $expectFailure = $true }
        'locked' {
            $lock = [IO.File]::Open($draft, 'Open', 'Read', 'Read')
            $expectFailure = $true
        }
        'invalid_pattern' { $env:QUOTECHARS = '['; $expectFailure = $true }
        'invalid_options' { $env:ISPELL_OPTIONS = '"unfinished'; $expectFailure = $true }
        'bytes' { }
        default { throw "Unknown case: $Case" }
    }
    & $Helper @arguments
    if (($LASTEXITCODE -ne 0) -ne $expectFailure) { throw "Unexpected status: $LASTEXITCODE" }
    if ($expectFailure) { $expected = $original }
    $actual = [IO.File]::ReadAllBytes($draft)
    if ([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($encoding.GetBytes($expected))) {
        throw 'Draft bytes changed unexpectedly.'
    }
    if (@(Get-ChildItem -LiteralPath $env:TMP -Force).Count -ne 0) { throw 'Temporary files remain.' }
    if (@(Get-ChildItem -LiteralPath $directory -Filter '*.speller-*' -Force).Count -ne 0) {
        throw 'Replacement files remain.'
    }
}
finally { if ($lock) { $lock.Dispose() } }
