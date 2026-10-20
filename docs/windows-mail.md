# Windows Mail Submission

Windows uses PowerShell 5.1 for Rnmail composition and native curl for
SMTP submission.  Install a maintained curl build with SMTP, SMTPS, and
TLS support if the Windows copy lacks those protocols.  `RNMAILER`
selects this curl-compatible executable, optionally with fixed arguments;
the default is `curl.exe`.  Unix Rnmail retains its native mailer
interface.

The alternatives were Simple MAPI and MailKit.  Simple MAPI passes a
structured MapiMessage to an installed mail client; its interface has no
raw RFC 5322 header block.  That does not establish preservation of custom
reply headers and complete MIME or moderated articles.  MailKit provides
MIME and authenticated SMTP, but adds .NET assemblies and package
deployment.  Native curl already supplies the required raw-message,
envelope, TLS, and authentication interfaces.  The composer remains a
script.  See [MapiMessage][m], [MailKit][mailkit], and [curl][curl].

Set `RNMAILER_CONFIG` to an account JSON file.  Required fields are `url`
(`smtp://host:587` for mandatory STARTTLS, or `smtps://host:465`) and
`sender` (an ASCII mailbox).  Optional fields:

- `credentialFile`: a PSCredential exported using `Export-Clixml` under
  the same Windows account and machine that runs trn.  Obtain it with
  `Get-Credential | Export-Clixml -LiteralPath account.xml`.  Use an app
  password if the provider requires one; ordinary passwords may be denied.
- `username` and `oauth2TokenFile`: an account name and a protected file
  containing a current OAuth2 access token.  Provider consent and token
  renewal are performed outside trn.  Do not specify both credential forms.
- `caFile`: a PEM CA bundle for a private submission service.  Otherwise
  curl uses its configured trust store.  Verification cannot be disabled.
- `timeoutSeconds`: total submission deadline; default 120, range 1-600.

Relative account paths resolve beside the JSON file.  Without credentials,
the account explicitly selects a TLS relay that permits unauthenticated
submission.  Protect account and token files using Windows file access
permissions.  Credentials go to curl through standard input; they are
never command-line arguments or diagnostic output.  Default curl config
files are disabled.  Server URLs cannot contain credentials.

To, Cc, and Bcc supply separate envelope recipients.  Bcc and its folded
continuations are removed from uploaded headers.  Standard comma-separated
mailbox lists, including quoted display names, are supported.  Envelope
mailboxes must be ASCII; internationalized local parts require a different
SMTPUTF8 adapter and are rejected before upload.  Unicode display names
and unstructured headers use RFC 2047 encoding.  Existing MIME headers and
body bytes are retained, including message/rfc822 forwards.  New plain-text
messages use UTF-8 MIME.  SMTP line endings and final newline are supplied.
Eight-bit bodies require the submission server's 8BITMIME extension;
the adapter declares BODY=8BITMIME while retaining existing MIME bytes.
A sender that cannot pass that SMTP envelope parameter fails the probe.

Success requires curl exit zero and a final SMTP DATA response of 250.
This means the server accepted responsibility, which may include queueing;
it does not confirm inbox delivery.  There is no desktop-client handoff
mode.  Rejection, timeout, cancellation, or missing confirmation preserves
the draft and does not create a sent record.  A connection lost after DATA
may have delivered the message; inspect the server before retrying.

The native Windows probe uses a loopback SMTP server, a temporary private
CA, mandatory STARTTLS, and test-only credentials.  It never sends outside
the machine or installs a certificate.  On 2026-10-20, Windows curl 8.21.0
with Schannel passed full multipart MIME and UTF-8 body preservation,
custom reply/news headers, three envelope recipients, authentication,
post-DATA acceptance/rejection, recipient/authentication rejection,
timeout, process cancellation, and certificate rejection.  The initial
`cmake --workflow temp` probe passed in 7.79 seconds.  Adapter regression
tests also cover OAuth2, implicit TLS, and eight-bit transport.  Bcc is
removed without changing the draft or an encapsulated original message.

[m]: https://learn.microsoft.com/windows/win32/api/mapi/ns-mapi-mapimessage
[mailkit]: https://github.com/jstedfast/MailKit
[curl]: https://curl.se/docs/manpage.html
