# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

param([string] $Helper, [string] $WorkDirectory)
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
$original = "From: protected`r`nSubject: mispelled`r`n`r`nmispelled body`r`n> quoted`r`n-- `r`nSignature`r`n"
[IO.File]::WriteAllBytes($draft, $encoding.GetBytes($original))
$env:ISPELL_PROGRAM = $checker
$env:ISPELL_OPTIONS = ''
$env:QUOTECHARS = '>'
$env:TMP = Join-Path $WorkDirectory 'temporary'
[void] [IO.Directory]::CreateDirectory($env:TMP)
$lock = [IO.File]::Open($draft, 'Open', 'Read', 'Read')
try {
    & $Helper $draft
    if ($LASTEXITCODE -eq 0) { throw 'Accepted replacing a locked draft.' }
    $actual = [IO.File]::ReadAllBytes($draft)
    if ([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($encoding.GetBytes($original))) {
        throw 'Draft bytes changed unexpectedly.'
    }
    if (@(Get-ChildItem -LiteralPath $env:TMP -Force).Count -ne 0) { throw 'Temporary files remain.' }
    if (@(Get-ChildItem -LiteralPath $directory -Filter '*.speller-*' -Force).Count -ne 0) {
        throw 'Replacement files remain.'
    }
}
finally { $lock.Dispose() }
