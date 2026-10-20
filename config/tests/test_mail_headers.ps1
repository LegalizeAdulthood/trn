# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string] $Adapter, [string] $WorkDirectory)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
. $Adapter
[void] [IO.Directory]::CreateDirectory($WorkDirectory)
$path = Join-Path $WorkDirectory 'draft.eml'
$utf8 = [Text.UTF8Encoding]::new($false)
$display = 'Andr' + [char] 0x00E9
$headers = "To: $display <first@example.test>,`n `"Smith, Jo`" <second@example.test>`n" +
    "Cc: copy@example.test`nBcc: hidden@example.test,`n hidden-two@example.test`n" +
    "Subject: Reply $display`nIn-Reply-To: <original@example.test>`n" +
    "MIME-Version: 1.0`nContent-Type: message/rfc822`n"
$body = "Subject: forward`nBcc: original-private@example.test`n`nOriginal body`n"
[IO.File]::WriteAllText($path, $headers + "`n" + $body, $utf8)
$submission = New-TrnMailSubmission $path author@example.test
if ($submission.Recipients.Count -ne 5) { throw 'Folded recipients were lost.' }
$message = Split-TrnMailMessage ($utf8.GetString($submission.Bytes))
if ($message[0] -match '(?im)^Bcc:' -or $message[1] -notmatch 'Bcc: original-private@example.test') {
    throw 'Bcc sanitizing changed the forwarded content or leaked an envelope header.'
}
$addresses = @(Get-TrnMailAddresses (Get-TrnMailHeader $message[0] To))
if ($addresses.Count -ne 2 -or $addresses[1].DisplayName -ne 'Smith, Jo') { throw 'Display-name quoting was lost.' }
if ($message[0] -notmatch 'Subject: =\?utf-8\?B\?' -or $message[0] -match '[^\x00-\x7f]') {
    throw 'Unicode header encoding failed.'
}
foreach ($name in @('Date', 'Message-ID', 'From', 'In-Reply-To', 'Content-Type')) {
    if (!(Get-TrnMailHeader $message[0] $name)) { throw "Missing header: $name" }
}
if ([IO.File]::ReadAllText($path) -cne ($headers + "`n" + $body)) { throw 'Submission altered the draft.' }
$long = ConvertTo-TrnEncodedHeader ($display * 50)
foreach ($word in $long -split '\s+') {
    if ($word.Length -gt 75) { throw 'Encoded word exceeds RFC 2047 limit.' }
}
$rejected = $false
try { $null = Get-TrnMailAddresses ($display + '@example.test') }
catch { $rejected = $true }
if (!$rejected) { throw 'Unsupported SMTPUTF8 mailbox was not rejected.' }
