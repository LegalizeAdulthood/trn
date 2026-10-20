# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

param([string] $Helper, [string] $Tool, [string] $Inews,
    [string] $Artcheck, [string] $Cmake, [string] $Fixture, [string] $WorkDirectory)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[void] [IO.Directory]::CreateDirectory($WorkDirectory)
$bin = Join-Path $WorkDirectory 'bin & [literal] !'
[void] [IO.Directory]::CreateDirectory($bin)
$helperDirectory = Split-Path $Helper
foreach ($name in @('Pnews.cmd', 'Pnews.ps1', 'trn-tools.ps1', 'Speller.cmd', 'Speller.ps1', 'mbox.saver.cmd', 'mbox.saver.ps1')) {
    Copy-Item -LiteralPath (Join-Path $helperDirectory $name) -Destination $bin
}
Copy-Item -LiteralPath $Inews -Destination (Join-Path $bin 'inews.exe')
Get-ChildItem -LiteralPath (Split-Path $Inews) -Filter '*.dll' | Copy-Item -Destination $bin
$portFile = Join-Path $WorkDirectory 'port'
$capture = Join-Path $WorkDirectory 'captured'
if ([IO.File]::Exists($portFile)) { [IO.File]::Delete($portFile) }
$job = Start-Job -ArgumentList $portFile, $capture -ScriptBlock {
    param($PortFile, $Capture)
    $ErrorActionPreference = 'Stop'
    $encoding = [Text.Encoding]::GetEncoding(28591)
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $listener.Start()
    [IO.File]::WriteAllText($PortFile, $listener.LocalEndpoint.Port.ToString())
    try {
        $pending = $listener.BeginAcceptTcpClient($null, $null)
        if (!$pending.AsyncWaitHandle.WaitOne(20000)) { throw 'inews did not connect.' }
        $client = $listener.EndAcceptTcpClient($pending)
        [IO.File]::WriteAllText($Capture + '.commands', "connected`n")
        $client.ReceiveTimeout = 20000
        $stream = $client.GetStream()
        $reader = [IO.StreamReader]::new($stream, $encoding)
        $writer = [IO.StreamWriter]::new($stream, $encoding)
        $writer.NewLine = "`r`n"
        $writer.AutoFlush = $true
        $writer.WriteLine('200 local posting fixture')
        while ($null -ne ($command = $reader.ReadLine())) {
            [IO.File]::AppendAllText($Capture + '.commands', $command + "`n")
            switch -Regex ($command) {
                '^CAPABILITIES' {
                    foreach ($line in @('101 capabilities', 'VERSION 2', 'READER', 'POST', '.')) { $writer.WriteLine($line) }
                    break
                }
                '^MODE READER' { $writer.WriteLine('200 reader'); break }
                '^POST' {
                    $writer.WriteLine('340 send article')
                    $article = [Text.StringBuilder]::new()
                    while ($null -ne ($line = $reader.ReadLine()) -and $line -ne '.') {
                        if ($line.StartsWith('..')) { $line = $line.Substring(1) }
                        [void] $article.Append($line).Append("`n")
                    }
                    [IO.File]::WriteAllBytes($Capture, $encoding.GetBytes($article.ToString()))
                    $writer.WriteLine('240 article accepted')
                    break
                }
                '^QUIT' { $writer.WriteLine('205 goodbye'); return }
                default { $writer.WriteLine('500 unsupported') }
            }
        }
    }
    finally { $listener.Stop(); if ($null -ne $client) { $client.Dispose() } }
}
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (![IO.File]::Exists($portFile)) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Local NNTP fixture did not start.' }
        Start-Sleep -Milliseconds 50
    }
    $env:NNTPSERVER = '127.0.0.1;' + [IO.File]::ReadAllText($portFile)
    $env:NNTP_FORCE_AUTH = ''
    & $Cmake "-DTRN_TEST_PNEWS=$(Join-Path $bin 'Pnews.cmd')" "-DTRN_TEST_PNEWS_TOOL=$Tool" `
        "-DTRN_TEST_OUTPUT_DIR=$(Join-Path $WorkDirectory 'draft')" '-DTRN_TEST_CASE=native_inews' `
        "-DTRN_TEST_ARTCHECK=$Artcheck" -P $Fixture
    if ($LASTEXITCODE -ne 0) {
        if ([IO.File]::Exists($capture + '.commands')) { Write-Output ([IO.File]::ReadAllText($capture + '.commands')) }
        Receive-Job $job -ErrorAction Continue
        throw 'Native posting workflow failed.'
    }
    $null = Wait-Job $job -Timeout 10
    Receive-Job $job -ErrorAction Stop
    if (![IO.File]::Exists($capture)) { throw 'Native inews did not post an article.' }
    $posted = [IO.File]::ReadAllText($capture)
    if ($posted -notmatch 'User-Agent: trn/' -or $posted -notmatch '\.From body' -or $posted -notmatch 'mispelled prepared body') {
        throw "Incorrect native NNTP article: $posted"
    }
}
finally { Stop-Job $job; Remove-Job $job -Force }
