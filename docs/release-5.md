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
runtime shell templates that need a Windows batch, PowerShell, or native
replacement strategy are:

- `config/data/Pnews.in`: interactive news poster; sources
  `Pnews.header`, calls `Speller`, `mbox.saver`, `trn-artchk`,
  `nntplist`, `inews`, the editor, the pager, and optional PGP tools.
- `config/data/Pnews.header.in`: shell scriptlet sourced by `Pnews`;
  include it in the `Pnews` replacement rather than treating it as a
  standalone command.
- `config/data/Rnmail.in`: reply and forward mailer; generated by CMake,
  and needs a Windows strategy with `Pnews`.
- `config/data/Speller.in`: spell-check wrapper using `awk`, `sed`,
  `diff`, `ed`, `spell` or `ispell`, `pr`, a pager, and temp files.
- `config/data/filexp.in`: tilde and install-prefix path expansion using
  `sed`, `expr`, `/etc/passwd`, and optional `/bin/csh` globbing.
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
  migration.  Item 19 owns that remaining drift.
- `man/trn.1` and `config/data/environment.in` still document Unix shell
  defaults such as `Rnmail`, `Pnews`, `tail`, and `/bin/sh`.  Item 19
  owns the Windows-safe default policy and documentation fallout.
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
4. Ship Windows and macOS release support: #14, #23.
5. Ship NNTP TLS/nntps and deterministic release tests: #3, #33.
6. Fix posting and mandatory charset/decode work: #5, #16, #17, #34.
7. Verify closed milestone work stays complete: #2, #9, #11, #12, #20,
   #28, #32, #36, #39.

### P1: Supporting Release Work

P1 work is not itself on the 5.0 milestone, but may be needed to finish
or stabilize P0 work.

1. Keep the threaded-article save crash report deferred unless a
   deterministic article and command repro is supplied: #10.
2. Replace remaining DOS-as-Windows platform decisions.
4. Complete Windows helper scripts when needed by Windows packaging: #29.
5. Modernize core NNTP command selection when needed by #3.
6. Add enough test automation to make #33 meaningful.

### P2: Non-Milestone Follow-Up

P2 work should not delay 5.0 unless it is moved onto the GitHub 5.0
milestone.

1. Add moderators support: #25.
2. Fix the UTF-8 underline repro if it is not covered by #17 or #34:
   #30.
3. Refactor AddGroup storage and sort unsubscribed-group listings: #15.
4. Use archive.org as additional test content: #35.

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

### 37. UTF-8 Underline Display

Issues: #30.

Milestone status: not on the 5.0 milestone.

Depends on: 31 through 35.

Files likely touched:

- terminal display code
- display tests

Tasks:

- Add a regression using the attached UTF-8 underline message.
- Ensure decoded header display cannot leave terminal attributes active.
- Defer if 31 through 35 do not naturally touch the display path.

Acceptance:

- The underline repro no longer leaves terminal state dirty, or #30 is
  explicitly deferred outside 5.0.
- Display unit tests cover the attached UTF-8 underline repro when the
  fix is implemented.
- If this slice fixes #30 instead of deferring it, commit description
  includes `fixes #30`.

### 38. Moderators Support

Issues: #25.

Milestone status: not on the 5.0 milestone.

Depends on: 26 and 27.

Files likely touched:

- `libtrn/DataSourceConfig.*`
- `libtrn/datasrc.cpp`
- posting path
- access templates
- tests

Tasks:

- Add `Moderators File` to the access schema.
- Support local INN-style moderator pattern files.
- Support remote `LIST MODERATORS` when advertised.
- Use wildmat matching for group-to-moderator resolution.

Acceptance:

- Posting to moderated groups uses configured moderator routing.
- This work is deferred unless it joins the 5.0 milestone.
- Moderator unit tests cover local pattern matching, remote
  `LIST MODERATORS`, and fallback behavior when implemented.
- If this slice implements #25 instead of deferring it, commit
  description includes `fixes #25`.

### 39. AddGroup Storage

Issues: #15.

Milestone status: not on the 5.0 milestone.

Plan source: `ToDo.md`.

Files likely touched:

- `libtrn/addng.cpp`
- `libtrn/include/trn/addng.h`
- tests

Tasks:

- Introduce `AddGroupIndex` for selector cursors and page state.
- Store `AddGroup` objects in `std::vector<AddGroup>`.
- Replace linked-list traversal with range helpers.
- Replace temporary home-grown de-duplication storage.

Acceptance:

- AddGroup storage no longer leaks discarded duplicate nodes.
- Object identity is separate from sorted traversal.
- AddGroup unit tests cover duplicate disposal, cursor identity, and
  sorted traversal.

### 40. Selector Sorting

Issues: #15.

Milestone status: not on the 5.0 milestone.

Depends on: 39.

Files likely touched:

- `libtrn/rt-page.cpp`
- `libtrn/rt-select.cpp`
- `libtrn/ngsrch.cpp`
- tests

Tasks:

- Make selector paging index-based.
- Add a sortable selector view for unsubscribed newsgroup results.
- Let users change sort order and refine the source glob.

Acceptance:

- Selector paging is index-based.
- Unsubscribed newsgroup listings can be sorted.
- Selector unit tests cover paging, sort order, reverse order, and glob
  refinement.
- Commit description includes `fixes #15`.

### 41. Archive.org Test Samples

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
- #15: 39 and 40.
- #25: 38.
- #29: 19.
- #30: 37.
- #35: 41.

## Release Gate

Release 5.0 can be cut when:

- every open GitHub issue on the 5.0 milestone is closed;
- every closed GitHub issue on the 5.0 milestone remains fixed;
- any issue still open on the 5.0 milestone has first been moved off
  that milestone in GitHub;
- supported CI is green;
- install/package artifacts contain the generated helpers;
- install/package artifacts contain required runtime libraries;
- non-markdown user documentation is synchronized with shipped
  functionality or explicitly marked historical;
- non-milestone P1/P2 work is either complete, documented as deferred,
  or tracked for a later release.
