<!-- Copyright (c) 2026, Richard Thomson -->

# Release 5 Plan

## Goal

Ship trn 5.0 as the first CMake-native release of the revived codebase.
The release should be buildable from current instructions, pass the
supported CI matrix, avoid known crash-class bugs, and provide a usable
Unix and Windows package story.

The release is not a full rewrite.  The line is:

- fix release-blocking correctness, build, and packaging defects;
- finish the CMake migration for current runtime policy;
- add enough deterministic test infrastructure to modernize NNTP safely;
- implement the NNTP and posting pieces needed for current use;
- defer broad internal cleanup unless it unlocks one of those goals.

## Current State

The INI refactor described in `ini-words.md` is implemented in current
source.  `IniSchema`, `IniDocument`, `IniSectionValues`, `OptionCatalog`,
`OptionDraft`, `OptionApplier`, `DataSourceConfig`, and `RcGroupConfig`
are present, wired into `libtrn`, and covered by focused tests.

The old `FILE_REF` macro task is also complete enough for this plan.
Source uses `file_ref`, and the config target has direct tests for it.

Those two areas should be treated as release verification only, not as
active feature slices.

NNTP capability discovery, modern commands, STARTTLS, and implicit TLS
are implemented.
`NNTPLink` retains capabilities, a sorted vector of rejected commands,
transport security state, and group/article selection.  STARTTLS verifies
certificates and host names, refreshes capabilities, and restores reading
state.  Data-source policy can require TLS before authentication or POST.
Implicit TLS starts before the greeting on port 563 or a configured
port.  Authentication honors advertised USER support, configurable
legacy fallback, and TLS policy.  SASL mechanism execution remains
future work behind a separate method dispatcher.

OpenSSL 3.2 or newer is now required.  The Windows ZIP currently contains
no runtime DLLs; collecting required DLLs remains an install/package
release gate.

Slice 28 test-first evidence, using `cmake --workflow temp` throughout:

- The first six tests failed for missing policy fields, STARTTLS startup,
  failure handling, and `483` retry.  The same six passed after those
  paths were implemented.
- Expanded coverage failed six of 14 tests for transport, policy, and
  forced-auth ordering.  Those 14 passed after implementation; a new
  selection-restoration test then failed as intended.
- The broader run failed five of 140 tests for selection restoration,
  short policy values, and startup conversations.  All 140 passed after
  implementing the fixes and updating the startup expectations.
- A final discovery-auth regression failed with duplicate `AUTHINFO USER`
  requests.  Clearing pending forced auth made all 141 tests pass.

The slice 28 `cmake --workflow rt-default` run passed 1402 tests with six
skips.  Coverage includes encrypted loopback article reads, certificate
rejection, TLS policy, reader-mode selection, legacy fallback, rejection
caching, empty HDR values, and batched overview retrieval.

Slice 29 test-first evidence, using `cmake --workflow temp` throughout:

- Restricted to `DataSourceConfigTest.recognizesImplicitTlsField`, the
  first run failed because the schema lacked the setting.  The same
  test passed after the field was added.
- The first six-test expansion failed five tests for missing data-source
  wiring, implicit startup, handshake failure handling, and TLS policy.
- With three encrypted loopback tests added, eight of nine tests failed.
  Five unit tests failed assertions; three fixture tests timed out while
  the client waited for a cleartext greeting instead of sending a TLS
  handshake.  The default-port expectations also require port 563.
- After wiring configuration, negotiating TLS before the greeting, and
  accepting either encrypted state for policy checks, the same nine
  tests passed.  They cover custom ports, reconnects, reader mode, and
  rejection of untrusted or wrong-host certificates.
- The broader NNTP/data-source filter passed all 150 tests, including
  STARTTLS and cleartext compatibility coverage.

The final slice 29 `cmake --workflow rt-default` run passed 1411 tests
with six skips and no failures.  Direct TLS regression coverage for #32
now includes encrypted article reads and certificate verification.

Slice 30 test-first evidence, using `cmake --workflow temp` throughout:

- The first auth/config filter failed 20 of 21 tests for unconditional
  USER/PASS, missing compatibility policy, reply handling, state resets,
  repeated challenges, and stale capabilities.  The same 21 tests passed
  after method selection, reply validation, and session state were added.
- Three new regressions failed among 24 tests: PASS replay across TLS,
  stale auth-method selection after TLS, and startup hiding a failed
  post-auth capability refresh.  The same 24 passed after those fixes.
- A final discovery-auth test failed with duplicate USER commands when
  TLS capability discovery authenticated the session.  Reusing that
  authenticated state made the same 25 tests pass.
- The broader NNTP/data-source filter passed all 175 tests, covering
  advertised auth, legacy and strict modes, cleartext refusal, STARTTLS,
  implicit TLS, command selection, and retained-buffer article reads.

The final slice 30 `cmake --workflow rt-default` run passed 1436 tests
with six skips and no failures.  The command, STARTTLS, implicit TLS,
and authentication paths satisfy #3; it is ready to close on publication.

## Script Audit

Repo-owned shell work excludes the `vcpkg` submodule.  The current
runtime shell templates have Windows `.cmd` counterparts that only print
unsupported diagnostics.  Issue #29 requires working replacements for
retained helpers and removal of the unused `filexp` helper:

- `config/data/Pnews.in`: interactive news poster; sources
  `Pnews.header`, calls `Speller`, `mbox.saver`, `trn-artchk`,
  `nntplist`, `inews`, the editor, the pager, and optional PGP tools.
- `config/data/Pnews.header.in`: shell scriptlet sourced by `Pnews`;
  include its behavior in Windows `Pnews`, retain it for Unix `Pnews`,
  and do not treat it as a standalone command.
- `config/data/Rnmail.in`: reply and forward mailer; generated by CMake,
  with editor, spell-checker, signature, and mailer integration.
- `config/data/Speller.in`: spell-check wrapper using `awk`, `sed`,
  `diff`, `ed`, `spell` or `ispell`, `pr`, a pager, and temp files.
- `config/data/filexp.in`: tilde and install-prefix path expansion using
  `sed`, `expr`, `/etc/passwd`, and optional `/bin/csh` globbing.
  No current runtime helper calls it; remove it on both platforms.
  Calls in `config/legacy` belong to the historical build system.
- `config/data/mbox.saver.in`: mailbox append helper using shell
  redirection, `tail`, and `sed`.
- `config/data/norm.saver.in`: normal save helper using shell
  redirection and `tail`.

Generated data files do not need Windows executable equivalents:
`config/data/newsnews.in`, `config/data/access_def.in`, and
`config/data/environment.in`.  However, `environment.in` and generated
config defaults still reference shell commands and must get Windows-safe
defaults where the feature is supported: `NEWSPOSTER`, `MAILPOSTER`,
`MBOXSAVER`, `NORMSAVER`, `CANCEL`, `EXSAVER`, `PIPESAVER`, `UNSHAR`,
`PREF_SHELL`, and `SHAR_SAVER`.

On Unix-like systems, assume standard modern POSIX tools are present.
Do not add CMake search or cache configuration for `awk`, `cat`,
`egrep`, `grep`, `mail`, or `sed` merely because legacy Configure
located them.

Legacy build scripts are not runtime Windows porting targets:
`config/legacy/Configure`, `config/legacy/Makefile.SH`, and
`config/legacy/hints/*.sh` should be retired from the active build path
or kept only as historical reference.

## Helper Implementation Choices

Issue #29 is a mandatory 5.0 milestone issue.  Slice 41 removes the unused
`filexp` helper; slices 42 through 46 add working Windows equivalents for
the five retained Unix helpers.  PowerShell is the default choice;
`Rnmail` remains provisional until its submission backend is proved.  For
script ports, retain the Unix scripts and replace Windows diagnostic stubs
with working launchers and scripts.  `Pnews.header` behavior belongs within
Windows `Pnews`; retain the Unix scriptlet.

Implement email submission last, in slice 46.  Slice 45 covers `Pnews`
composition and NNTP posting without depending on the Windows email
backend choice.  Slice 46 also completes `Pnews` mail copies and moderated
email submissions through the shared backend.

The audited file, text, and orchestration logic does not itself require
C++.  Windows email submission remains an unverified platform dependency,
not evidence that a suitable native mailer is already available.  C++
could share implementations across platforms and avoid a script runtime,
but would also rewrite working Unix workflows and add compiled-command
integration.  Existing CMake/gtest use alone does not justify that cost;
CTest can exercise scripts and fake tools.

Per-helper assessment and planned choice:

- `filexp`: remove the unused Unix helper and Windows stub, including
  generation, installation, packaging, tests, and active documentation.
  Preserve the separate C++ `file_exp` implementation used by trn.
- `norm.saver`: small PowerShell script for byte seeking, stream copying,
  append markers, and separators.  Use .NET file streams directly.
- `mbox.saver`: small to moderate PowerShell script adding envelope and
  control-A formats and line-prefix escaping to the normal saver logic.
- `Speller`: moderate PowerShell port for protected headers, quotations,
  signatures, and checker invocation.  Text preservation and reinsertion
  need tests; C++ does not remove the external checker dependency.
- `Pnews`: larger PowerShell workflow including header/distribution
  handling.  Reuse `trn-artchk`, `nntplist`, and `inews` for their existing
  work; do not reimplement their functions in the script.
- `Rnmail`: larger PowerShell workflow for prompts, editing, checking,
  and draft recovery.  Select and prove the Windows submission backend
  before confirming the script choice; delivery cannot be assumed to
  match Unix command-line mailers.

Target Windows PowerShell 5.1 rather than requiring a PowerShell 7
installation.  Use tested `.cmd` launchers for existing command names.
Verify argument quoting and script launch from default Windows settings
without changing machine/user execution policy.  Copy article bytes and
feed native process input through explicit .NET streams; PowerShell text
pipelines and default file redirection are not byte-preserving savers.

If a later slice selects C++, record the concrete benefit or blocker in
that slice.  Its executable must replace the Unix script too, with the
obsolete Unix/Windows templates and generation/install rules removed.
Do not keep a parallel Unix script for a command replaced with C++.

Each script port follows the strict test-driven sequence in
Implementation Rules.  First pin its arguments, environment, input/output,
interactive behavior, and failure handling with fixtures.  Observe the
new script or integration test fail before implementing its behavior,
then rerun that same test successfully.  Add failures before implementing
each additional behavior.

Each replacement must:

- run on native Windows without WSL, MSYS, Cygwin, Bash, or POSIX text
  utilities;
- preserve the command's supported workflow and configuration overrides;
- handle filenames and executable paths containing spaces, pass child
  arguments correctly, and propagate child failures;
- use isolated temporary files and preserve user data after failures or
  cancellation;
- have deterministic tests using fake editors, pagers, spell-checkers,
  mailers, and posting transports, with no live delivery;
- wire Windows defaults and helper paths to the launcher/script and ship
  all required script support in installed/package artifacts;
- test that Windows callers reach the implementation rather than an
  unsupported stub, and retain Unix script behavior and package checks;
- document native tool configuration and supported behavior, and retain
  Unix regression coverage for the replaced script.

Configured editors, spell-checkers, mailers, and optional PGP tools must
be native executables on Windows.  Their configuration and failure paths
must be exercised; a Unix-only default or an unsupported placeholder is
not a Windows implementation.  Share PowerShell support as individual
helper slices need it.

## Windows Email Submission

`MAILPOSTER` selects the composer invoked by trn for replies and moderator
submissions; `FORWARDPOSTER` selects the forwarding composer.  The Unix
`Rnmail` script then uses `RNMAILER`, defaulting to `/bin/mail`, with
special handling for sendmail-like and other command-line interfaces.
The Windows port needs an explicit submission contract and supported
backend, not just a translated script and a fake command that exits zero.

Separate composition from submission.  Composition covers recipients,
headers, quoted text, MIME forwarding, editing, signatures, spell-checking,
and recovery.  Submission covers the backend's message and recipient
interface, account configuration, acceptance/cancellation/failure status,
and any required runtime or mail-client dependency.

Slice 46 must resolve the backend before implementing its composer:

- Evaluate a configured native command-line sender or SMTP submission
  library against desktop-client integration such as Simple MAPI.  Record
  the supported backend, required installed components, and account setup.
  Do not assume a Unix-compatible mail command or a particular desktop
  mail client is present.
- A desktop-client handoff must preserve the required message content
  and report handoff separately from submission.  Launching `mailto:` or
  opening a draft is not proof of sending a message.  Check MIME forwards,
  custom reply headers, and complete moderated articles before accepting
  a client integration as the general backend.
- If SMTP is selected, use a maintained sender/library with the required
  TLS and account authentication support.  Do not substitute PowerShell's
  obsolete `Send-MailMessage` as the production backend.  Document account
  and credential configuration and keep credentials out of command logs.
- Define how To/Cc/Bcc envelope recipients and message headers are passed,
  how Bcc stays out of transmitted headers, and how Unicode, MIME content,
  timeouts, cancellation, and failed submissions are handled.
- Preserve drafts until the backend has accepted submission or queueing.
  A successful launch alone must not trigger sent-mail recording or draft
  deletion.  Backend acceptance is not confirmation of inbox delivery.
- Use fake backends for deterministic orchestration tests, plus a checked
  native Windows integration probe with a controlled local endpoint or
  supported client.  A capture executable alone does not establish that
  the real Windows backend works.  No live external email is sent by CI.

The backend choice is still open.  Record probe evidence before locking
the final `Rnmail` implementation language.  If C++ is selected for the
command, replace Unix `Rnmail` too, as required above.  `Pnews` mail copies
and moderator submission must use the same checked submission contract;
slice 46 owns their backend integration and acceptance checks.

## Documentation Audit

Markdown files are excluded from this audit.

Non-markdown documentation sources audited:

- top-level text: `INIT.txt`, `HACKERSGUIDE.txt`, and `default_topic`;
- man pages: `man/trn.1`, `man/Pnews.1`, and `man/Rnmail.1`;
- online help files under `HelpFiles`;
- generated data and help text: `config/data/access_def.in`,
  `config/data/environment.in`, and `config/data/newsnews.in`;
- compiled help and option labels in `libtrn/help.cpp`,
  `libtrn/OptionCatalog.cpp`, and scoring help call sites.

Documentation gaps and owning work items:

- CMake installs `trn`, helper commands, generated helper/data files,
  `HelpFiles`, supported man pages, and selected top-level release text
  files.
- `man/Rnmail.1` describes shell helpers before the CMake helper
  migration.  Slice 46 owns Windows mailer and launcher documentation.
- `man/trn.1` and `config/data/environment.in` still document Unix shell
  defaults such as `Rnmail`, `Pnews`, `tail`, and `/bin/sh`.  Slices 41
  through 46 own helper defaults and their documentation; other shell
  command defaults still need the platform policy described above.
- `man/trn.1`, `config/data/access_def.in`, and
  `HelpFiles/samples/access` now describe implicit TLS and authentication
  policy.  Older `XGTITLE` wording still needs synchronization.
- `config/data/access_def.in` omits live access-schema fields such as
  `XHDR Broken` and `Xrefs`, and contains the malformed sample
  `Force Auth = [yes|no}`.  Items 8 and 10 own generated data
  documentation sync.
- Compiled option help in `libtrn/help.cpp` drifts from
  `libtrn/OptionCatalog.cpp`: `Select My Postings` omits `thread`, and
  `Active File Refetch Mins` does not match `Default Refetch Time`.
  Item 23 owns help/catalog comparison.
- Selector help still says add-group ordering and reverse ordering are
  not working.  Item 23 must verify that text against current selector
  behavior.
- Scoring help paths still print "No help available (yet)."  Item 23
  should classify these as documented, deferred, or obsolete.
- The online help browser looks for `%X/sitehelp/top` and
  `%X/HelpFiles/top`; CMake installs `HelpFiles` under the private
  library.  Items 20 and 23 own package and verification coverage.
- Feature docs need updates as release work lands: UTF-8 and restored
  charset conversions, optional `UTF-8 => ASCII`, yEnc extraction,
  TLS/nntps/auth policy, and Windows/macOS platform status.  Items 21,
  22, and 26 through 36 own the matching feature-specific documentation.

## Charset Analysis

Plan source: `charset-handling.md`.

The release contract is defined in
[`charset-pipeline.md`](charset-pipeline.md):

- stored or fetched article data remains raw and immutable;
- transfer encodings are decoded before charset conversion;
- human-readable text is converted to Unicode, then emitted as UTF-8;
- character substitution consumes and produces Unicode text;
- layout, wrapping, colors, cursor movement, and backward paging are
  display concerns over final UTF-8;
- decoder and substitution state belongs to an article, header, or MIME
  presentation instance, not to process-global variables.

RFC 2047 words and MIME text sections now retain explicit charset
selection for ASCII, UTF-8, Latin-1, Latin-9, and Windows-1252.  Unknown
or missing charsets use deterministic UTF-8 fallback; malformed bytes
become U+FFFD.  Transfer decoding precedes charset conversion, and the
presentation cache contains UTF-8.  Raw article bytes remain unchanged.
Legacy charset helpers remain for compatibility; the restored decoding,
HTML scanning, and terminal output paths do not depend on their mutable
global selection.

The new `decode_utf8_scalar` and `encode_utf8_scalar` operations enforce
modern Unicode scalar boundaries without global charset state.  Invalid
input reports an error with one-byte progress; empty input consumes zero
bytes.  Encoding rejects surrogates and values above U+10FFFF.

Historical substitution now consumes Unicode scalars, preserves unmapped
UTF-8, and keeps TeX prefixes in a `CharSubstitution` instance.  Each
header or compression call uses fresh substitution state.  Malformed
bytes become U+FFFD without consuming subsequent valid characters.

The optional `u` profile and `[unicode substitutions]` configuration now
support scalar-keyed mappings over built-in punctuation and Latin
approximations.  Defaults remain unchanged.  The #34 table and equivalent
header/body behavior are covered by scalar and presentation tests.
The larger
`ArticlePresentationView` and `PresentationBuffer` architecture can
remain a follow-up unless it is the smallest correct release change.

Slice 31 test-first evidence, using `cmake --workflow temp` throughout:

- The test executable failed to link before either new scalar operation
  was implemented.
- Temporary adapters to the legacy helpers ran 68 tests; 18 failed for
  overlong forms, surrogates, out-of-range scalars, obsolete encodings,
  and global charset dependence.
- Replacing those adapters with bounded, pure scalar operations made the
  same 68 tests pass.  Coverage includes all scalar-width boundaries,
  NUL, noncharacters, CJK, emoji, malformed continuation, truncation,
  recovery progress, and retrying partial views without retained state.
- Header pipeline coverage retains raw input, transfer-decodes Q and
  Base64 before Latin-1 conversion, preserves plain-substitution UTF-8,
  and gives layout the final UTF-8 text.

The final slice 31 `cmake --workflow rt-default` run passed 1504 tests
with six skips and no failures, including the existing UTF-8, MIME,
header, and display regression tests for #2.

Slice 32 test-first evidence, using `cmake --workflow temp` throughout
the focused runs:

- The first run failed 14 of 43 tests for byte-oriented Latin mappings,
  damaged unmapped Unicode, malformed recovery, line bounds, inactive
  TeX conversion, and substitution before header/name/subject layout.
- Four additional tests failed to link before `CharSubstitution` was
  implemented.  They cover interleaved instances, prefixes across calls,
  flushing and reset, and discarding state between articles.
- Scalar substitution and instance-owned TeX prefixes made all 47 tests
  pass.  Coverage includes identity, embedded NUL, CJK, combining marks,
  emoji, U+2019, and independence from global charset selection.
- The first `cmake --workflow rt-default` run found two existing status
  tests failing: including the scalar header enabled a legacy branch
  that hid the selected profile.  Retaining the profile status labels
  made the expanded 55-test focused run pass, including both failures.

The final slice 32 `cmake --workflow rt-default` run passed 1522 tests
with six skips and no failures.  The UTF-8, MIME, header, and display
regression coverage for #2 remains green.

Slice 33 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The first run failed 11 of 29 tests for stale charset selection,
  malformed recovery, global control filtering, whitespace handling,
  and MIME body bytes reaching the presentation cache undecoded.
- Seven decoder and terminal tests then failed to compile because the
  explicit APIs were missing.  Implementing per-word and per-section
  decoders, UTF-8 buffering, and terminal operations made all 36 pass.
- The expanded 94-test run failed eight tests for cached split scalars,
  unfinished MIME text, repeated TeX conversion, global HTML/name state,
  and header/subject truncation splitting UTF-8.  Retaining partial bytes
  per section and using scalar-aware layout left one boundary test
  failing because suppressed subheaders produced an extra blank line.
- Adding final-HTML-line substitution coverage produced two failures
  among 95 tests.  Suppressing that blank line and substituting the final
  filtered line made the same 95 pass.
- A mode-change regression initially failed in terminal fixture setup.
  After correcting the fixture, the old implementation failed its cache
  invalidation and first-page assertions.  Clearing transformed text and
  restarting display when conversion mode changes made all 96 pass.

The final slice 33 `cmake --workflow rt-default` run passed 1557 tests
with six skips and no failures.  Coverage includes source retention,
cache growth and rereads, transfer-decoded scalar fragments, section/EOF
finalization, unknown and malformed charsets, repeated articles, and #2
regressions.  Historical conversions for #17 are implemented locally.

Slice 34 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The initial 87-test run failed the two new Unicode mode tests because
  punctuation remained unchanged and the status label was missing.
- Seven new profile/configuration tests then failed to compile because
  the profile, explicit-profile constructor, and option loader APIs were
  absent.  Implementing scalar mappings, sorted-vector lookup, section
  loading, validation, and profile snapshots left one failure among 94
  tests: an escaped quote swallowed the following INI settings.
- Adding direct INI and decoded-header regressions produced three
  failures among 112 tests.  Honoring escaped quotes while scanning INI
  values and enabling the Unicode profile for headers made all 112 pass.

The slice 34 `cmake --workflow rt-default` run passed 1568 tests with six
skips and no failures.  Tests cover opt-in/opt-out, unchanged defaults,
user precedence, escaped and empty replacements, invalid-section
retention, independent instances, saved configuration, and #2 regressions.
Configuration syntax and source-byte decoding are documented in
`charset-pipeline.md` and the shipped help/sample configuration.

Slice 35 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The initial 141-test run failed 17 tests: 16 missing scalar mappings
  and ISO-8859-1 U+0085 newline conversion.  Plain UTF-8, unknown scalars,
  malformed recovery, and nonrecursive user replacements already passed.
- Adding MIME/header equivalence, escaped replacement, deletion, cache
  reread, and malformed presentation tests produced 18 failures among
  144 tests.  The extra failure covered the missing smiley and narrow
  space mappings through decoded headers and MIME bodies.
- Completing the sorted built-in table made the same 144 tests pass.
  All 25 issue-related scalar cases preserve their original UTF-8 in
  plain mode.  Windows-1252 examples are decoded before substitution;
  their Unicode mappings take precedence over ambiguous byte examples.

The slice 35 `cmake --workflow rt-default` run passed 1600 tests with six
skips and no failures, including the #2 UTF-8 regressions.  The shipped
table, source-charset interpretation, and mapping policies are documented
in `charset-pipeline.md`.  Issue #34 is implemented locally and ready to
close; its closing commit must include `Fixes #34`.

## yEnc Extraction

The `e` command recognizes yEnc headers and decodes original article
bytes.  Single-part and multipart extraction use the platform filename
policy, validate sizes and checksums, and assemble complete parts through
a staging file.  Parts may arrive out of order; a missing total can be
inferred from the final byte range.  Existing uuencode, custom-command,
and shell archive extraction paths remain covered.

Slice 36 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The initial command test failed among 36 tests because `e` did not
  recognize yEnc and produced no decoded file.
- Seven decoder and five extraction tests then failed to compile because
  the yEnc API was absent.  Implementing prescan, decoding, validation,
  filename handling, and multipart storage/assembly made all 48 pass.
- Expanded all-byte and malformed-input coverage failed two of 54 tests.
  Windows text input lost the article body before prescan, and an escaped
  raw NUL was accepted.  Reading local extraction input as binary,
  recomputing its body offset, and rejecting raw NUL made all 54 pass.
- An NNTP cache regression then failed among 58 tests on the second read
  of the all-byte article.  Binary cache I/O made the same 58 pass.

The slice 36 `cmake --workflow rt-default` run passed 1620 tests with six
skips and no failures.  Coverage includes all 256 byte values, critical
escapes, CRC/size failures, out-of-order and missing-total multipart
articles, source filenames, ordinary MIME text wrappers, and the existing
extraction paths.  `yenc-extraction.md` documents the format and behavior.
Issue #5 is implemented locally and ready to close; its closing commit
must include `Fixes #5`.

## UTF-8 Underline Display

The subject from the article attached to
[#30](https://github.com/LegalizeAdulthood/trn/issues/30) is retained as a
small regression fixture with its original mixed Q and Base64 words.
Earlier charset slices reached the header display path, so this work was
implemented rather than deferred.  Start/stop underline sequences are
balanced around ordinary and wrapped subjects.  Underlining one character
at a time now emits complete scalars before cursor commands, uses display
widths, and ignores legacy charset selection.

Slice 37 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The initial 43-test run failed the attached subject's character
  underline test: cursor and underline commands split UTF-8 sequences.
  The start/stop attribute-restoration regression already passed.
- Expanded coverage failed four of 47 tests before implementation.  Both
  underline paths damaged UTF-8; wide/combining characters, malformed
  bytes, legacy charset independence, and visible controls also failed.
  Wrapped-subject attribute restoration already passed.
- Whole-scalar terminal output, display-column cursor movement, and
  visible control rendering made the same 47 tests pass.

The slice 37 `cmake --workflow rt-default` run passed 1626 tests with six
skips and no failures.  `charset-pipeline.md` documents the display
contract.  Issue #30 is implemented locally and ready to close; its
closing commit must include `Fixes #30`.

## Moderator Routing

`Moderators File` selects ordered INN-style wildmat rules for each source.
Local sources default to a file beside the active file; NNTP sources use
advertised `LIST MODERATORS`.  Templates expand `%s` and `%%`.  Matching
unapproved posts to moderated groups use `MAILPOSTER`, with unique
submission addresses in `To` and the news headers and body retained.
Missing rules and unavailable lists preserve normal news posting.
`moderator-routing.md` and both access samples document configuration.

Slice 38 test-first evidence, using `cmake --workflow temp` for all
focused runs:

- The initial 96-test run failed the new schema test because
  `Moderators File` was absent.
- Configuration and moderator tests then failed to compile because the
  getter/setter, parser, and data-source lookup APIs were missing.
  Implementing the schema and ordered local/remote lookup made 108 tests
  pass.
- Access-policy and posting regressions failed six of 117 tests for
  missing path/default handling and composer routing.  Loading source
  policy and routing outgoing moderated posts made the same 117 pass.
- Expanded failure coverage failed one of 120 tests: a disconnected
  server attempted to reconnect.  Guarding disconnected lookup and
  adding remote-post/composer-failure checks made all 122 pass.
- Two additional regressions failed among 124 tests because disabled and
  unadvertised routing still queried active groups.  Checking source
  policy and capability support first made the same 124 tests pass.

The final slice 38 `cmake --workflow rt-default` run passed 1655 tests
with six skips and no failures.  Coverage includes first-match rules,
exclusions, address templates, source overrides, complete and incomplete
NNTP replies, folded/crossposted headers, deduplication, approved posts,
and composer invocation.  Issue #25 is implemented locally and ready to
close; its closing commit must include `Fixes #25`.

AddGroup storage now owns objects in a vector.  Stable `AddGroupIndex`
values identify groups, selector cursors, and page items.  Sorting changes
a separate index vector, with constant-time forward/backward traversal.
A name-sorted index vector provides binary-search duplicate detection
before insertion.  Local and NNTP discovery collect directly into this
table, retaining the first candidate and its source metadata.

Slice 39 test-first evidence, using `cmake --workflow temp` throughout:

- The initial run failed to compile because `AddGroupTable`,
  `AddGroupIndex`, and the discovery test entry points were absent.
- After implementing vector ownership and converting selector state and
  traversal, the same focused selection passed all 36 tests.  Coverage
  includes duplicate candidates within NNTP replies and across local
  sources, cleanup and reuse, empty traversal, cursor/page-item identity
  after growth, natural/name/count sorting, ties, and reverse traversal.

The final slice 39 `cmake --workflow rt-default` run passed 1661 tests
with six skips and no failures.  The unsubscribed-list selector work was
left for slice 40.

Unsubscribed groups now use an index-based selector.  `O` chooses natural,
group-name, or article-count order; `R` reverses it.  `l glob` replaces the
source restriction and rescans local and NNTP sources.  Empty results
remain interactive, invalid globs retain the previous list, and leaving
the view does not subscribe to groups.  Existing outer restrictions and
the unread threshold are preserved.  Disabling the add selector retains
a plain listing.

Slice 40 test-first evidence, using `cmake --workflow temp` throughout:

- The initial 37-test run failed the new listing regression because raw
  output had no selector title or sort controls.
- Expanded coverage failed 19 of 53 tests before implementation.  The
  failures covered the listing, sorting, paging, glob rescans, membership,
  duplicate results, help, and leaving without subscriptions.
- After wiring the selector and initializing the terminal/search test
  fixture, four of 57 tests still failed: saved restriction lookup,
  alphabetic-key conflicts, initially empty results, and unread-threshold
  preservation.  Fixing those paths made the same 57 tests pass.

The final slice 40 `cmake --workflow rt-default` run passed 1682 tests
with six skips and no failures.  Issue #15 is implemented locally; its
closing commit must include `Fixes #15`.

## GitHub 5.0 Milestone

The GitHub 5.0 milestone is the mandatory release scope.  Every issue on
that milestone is a release gate.  Open issues must be fixed before the
release is cut, unless the issue is explicitly removed from the
milestone on GitHub.  Closed issues are mandatory regression checks.

Open mandatory milestone issues:

- #3: Add nntps/SSL support (implemented locally; ready to close).
- #5: Support decoding yEnc files (implemented locally; ready to close).
- #14: Make trn available as a Windows 32-bit/64-bit application.
- #16: Ensure posting works properly.
- #17: Restore original character encoding conversions (implemented
  locally; ready to close).
- #18: Update the INSTALL file.
- #19: All previous Configure script options should be CMake configure
  options.
- #23: Build on macOS.
- #24: `"buffer overflow detected"` when local newsgroup source is
  missing `Group Desc`.
- #25: Validate/support moderator patterns (implemented locally; ready to
  close).
- #29: Make Windows equivalents of helper scripts (slices 41 through 46).
- #31: All files previously generated by configure script should be
  generated by CMake.
- #33: Test plan.
- #34: Add a `"UTF-8 => ASCII"` code conversion (implemented locally;
  ready to close).
- #38: Diagnose `compress_name` disabled tests, re-enable and pass them.

Closed milestone issues remain mandatory release verification items.  The
release must not regress #2, #9, #11, #12, #20, #28, #32, #36, or #39.

## Priorities

### P0: Must Finish Before 5.0

P0 is the GitHub 5.0 milestone.  No open P0 issue may be deferred to 5.1
while it remains on that milestone.  Closed P0 issues must remain fixed.

1. Finish install docs: #18.
2. Finish CMake and generated-file migration: #19, #31.
3. Fix mandatory crash and disabled-test issues: #24, #38.
4. Ship Windows and macOS release support: #14, #23, #29.
5. Ship NNTP TLS/nntps and deterministic release tests: #3, #33.
6. Fix posting, moderator routing, and charset/decode work: #5, #16, #17,
   #25, #34.
7. Verify closed milestone work stays complete: #2, #9, #11, #12, #20,
   #28, #32, #36, #39.

### P1: Supporting Release Work

P1 work is not itself on the 5.0 milestone, but may be needed to finish
or stabilize P0 work.

1. Keep the threaded-article save crash report deferred unless a
   deterministic article and command repro is supplied: #10.
2. Replace remaining DOS-as-Windows platform decisions.
3. Modernize core NNTP command selection when needed by #3.
4. Add enough test automation to make #33 meaningful.

### P2: Non-Milestone Follow-Up

P2 work should not delay 5.0 unless it is moved onto the GitHub 5.0
milestone.

1. Use archive.org as additional test content: #35.

## Implementation Rules

Use these as review units.  Each numbered item owns at most one open
GitHub issue.  Closed milestone issues are not implementation items; they
are captured in the regression checklist.  A large open issue can span
several numbered items; close it only after its final acceptance line is
met.

Slice tasks are ordered requirements, not suggestions.  Do not implement
a later task until earlier reproduction, audit, probe, or verification
tasks have concrete evidence recorded in the work.  If the first task
cannot be completed with available local, CI, issue, or user-provided
evidence, stop and report the blocker; do not substitute source
inspection, plausible diagnosis, or a guessed fix.  Do not remove a slice
from this plan until every task and acceptance line has been satisfied by
that evidence.

Use strict test-driven development for all changes in this plan:

1. Write a test for the required change.  Run it before changing the
   implementation and verify that it fails for the intended reason.
2. Implement the change.
3. Rerun the previously failing test and verify that it now passes.

Repeat this sequence for every change.  Record the failing and passing
test commands and results as slice evidence.  Tests added only after the
implementation do not satisfy this requirement.  Do not remove a slice
until its test-first evidence and all acceptance checks are complete.

For each remaining configuration-variable slice, use standard CMake
feature discovery first: probe the build or target system when CMake can
determine the answer, and make generated details non-cache variables.
Keep a cache variable only for a real user override, install-time path,
or release policy that cannot be discovered reliably.

Use `config/legacy/Configure` to understand historical defaults, special
values, and compatibility expectations.  Do not copy its Q&A structure
when CMake can determine the result directly.

When a slice is the final active slice for an open GitHub issue, its
commit description must include `Fixes #nnn`.  If a slice can either fix
or defer an issue, use `Fixes` only when the issue is actually resolved.

## Implementation Slices

### 41. Remove Unused filexp Helper

Issues: #29.

Milestone status: mandatory 5.0 work.

Files likely touched:

- `config/data/filexp.in` and `config/data/filexp.cmd.in`
- CMake generation, exported helper names, and install/package rules
- generated-config and install/package tests
- active helper and path-expansion documentation

Tasks:

- Add failing tests requiring clean generation, installation, and package
  output to omit `filexp` and `filexp.cmd` on Unix and Windows.
- Remove both templates and their active CMake generation, exported
  helper names, installation, and packaging references.  Update tests
  that currently require the helper or its unsupported Windows stub.
- Remove active documentation that advertises the command.  Keep legacy
  build references as historical material; do not port or reactivate them.
- Preserve trn's separate C++ `file_exp` path expansion and its regression
  coverage.  This slice removes only the unused external command.

Acceptance:

- The previously failing tests pass on native Windows and Unix.
- Clean generated, installed, and packaged artifacts contain neither
  `filexp` nor `filexp.cmd`; no active workflow requires the command.
- Existing C++ path expansion and the retained helper workflows continue
  to pass their regression checks.

### 42. Windows norm.saver Script

Issues: #29.

Milestone status: mandatory 5.0 work.

Files likely touched:

- `config/data/norm.saver.in` as the Unix behavior reference
- `config/data/norm.saver.cmd.in` and a new PowerShell template
- CMake generation, defaults, and install rules
- saver invocation tests and documentation

Tasks:

- Add failing tests for the seven-argument contract, offset numbering,
  full-article markers, body-only saves, append behavior, and separators.
  Cover empty input, paths with spaces, and input/output failures.
- Implement PowerShell/.NET stream I/O with explicit byte offsets and
  matching output.  Do not decode/re-encode copied article bytes; cover
  all byte values and preserve existing destinations on open failures.
- Wire Windows `NORMSAVER` and ordinary saves to the launcher/script;
  replace its diagnostic stub and update install/package tests and help.
  Retain the Unix script and verify its output against common fixtures.

Acceptance:

- The same tests pass after implementation on Windows and Unix.
- Saving from trn appends the expected article or body without `tail`
  or shell redirection; failed saves return an error.
- Installed/package invocation exercises the real save path.

### 43. Windows mbox.saver Script

Issues: #29.

Milestone status: mandatory 5.0 work.

Depends on: 42 for shared saver argument and append behavior.

Files likely touched:

- `config/data/mbox.saver.in` as the Unix behavior reference
- `config/data/mbox.saver.cmd.in` and a new PowerShell template
- CMake generation, defaults, and install rules
- mailbox-save tests and documentation

Tasks:

- Add failing tests for the eight-argument contract, supplied envelope,
  article markers, offsets, `From `/`from ` escaping, blank separators,
  and the configured control-A mailbox format.
- Implement PowerShell mailbox serialization with explicit byte/stream
  handling.  Cover repeated saves, all byte values, empty bodies, and
  existing mailbox contents without unintended encoding changes.
- Wire Windows `MBOXSAVER`, author-copy, and mail records to the
  launcher/script; replace its diagnostic stub and update packaging and
  documentation.  Retain the Unix script and common-format fixtures.

Acceptance:

- Previously failing mailbox fixtures pass on Windows and Unix.
- Installed helper output matches the selected mailbox format and keeps
  multiple appended articles distinct without `sed`, `tail`, or a shell.
- Read/write failures propagate without reporting a successful save.

### 44. Windows Speller Script

Issues: #29.

Milestone status: mandatory 5.0 work.

Depends on: 42 for Windows script invocation conventions.

Files likely touched:

- `config/data/Speller.in` as the Unix behavior reference
- `config/data/Speller.cmd.in` and a new PowerShell template
- shared PowerShell support and CMake configuration
- spell-check workflow tests, install rules, and documentation

Tasks:

- Add failing tests for multiple input files, editable Subject/Keywords/
  Summary headers and continuations, body text, `QUOTECHARS`, signatures,
  and preservation of other headers and quoted text.
- Implement splitting, configured native checker invocation, and
  reassembly in PowerShell.  Replace `awk`, `diff`/`ed`, and related
  pipelines with script logic; test quoted-text reinsertion after edits.
  Cover interactive correction and word-list/pager modes with fake tools,
  including tool failure and cancellation.
- Preserve the original message on failed checks, clean temporary files,
  and wire native tool settings, installed helper paths, and callers.
- Replace the Windows diagnostic stub, retain the Unix script, and update
  user and package documentation.

Acceptance:

- The same workflow tests pass on Windows and Unix after implementation.
- A configured native checker can correct a draft while protected text
  remains intact; missing/failed tools leave the original unchanged.
- The installed Windows helper works without `awk`, `sed`, `diff`, `ed`,
  or `pr`; the existing Unix workflow remains covered.

### 45. Windows Pnews Script

Issues: #29.

Milestone status: mandatory 5.0 work.

Depends on: 42 through 44.

Files likely touched:

- Unix `config/data/Pnews.in` and `Pnews.header.in` as references
- `config/data/Pnews.cmd.in` and a new PowerShell template
- shared PowerShell support and helper/default configuration
- posting tests, install/package checks, and `man/Pnews.1`

Tasks:

- Add failing tests for interactive, newsgroup/title, and
  `-h headerfile [oldarticle]` modes, header generation, distribution
  prompts, inclusion, signatures, and the edit/check/post/abort loop.
- Implement the PowerShell composer, incorporating `Pnews.header`
  behavior.  Honor editor, pager, checker, fast-mode, author-copy, and
  optional PGP settings; retain User-Agent and MIME handling.
- Exercise `trn-artchk`, `nntplist`, and `inews` integration with native
  fake tools and posting capture.  Default to the installed C++ `inews -h`
  with the completed article on standard input.  Preserve drafts after
  failed posting and cancellation, and propagate child failures.
- Defer mail copies and moderated-email submission to slice 46.  First
  add failing tests requiring a clear failure and a recoverable draft
  when a requested email workflow has no backend, then implement that
  handling without blocking ordinary NNTP posting.
- Replace the Windows diagnostic stub and `NEWSPOSTER` default; update
  this helper's install/package rules and documentation.  Retain Unix
  `Pnews` and `Pnews.header`.

Acceptance:

- The same previously failing posting tests pass on Windows and Unix.
- Installed Windows `Pnews` composes and submits through native `inews`
  without sourcing Unix `Pnews.header`.
- NNTP posting works independently of the unresolved email backend.
  Email-dependent workflows retain drafts until slice 46 completes them.

### 46. Windows Rnmail and Email Submission

Issues: #29.

Milestone status: mandatory 5.0 work; final slice for #29.

Depends on: 42 through 45.

Files likely touched:

- `config/data/Rnmail.in` as the Unix behavior reference
- `config/data/Rnmail.cmd.in` and a provisional PowerShell template
- shared script support, or C++ `Rnmail` sources if the probe justifies it
- helper/default configuration
- selected submission-backend integration and dependency/package rules
- reply/forward tests, install rules, and `man/Rnmail.1`
- `Pnews` email integration, posting tests, and `man/Pnews.1`

Tasks:

- First define the Windows Email Submission contract above, compare
  backend candidates, and record a native Windows integration probe for
  the selected sender/client.  Cover full-message and recipient handling,
  account setup, acceptance, cancellation, and failure.  Resolve any
  content or dependency gap before implementing the composer port.
- Confirm the script choice using that evidence.  If a C++ `Rnmail` is
  justified instead, revise this slice and replace the Unix script too.
- Add failing tests for interactive and recipient-list modes,
  `-h headerfile [oldart]`, reply/forward inclusion, signatures, draft
  recovery, and the edit/check/send/abort loop.
- For the script route, implement the PowerShell composer and native tool
  invocation.  Honor `EDITOR`/`VISUAL`, `RNMAILER`, `PAGER`, mail
  signature, record, and fast-mode settings; test optional PGP invocation.
- Exercise accepted, queued, handed-off, cancelled, and failed outcomes
  as supported by the selected backend.  Use a capture backend for
  deterministic tests and the real adapter with a controlled integration
  endpoint/client; retain drafts for unaccepted or unconfirmed outcomes.
- Replace the Windows diagnostic stub and wire `MAILPOSTER`, forwarding,
  mail records, and moderator submission to installed Windows commands.
  Retain Unix `Rnmail` for the script route; a C++ route replaces it too.
  Update platform-specific documentation.
- Add failing tests for `Pnews` mail copies and moderated submissions,
  then integrate the checked backend.  Retain full article headers/body
  and apply the same submission, handoff, and draft-recovery status rules.
- Update install/package rules and documentation for the five retained
  helpers.  Run Windows and Unix installed-package workflows for save,
  mailbox save, spell-check, mail reply/forward, and posting using the
  appropriate Windows PowerShell and Unix shell helpers.

Acceptance:

- Previously failing compose/reply/forward tests pass on Windows and
  Unix with native fake tools and no external mail delivery.
- The installed Windows helper submits complete replies and MIME forwards
  through the selected supported backend.  Native integration evidence
  covers recipient/header handling, Unicode, configuration, and failures;
  a successful fake-mailer exit alone does not satisfy this acceptance.
- Draft handoff is distinct from accepted submission/queueing; failures,
  cancellation, and unconfirmed handoff retain a recoverable draft.
- Backend dependencies and account configuration are packaged or
  documented and exercised; no `/bin/mail` equivalent is assumed.
- Quoted executable paths and filenames work without `/bin/mail` or a
  Unix shell dependency on Windows.
- `Pnews` mail copies and moderated submissions use the checked native
  email backend and preserve drafts for unaccepted outcomes.
- The five retained commands from #29 perform their documented workflows
  on native Windows without Unix utilities or unsupported stubs;
  install/package tests verify helper selection and usable defaults.
- Windows packages contain working launchers and scripts for all five
  helpers unless a slice justified a C++ replacement on both platforms.
  Unix packages retain shell helpers and `Pnews.header` for script ports.
- `filexp` is absent from both platforms' packages, as required by slice
  41.
- Commit description includes `Fixes #29` only after slices 41 through 46
  and their acceptance checks are complete.

### 47. Archive.org Test Samples

Issues: #35.

Milestone status: not on the 5.0 milestone.

Depends on: 23 through 25.

Files likely touched:

- test data
- test documentation

Tasks:

- Use archive.org content only after deterministic fixtures exist.
- Import only small, license-acceptable, pinned test samples.
- Keep downloaded or derived samples reproducible.

Acceptance:

- Archive.org content is optional and not required for default CI.
- Any imported sample has source, license, and reproduction notes.
- Imported samples are exercised by optional unit or regression tests
  with no network dependency.
- Commit description includes `fixes #35`.

## Deferred Slices

### Deferred. Python NNTP Fixture Server

Issues: #33.

Milestone status: deferred until a checked release test needs a
data-backed NNTP server or runtime CTest orchestration that the preferred
C++ gtest mock cannot cover directly.

Plan source:

- `nntp-test-server.md`

Files likely touched:

- `tests/support/nntp_test_server.py`
- `tests/support/run_nntp_fixture.py`
- `tests/CMakeLists.txt`
- test data files

Tasks:

- Add a Python fixture server with JSON config and port-file output.
- Add a wrapper that starts the server, writes runtime access files or
  environment, runs the child test command, and stops the server.
- Cover data-backed `LIST`, `GROUP`, `LISTGROUP`, article retrieval,
  overview, headers, posting capture, and dot-stuffing.

Acceptance:

- The Python fixture is introduced only with a consumer test that needs
  data-backed server behavior or runtime CTest orchestration.
- The fixture needs no external server and is deterministic under
  parallel test execution.
- Server config parsing, port-file behavior, and unsupported-command
  handling are tested.

## Deferred Cleanup

The function-to-member refactors in `StructureFunctions.md` are not 5.0
release gates.  Take them only when they simplify an active numbered
item.

Full SASL support is not a 5.0 gate.  30 should leave a small hook for it
if practical.

Broad replacement of every POSIX file operation with `std::filesystem`
is not a 5.0 gate.  Use it where active numbered items already touch the
code.

The universal selector push/pop cleanup is not a 5.0 gate unless it is
needed by 40.

The full `ArticlePresentationView`, `PresentationBuffer`, and
`DisplayView` architecture from `charset-handling.md` is not a 5.0 gate
unless it is the smallest correct implementation for 31 through 35.

## Issue Map

Mandatory open GitHub 5.0 milestone issues:

- #3: completed by removed slice 30 and earlier NNTP slices.
- #5: completed by removed slice 36; the closing commit must include
  `Fixes #5`.
- #14: completed by removed slice 20.
- #16: completed by removed slices 15 and 16.
- #17: completed by removed slices 31 through 33; the closing commit must
  include `fixes #17`.
- #18: completed by removed slices 3 and 7.
- #19: completed by removed slices 5 and 6; the closing commit must
  include `fixes #19`.
- #23: completed by removed slices 21 and 22.
- #24: 11 and 12.
- #25: completed by removed slice 38; the closing commit must include
  `Fixes #25`.
- #29: 41 through 46; the final closing commit must include `Fixes #29`.
- #31: completed by removed slices 8 through 10.
- #33: see `docs/release-5-test-plan.md`.
- #34: completed by removed slices 34 and 35; the closing commit must
  include `Fixes #34`.
- #38: 13.

Mandatory closed GitHub 5.0 milestone regression checklist:

- #2: UTF-8 support from acli remains covered.
- #9: overview retrieval performance does not regress.
- #11: packaging status remains visible in the canonical readme.
- #12: the canonical readme preserves the completed update.
- #20: posted articles still include a User-Agent header.
- #28: BSD configure behavior remains feature-test driven.
- #32: direct SSL/nntps behavior remains covered.
- #36: FreeBSD CI support remains present.
- #39: intermittent interpolator crashes remain fixed.

Non-milestone supporting or follow-up issues:

- #10: deferred pending a deterministic article and command repro from the
  reporter's spool state.
- #15: completed by removed slices 39 and 40; the closing commit must
  include `Fixes #15`.
- #30: completed by removed slice 37; the closing commit must include
  `Fixes #30`.
- #35: 47.

## Release Gate

Release 5.0 can be cut when:

- every open GitHub issue on the 5.0 milestone is closed;
- every closed GitHub issue on the 5.0 milestone remains fixed;
- any issue still open on the 5.0 milestone has first been moved off
  that milestone in GitHub;
- supported CI is green;
- install/package artifacts contain the generated helpers;
- all five retained #29 helpers work on native Windows, including
  installed-package save, spell-check, reply/forward, and posting tests;
- `filexp` is absent from generated, installed, and packaged artifacts;
- install/package artifacts contain required runtime libraries;
- non-markdown user documentation is synchronized with shipped
  functionality or explicitly marked historical;
- non-milestone P1/P2 work is either complete, documented as deferred,
  or tracked for a later release.
