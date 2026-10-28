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

## GitHub 5.0 Milestone

The GitHub 5.0 milestone is the mandatory release scope.  Every issue on
that milestone is a release gate.  Open issues must be fixed before the
release is cut, unless the issue is explicitly removed from the
milestone on GitHub.  Closed issues are mandatory regression checks.

Use the milestone for current issue status; do not duplicate open/closed
issue lists or completed-slice maps in this plan.

## Supporting Release Work

- Replace remaining DOS-as-Windows platform decisions when needed for
  supported Windows release builds.
- Add test automation needed for #33, following
  [the release test plan](release-5-test-plan.md).

## Documentation Verification

Markdown files are excluded from the non-markdown documentation audit.
Before release, verify these sources against shipped behavior:

- top-level text: `INIT.txt`, `HACKERSGUIDE.txt`, and `default_topic`;
- man pages: `man/trn.1`, `man/Pnews.1`, and `man/Rnmail.1`;
- online help files under `HelpFiles`;
- generated data and help text: `config/data/access_def.in`,
  `config/data/environment.in`, and `config/data/newsnews.in`;
- compiled help and option labels in `libtrn/help.cpp`,
  `libtrn/OptionCatalog.cpp`, and scoring help call sites.

Check platform-specific helper defaults, access-schema fields, option
names and values, selector/scoring help, and installed help lookup paths.
Retire obsolete instructions or mark them historical.

## Implementation Rules

Use these as review units.  Each numbered item owns at most one open
GitHub issue.  Closed milestone issues are not implementation items; they
remain release regression checks.  A large open issue can span several
numbered items; close it only after its final acceptance line is met.

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
test commands and results while implementing the slice.  Tests added only
after the implementation do not satisfy this requirement.  Remove a slice
when its test-first evidence and all acceptance checks are complete.
Keep completed-slice summaries, test logs, and historical status out of
this working plan.

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

## Deferred Slices

### Deferred. Python NNTP Fixture Server

Issues: #33.

Deferred until a checked release test needs a
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
release gates.  Take them only when needed to resolve a release gate.

The threaded-article save crash report #10 remains deferred until a
deterministic article and command repro is supplied.

Full SASL support is not a 5.0 gate.

Broad replacement of every POSIX file operation with `std::filesystem`
is not a 5.0 gate.  Use it where active release work already touches the
code.

The universal selector push/pop cleanup is not a 5.0 gate.

The full `ArticlePresentationView`, `PresentationBuffer`, and
`DisplayView` architecture from `charset-handling.md` is not a 5.0 gate.

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
- non-milestone supporting work is complete, documented as deferred,
  or tracked for a later release.
