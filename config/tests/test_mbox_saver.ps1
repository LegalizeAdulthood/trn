# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string]$Helper, [string]$WorkDirectory, [string]$Case, [int]$Mmdf)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[IO.Directory]::CreateDirectory($WorkDirectory) | Out-Null
$article = [IO.Path]::Combine($WorkDirectory, 'article')
$destination = [IO.Path]::Combine($WorkDirectory, 'saved')
$encoding = [Text.UTF8Encoding]::new($false)
$envelope = 'From supplied@example.test Thu Jan 1 00:00:00 1970'
[byte[]]$contents = $encoding.GetBytes("From start`r`n") + (0..255) + @(10) +
    $encoding.GetBytes("from end`nFROM keep`nFrom ")
[byte[]]$escaped = $encoding.GetBytes(">From start`r`n") + (0..255) + @(10) +
    $encoding.GetBytes(">from end`nFROM keep`n>From ")
[byte[]]$previous = @(255, 0, 13, 10)
[IO.File]::WriteAllBytes($article, $contents)
[IO.File]::WriteAllBytes($destination, $previous)
$parameters = @($article, '.', 'comp/test', '42', '1', 'comp.test', $destination, $envelope)

function Invoke-Saver([string[]]$Parameters) {
    & $Helper @Parameters
    return $LASTEXITCODE
}

function Assert-Bytes([string]$Path, [byte[]]$Expected) {
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Path)) -cne
            [Convert]::ToBase64String($Expected)) {
        throw "Unexpected bytes in $Path"
    }
}

switch ($Case) {
    { $_ -in 'bytes', 'boundary', 'unicode_envelope' } {
        if ($Case -eq 'boundary') {
            $padding = 'x' * 65533
            $contents = $encoding.GetBytes($padding + "`nFrom cross`nfrom lower`nF`nFr`nFro`nFrom`nFrom ")
            $escaped = $encoding.GetBytes($padding + "`n>From cross`n>from lower`nF`nFr`nFro`nFrom`n>From ")
        }
        if ($Case -eq 'unicode_envelope') {
            $parameters[7] = "From Andr$([char]233)@example.test Thu Jan 1 00:00:00 1970"
        }
        [IO.File]::WriteAllBytes($article, $contents)
        if ($Mmdf -ne 0) {
            [byte[]]$separator = @(1, 1, 1, 1, 10)
            $expected = $previous + $separator + $contents + @(10, 10) + $separator
        } else {
            $expected = $previous + $encoding.GetBytes($parameters[7] + "`n") + $escaped + @(10, 10)
        }
        if ((Invoke-Saver $parameters) -ne 0) { throw 'Mailbox copy failed' }
        Assert-Bytes $destination $expected
    }
    'invalid_offset' {
        foreach ($offset in @('-1', 'bad', '9223372036854775808')) {
            $parameters[4] = $offset
            if ((Invoke-Saver $parameters) -eq 0) { throw "Accepted offset $offset" }
        }
        Assert-Bytes $destination $previous
    }
    'arity' {
        if ((Invoke-Saver $parameters[0..6]) -eq 0) { throw 'Accepted missing envelope' }
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
