# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string]$Helper, [string]$WorkDirectory, [string]$Case)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[IO.Directory]::CreateDirectory($WorkDirectory) | Out-Null
$article = [IO.Path]::Combine($WorkDirectory, 'article')
$destination = [IO.Path]::Combine($WorkDirectory, 'saved')
[byte[]]$contents = 0..255
[byte[]]$previous = @(255, 0, 13, 10)
[IO.File]::WriteAllBytes($article, $contents)
[IO.File]::WriteAllBytes($destination, $previous)
$parameters = @($article, '.', 'comp/test', '42', '1', 'comp.test', $destination)

function Invoke-Saver([string[]]$Parameters) {
    & $Helper @Parameters
    return $LASTEXITCODE
}

function Assert-Bytes([string]$Path, [byte[]]$Expected) {
    $actual = [IO.File]::ReadAllBytes($Path)
    if ([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($Expected)) {
        throw "Unexpected bytes in $Path"
    }
}

switch ($Case) {
    'bytes' {
        if ((Invoke-Saver $parameters) -ne 0) { throw 'Byte copy failed' }
        Assert-Bytes $destination ($previous + $contents + @(10, 10))
        $parameters[4] = '128'
        if ((Invoke-Saver $parameters) -ne 0) { throw 'Byte offset failed' }
        Assert-Bytes $destination ($previous + $contents + @(10, 10) + $contents[127..255] + @(10, 10))
    }
    'invalid_offset' {
        foreach ($offset in @('-1', 'bad', '9223372036854775808')) {
            $parameters[4] = $offset
            if ((Invoke-Saver $parameters) -eq 0) { throw "Accepted offset $offset" }
        }
        Assert-Bytes $destination $previous
    }
    'arity' {
        if ((Invoke-Saver $parameters[0..5]) -eq 0) { throw 'Accepted missing argument' }
        if ((Invoke-Saver ($parameters + @('extra'))) -eq 0) { throw 'Accepted extra argument' }
        Assert-Bytes $destination $previous
    }
    'missing_input' {
        $parameters[0] = $article + '.missing'
        if ((Invoke-Saver $parameters) -eq 0) { throw 'Accepted missing input' }
        Assert-Bytes $destination $previous
    }
    'missing_parent' {
        $parameters[6] = [IO.Path]::Combine($WorkDirectory, 'missing', 'saved')
        if ((Invoke-Saver $parameters) -eq 0) { throw 'Accepted missing output directory' }
        Assert-Bytes $destination $previous
    }
    'locked_output' {
        $lock = [IO.File]::Open($destination, 'Open', 'ReadWrite', 'None')
        try {
            if ((Invoke-Saver $parameters) -eq 0) { throw 'Accepted locked output' }
        } finally {
            $lock.Dispose()
        }
        Assert-Bytes $destination $previous
    }
    'same_file' {
        $parameters[6] = $article
        if ((Invoke-Saver $parameters) -eq 0) { throw 'Accepted saving onto the input' }
        Assert-Bytes $destination $previous
    }
    default { throw "Unknown test case $Case" }
}
Assert-Bytes $article $contents
