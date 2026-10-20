# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
#requires -Version 5.1

param([string] $WorkDirectory, [string] $Adapter = '',
    [string] $Helper = '', [string] $OnlyCase = '')
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = [Text.UTF8Encoding]::new($true)
$utf8 = [Text.UTF8Encoding]::new($false)
[void] [IO.Directory]::CreateDirectory($WorkDirectory)
$rsa = [Security.Cryptography.RSA]::Create(2048)
$request = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
    'CN=localhost', $rsa, [Security.Cryptography.HashAlgorithmName]::SHA256,
    [Security.Cryptography.RSASignaturePadding]::Pkcs1)
$request.CertificateExtensions.Add([Security.Cryptography.X509Certificates.X509BasicConstraintsExtension]::new($true, $false, 0, $true))
$san = [Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
$san.AddDnsName('localhost')
$request.CertificateExtensions.Add($san.Build())
$certificate = $request.CreateSelfSigned([DateTimeOffset]::Now.AddDays(-1), [DateTimeOffset]::Now.AddDays(2))
$pfx = Join-Path $WorkDirectory 'server.pfx'
$ca = Join-Path $WorkDirectory 'ca.pem'
[IO.File]::WriteAllBytes($pfx, $certificate.Export('Pfx', 'fixture'))
[IO.File]::WriteAllText($ca, "-----BEGIN CERTIFICATE-----`n" +
    [Convert]::ToBase64String($certificate.Export('Cert'), 'InsertLineBreaks') +
    "`n-----END CERTIFICATE-----`n", $utf8)
$certificate.Dispose()
$rsa.Dispose()
$message = Join-Path $WorkDirectory 'message with spaces.eml'
$body = 'Unicode ' + [char] 0x00E9 + "`r`n.dot body`r`n"
$text = "From: author@example.test`r`nTo: reader@example.test`r`nCc: copy@example.test`r`n" +
    "Date: Tue, 20 Oct 2026 12:00:00 +0000`r`nMessage-ID: <fixture@example.test>`r`n" +
    "Subject: =?utf-8?B?UmVwbHkgw6k=?=`r`nIn-Reply-To: <original@example.test>`r`n" +
    "Newsgroups: example.moderated`r`nMIME-Version: 1.0`r`n" +
    "Content-Type: multipart/mixed; boundary=fixture`r`n`r`n--fixture`r`n" +
    "Content-Type: text/plain; charset=utf-8`r`n`r`n$body--fixture`r`n" +
    "Content-Type: message/rfc822`r`n`r`nSubject: forward`r`n`r`nOriginal body`r`n--fixture--`r`n"
[IO.File]::WriteAllText($message, $text, $utf8)
if ($Adapter) { . $Adapter }
foreach ($case in @('accepted', 'oauth2', 'implicit', 'rejected', 'recipient', 'authentication', 'timeout', 'cancelled', 'certificate')) {
    if ($OnlyCase -and $case -ne $OnlyCase) { continue }
    $expectAccepted = $case -in @('accepted', 'oauth2', 'implicit')
    $directory = Join-Path $WorkDirectory $case
    [void] [IO.Directory]::CreateDirectory($directory)
    $portFile = Join-Path $directory 'port'
    $capture = Join-Path $directory 'capture'
    foreach ($suffix in @('', '.authenticated', '.envelope')) { [IO.File]::Delete($capture + $suffix) }
    if ([IO.File]::Exists($portFile)) { [IO.File]::Delete($portFile) }
    $job = Start-Job -ArgumentList $portFile, $capture, $pfx, $case -ScriptBlock {
        param($PortFile, $Capture, $Pfx, $Case)
        $ErrorActionPreference = 'Stop'
        $client = $null
        $encoding = [Text.UTF8Encoding]::new($false)
        $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
        $listener.Start()
        [IO.File]::WriteAllText($PortFile + '.tmp', $listener.LocalEndpoint.Port.ToString())
        [IO.File]::Move($PortFile + '.tmp', $PortFile)
        try {
            $pending = $listener.BeginAcceptTcpClient($null, $null)
            if (!$pending.AsyncWaitHandle.WaitOne(15000)) { throw 'SMTP client did not connect.' }
            $client = $listener.EndAcceptTcpClient($pending)
            $client.ReceiveTimeout = 10000
            $stream = $client.GetStream()
            $secure = $false
            if ($Case -eq 'implicit') {
                $stream = [Net.Security.SslStream]::new($stream, $false)
                $cert = [Security.Cryptography.X509Certificates.X509Certificate2]::new($Pfx, 'fixture')
                $stream.AuthenticateAsServer($cert, $false, [Security.Authentication.SslProtocols]::Tls12, $false)
                $secure = $true
            }
            $reader = [IO.StreamReader]::new($stream, $encoding, $false, 1024, $true)
            $writer = [IO.StreamWriter]::new($stream, $encoding, 1024, $true)
            $writer.NewLine = "`r`n"; $writer.AutoFlush = $true
            $writer.WriteLine('220 localhost fixture')
            while ($null -ne ($command = $reader.ReadLine())) {
                switch -Regex ($command) {
                    '^EHLO' {
                        $writer.WriteLine('250-localhost')
                        if (!$secure) { $writer.WriteLine('250 STARTTLS') }
                        else {
                            if ($Case -eq 'oauth2') { $writer.WriteLine('250-AUTH XOAUTH2') }
                            else { $writer.WriteLine('250-AUTH PLAIN') }
                            $writer.WriteLine('250-8BITMIME')
                            $writer.WriteLine('250 SMTPUTF8')
                        }
                        break
                    }
                    '^STARTTLS' {
                        $writer.WriteLine('220 begin TLS')
                        $reader.Dispose(); $writer.Dispose()
                        $stream = [Net.Security.SslStream]::new($stream, $false)
                        $cert = [Security.Cryptography.X509Certificates.X509Certificate2]::new($Pfx, 'fixture')
                        $stream.AuthenticateAsServer($cert, $false, [Security.Authentication.SslProtocols]::Tls12, $false)
                        $reader = [IO.StreamReader]::new($stream, $encoding, $false, 1024, $true)
                        $writer = [IO.StreamWriter]::new($stream, $encoding, 1024, $true)
                        $writer.NewLine = "`r`n"; $writer.AutoFlush = $true
                        $secure = $true
                        break
                    }
                    '^AUTH PLAIN(?: (.*))?$' {
                        $token = $Matches[1]
                        if (!$token) { $writer.WriteLine('334 '); $token = $reader.ReadLine() }
                        $credentials = $encoding.GetString([Convert]::FromBase64String($token))
                        if ($credentials -ne ([char] 0 + 'fixture' + [char] 0 + 'test-password')) { throw 'Wrong fixture credentials.' }
                        [IO.File]::WriteAllText($Capture + '.authenticated', 'yes')
                        if ($Case -eq 'authentication') { $writer.WriteLine('535 rejected') }
                        else { $writer.WriteLine('235 authenticated') }
                        break
                    }
                    '^AUTH XOAUTH2(?: (.*))?$' {
                        $token = $Matches[1]
                        if (!$token) { $writer.WriteLine('334 '); $token = $reader.ReadLine() }
                        $credentials = $encoding.GetString([Convert]::FromBase64String($token))
                        if ($credentials -ne ('user=fixture' + [char] 1 + 'auth=Bearer test-token' + [char] 1 + [char] 1)) {
                            throw 'Wrong OAuth2 fixture credentials.'
                        }
                        [IO.File]::WriteAllText($Capture + '.authenticated', 'yes')
                        $writer.WriteLine('235 authenticated')
                        break
                    }
                    '^(MAIL FROM|RCPT TO):' {
                        if (!$secure) { throw 'Envelope sent without TLS.' }
                        [IO.File]::AppendAllText($Capture + '.envelope', $command + "`n", $encoding)
                        if ($command -match '^MAIL FROM:' -and $command -notmatch ' BODY=8BITMIME$') {
                            $writer.WriteLine('555 declare eight-bit message transport')
                        }
                        elseif ($Case -eq 'recipient' -and $command -match 'RCPT TO') { $writer.WriteLine('550 rejected') }
                        else { $writer.WriteLine('250 ok') }
                        break
                    }
                    '^DATA' {
                        $writer.WriteLine('354 send message')
                        $data = [Text.StringBuilder]::new()
                        while ($null -ne ($line = $reader.ReadLine()) -and $line -ne '.') {
                            if ($line.StartsWith('..')) { $line = $line.Substring(1) }
                            [void] $data.Append($line).Append("`r`n")
                        }
                        [IO.File]::WriteAllText($Capture, $data.ToString(), $encoding)
                        if ($Case -eq 'rejected') { $writer.WriteLine('554 message rejected') }
                        elseif ($Case -in @('timeout', 'cancelled')) { Start-Sleep -Seconds 12 }
                        else { $writer.WriteLine('250 queued fixture-id') }
                        break
                    }
                    '^QUIT' { $writer.WriteLine('221 goodbye'); return }
                    default { throw "Unexpected SMTP command: $command" }
                }
            }
        }
        catch { if ($Case -notin @('cancelled', 'timeout', 'certificate')) { throw } }
        finally { $listener.Stop(); if ($client) { $client.Dispose() } }
    }
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        while (![IO.File]::Exists($portFile)) {
            if ([DateTime]::UtcNow -gt $deadline) { Receive-Job $job; throw 'SMTP fixture did not start.' }
            Start-Sleep -Milliseconds 50
        }
        $port = [IO.File]::ReadAllText($portFile)
        $config = Join-Path $directory 'account.conf'
        $options = "url = smtp://localhost:$port`nuser = fixture:test-password`nlogin-options = AUTH=PLAIN`n"
        if ($case -ne 'certificate') { $options += 'cacert = "' + $ca.Replace('\', '/') + '"' + "`n" }
        [IO.File]::WriteAllText($config, $options, $utf8)
        if ($Adapter -and $case -ne 'cancelled') {
            $password = [Security.SecureString]::new()
            foreach ($character in 'test-password'.ToCharArray()) { $password.AppendChar($character) }
            $credential = [Management.Automation.PSCredential]::new('fixture', $password)
            $credentialFile = Join-Path $directory 'credential.xml'
            $credential | Export-Clixml -LiteralPath $credentialFile
            $account = Join-Path $directory 'account.json'
            $settings = @{url="smtp://localhost:$port"; sender='author@example.test'; credentialFile=$credentialFile; timeoutSeconds=3}
            if ($case -eq 'implicit') { $settings.url = "smtps://localhost:$port" }
            if ($case -eq 'oauth2') {
                $settings.Remove('credentialFile')
                $settings.username = 'fixture'
                $settings.oauth2TokenFile = Join-Path $directory 'token'
                [IO.File]::WriteAllText($settings.oauth2TokenFile, 'test-token', $utf8)
            }
            if ($case -ne 'certificate') { $settings.caFile = $ca }
            [IO.File]::WriteAllText($account, ($settings | ConvertTo-Json), $utf8)
            $env:RNMAILER_CONFIG = $account
            $env:RNMAILER = (Get-Command curl.exe -CommandType Application).Source
            $inputMessage = Join-Path $directory 'draft.eml'
            [IO.File]::WriteAllText($inputMessage, $text.Insert($text.IndexOf('Subject:'), "Bcc: hidden@example.test`r`n"), $utf8)
            $success = $false
            try {
                if ($Helper) {
                    $homeDirectory = Join-Path $directory 'home'
                    [void] [IO.Directory]::CreateDirectory($homeDirectory)
                    $env:HOME = $homeDirectory; $env:DOTDIR = $homeDirectory
                    $env:FAST_RNMAIL = 'no'; $env:PGPCOMMAND = ''
                    $env:MAILSIGNATURE = Join-Path $homeDirectory 'absent'
                    $env:MAILRECORD = Join-Path $directory 'sent.mbox'
                    $env:REPLYSAVER = ''
                    $include = Join-Path $directory 'empty include'
                    [IO.File]::WriteAllBytes($include, [byte[]] @())
                    $composer = [Diagnostics.Process]::new()
                    $composer.StartInfo.FileName = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
                    $composer.StartInfo.Arguments = '-NoLogo -NoProfile -ExecutionPolicy Bypass -File "' +
                        [IO.Path]::ChangeExtension($Helper, '.ps1') + '" -h "' + $inputMessage + '"'
                    $composer.StartInfo.UseShellExecute = $false
                    $composer.StartInfo.RedirectStandardInput = $true
                    [void] $composer.Start()
                    $composer.StandardInput.WriteLine($include)
                    $composer.StandardInput.WriteLine('s')
                    $composer.StandardInput.Close()
                    $composer.WaitForExit()
                    if ($composer.ExitCode -ne 0) { throw 'Installed mail composer failed.' }
                    $composer.Dispose()
                    if (![IO.File]::Exists($env:MAILRECORD)) { throw 'Installed mail composer did not save sent mail.' }
                }
                else { Send-TrnMail $inputMessage }
                $success = $true
            }
            catch { if ($expectAccepted) { throw } }
            if ($expectAccepted -ne $success) { throw "Incorrect adapter outcome: $case" }
            if (![IO.File]::Exists($inputMessage)) { throw 'Adapter removed the draft.' }
        }
        else {
            $process = [Diagnostics.Process]::new()
            $process.StartInfo.FileName = (Get-Command curl.exe -CommandType Application).Source
            $process.StartInfo.Arguments = '--disable --silent --show-error --ssl-reqd --max-time 3 --config "' +
                $config + '" --mail-from "<author@example.test> BODY=8BITMIME" --mail-rcpt reader@example.test --mail-rcpt copy@example.test' +
                ' --mail-rcpt hidden@example.test --upload-file "' + $message + '"'
            $process.StartInfo.UseShellExecute = $false
            $process.StartInfo.RedirectStandardError = $true
            [void] $process.Start()
            if ($case -eq 'cancelled') {
                $deadline = [DateTime]::UtcNow.AddSeconds(5)
                while (![IO.File]::Exists($capture) -and !$process.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 25 }
                if (!$process.HasExited) { $process.Kill() }
            }
            $errorText = $process.StandardError.ReadToEnd()
            $process.WaitForExit()
            if ($expectAccepted -ne ($process.ExitCode -eq 0)) { throw "${case}: status $($process.ExitCode): $errorText" }
            $process.Dispose()
        }
        if ($expectAccepted) {
            if ([IO.File]::ReadAllText($capture) -cne $text) { throw 'Complete MIME message was not preserved.' }
            $envelope = [IO.File]::ReadAllText($capture + '.envelope')
            foreach ($recipient in @('reader', 'copy', 'hidden')) {
                if ($envelope -notmatch "RCPT TO:<$recipient@example.test>") { throw "Missing recipient: $recipient" }
            }
            if (![IO.File]::Exists($capture + '.authenticated')) { throw 'Account was not authenticated.' }
        }
        if ($case -in @('rejected', 'timeout', 'cancelled') -and ![IO.File]::Exists($capture)) {
            throw "The $case probe did not reach DATA."
        }
        if ($case -in @('recipient', 'authentication', 'certificate') -and [IO.File]::Exists($capture)) {
            throw "The $case probe uploaded message content."
        }
        if ($case -notin @('timeout', 'cancelled')) {
            $null = Wait-Job $job -Timeout 5
            Receive-Job $job -ErrorAction Stop
        }
        Write-Output "SMTP $case passed"
    }
    finally { Stop-Job $job; Remove-Job $job -Force }
}
