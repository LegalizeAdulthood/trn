# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string]$Helper, [string]$WorkDirectory, [switch]$HasEnvelope)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[void][IO.Directory]::CreateDirectory($WorkDirectory)
$article = [IO.Path]::Combine($WorkDirectory, 'article')
$destination = [IO.Path]::Combine($WorkDirectory, 'saved')
$contents = [Text.Encoding]::ASCII.GetBytes("Header: test`n`nBody`n")
$previous = [Text.Encoding]::ASCII.GetBytes("previous`n")
[IO.File]::WriteAllBytes($article, $contents)
[IO.File]::WriteAllBytes($destination, $previous)
$parameters = @($article, '.', 'comp/test', '42', '1', 'comp.test', $destination)
if ($HasEnvelope) {
    $parameters += 'From supplied@example.test Thu Jan 1 00:00:00 1970'
}

$lock = [IO.File]::Open($destination, 'Open', 'ReadWrite', 'None')
try {
    & $Helper @parameters
    if ($LASTEXITCODE -eq 0) { throw 'Accepted locked output' }
} finally {
    $lock.Dispose()
}

function Assert-Contents([string]$Path, [byte[]]$Expected) {
    $actual = [IO.File]::ReadAllBytes($Path)
    if ([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($Expected)) {
        throw "Changed file: $Path"
    }
}
Assert-Contents $destination $previous
Assert-Contents $article $contents
