<!-- Copyright (c) 2026, Richard Thomson -->

# do_interp Refactor

## Target Shape

The real interpolation implementation is this public API:

```cpp
std::string do_interp(
    std::string_view &pattern,
    std::string_view stoppers,
    std::string_view cmd);
```

Contract:

- `pattern` is the input cursor.
- The return value is the interpolated text.
- On return, `pattern` has been advanced to the stopper or to the end.
- The stopper is not consumed.
- Empty `stoppers` means interpolate to the end.
- Empty `cmd` means there is no search command context.

All other overloads delegate to this API or wrap it for legacy callers.
There is no hidden `do_interp_core` and no result structure.

Convenience overloads:

```cpp
std::string do_interp(std::string_view pattern);
std::string interp_search(std::string_view pattern, std::string_view cmd);
```

Legacy wrappers:

```cpp
const char *do_interp(
    char *dest,
    int dest_size,
    const char *pattern,
    const char *stoppers,
    const char *cmd);
const char *interp(char *dest, int dest_size, const char *pattern);
const char *interp_search(
    char *dest,
    int dest_size,
    const char *pattern,
    const char *cmd);
```

The legacy wrappers copy the string result into the caller-owned buffer
and return a pointer into the original pattern.

## Refactoring Rules

- Add or extend tests before changing behavior covered by a slice.
- Newly added tests must pass against the current implementation before
  refactoring.
- Do not preserve the `std::size_t result_size` overload in the final
  string API.  It only exists because the current implementation writes
  into fixed-size output buffers.
- Keep bounded output and overflow handling inside the legacy C-buffer
  wrapper only.
- Do not let the address of local string storage escape through output
  parameters, global variables, static variables, or functions that cache
  those addresses.
- Keep current interpolation semantics unless the behavior is explicitly
  changed by user decision.
- Keep `std::string_view` cursors tied to caller-owned input.  Never
  return a view into a local temporary.

## Current C API Roles

The current C function does three jobs:

- Parses the interpolation pattern.
- Writes interpolated text into a caller-owned fixed-size buffer.
- Returns a pointer to the unconsumed pattern text.

After the refactor, parsing and output construction belong to the string
API.  The C overload only adapts legacy caller-owned buffers.

Current stopper-aware callers:

- `libtrn/terminal.cpp`, `mac_line`: splits macro sequence from macro
  definition at space or tab.
- `libtrn/util.cpp`, condition parsing: stops at comparison tokens.
- Internal conditional, prompt, and backtick parsing in `intrp.cpp`.

Current size-limited string callers:

- Response header builders pass `5 * LINE_BUF_LEN`.
- Those calls should become ordinary `do_interp(pattern)` calls after
  the string implementation is unbounded.

## Implementation Slices

Slices are stable.  Do not renumber remaining slices when one is
completed; remove the completed slice from this file.

### Tier 0 - Coverage

These slices add or tighten tests before the parser implementation moves.

### Tier 1 - Public API Foundation

These slices change the owning parser API.  Complete them before moving
callers off the C wrappers.

#### DINT-012-033 - Followup Target Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-013`.
- Case: `%F`.
- Change: keep fetched Followup-To or Newsgroups header text in owning
  string storage and expose it through the local value view.
- Preserve: Followup-To preference.
- Tests: focused interpolation tests.

#### DINT-012-034 - Message-id Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-013`.
- Case: `%i`.
- Change: keep fetched or synthesized Message-ID text in string storage
  and expose it through the local value view.
- Preserve: angle-bracket insertion.
- Tests: focused interpolation tests.

#### DINT-012-035 - Organization Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-011`.
- Case: `%o`.
- Change: keep organization text in string storage, including file
  contents when the organization value names a file.
- Preserve: `NEWSORG`, `ORGANIZATION`, `IGNORE_ORG`, and file fallback
  behavior.
- Tests: focused interpolation tests.

#### DINT-012-036 - References Tail Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-013`.
- Case: `%r`.
- Change: normalize fetched References text in owning string storage and
  expose only the last reference as a string view.
- Preserve: empty result when no reference is found.
- Tests: focused interpolation tests.

#### DINT-012-037 - References Header Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-036`.
- Case: `%R`.
- Change: normalize and build References text in owning string storage.
- Preserve: root/prior reference trimming and Message-ID append behavior.
- Tests: focused interpolation tests.

#### DINT-012-038 - Subject Cases

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-013`.
- Cases: `%s`, `%S`.
- Change: derive the subject string view from owning subject storage
  without pointer walking.
- Preserve: `Re:` removal for `%s` and the old `- (nf` truncation.
- Tests: focused interpolation tests.

#### DINT-012-039 - Author Cases

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-032`.
- Cases: `%t`, `%T`.
- Change: keep selected author source text in owning string storage and
  expose it through the local value view before address parsing.
- Preserve: Reply-To preference, Path substitution for `%T`, and host
  prefix trimming.
- Tests: focused interpolation tests.

#### DINT-012-040 - Short From Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-032`.
- Case: `%y`.
- Change: replace pointer-walking shortening with string position logic
  and expose the result through the local value view.
- Preserve: current star-shortening behavior.
- Tests: focused interpolation tests.

#### DINT-012-041 - Article Size Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-001`.
- Case: `%z`.
- Change: build the formatted article size in local `std::string`
  storage.
- Preserve: empty result outside a newsgroup.
- Tests: focused interpolation tests.

#### DINT-012-050 - Conditional Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-010` through `DINT-012-041`.
- Case: `%(...?...:...)`.
- Change: make condition interpolation use the reference-cursor string
  API and string/string-view values internally.
- Preserve: stopper position, regex matching against current output,
  false-branch skipping, and nested interpolation.
- Tests: focused interpolation tests.

#### DINT-012-060 - Default Literal Case

- Files: `libtrn/intrp.cpp`.
- Kind: switch-arm value conversion.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-001`.
- Case: unknown `%` escape.
- Change: append the literal escaped character through the same local
  string/string-view value path used by converted cases.
- Preserve: `metabit` handling.
- Tests: focused interpolation tests.

#### DINT-012-070 - Remove Case-local C String Result

- Files: `libtrn/intrp.cpp`.
- Kind: parser cleanup.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-010` through `DINT-012-060`.
- Change: remove the case-local `const char *s` result variable after all
  switch arms produce string/string-view values.
- Keep: C output buffer and pointer cursor as temporary outer parser
  scaffolding.
- Tests: focused interpolation tests.

#### DINT-012-080 - Move Outer Parser To Reference Cursor

- Files: `libtrn/intrp.cpp`.
- Kind: implementation replacement.
- Function: `do_interp(std::string_view &, std::string_view,
  std::string_view)`.
- Depends on: `DINT-012-070`.
- Change: move the parser body to the reference-cursor string overload.
- Replace: C output-buffer writes with local `std::string`
  construction.
- Replace: pointer cursor mutation with `std::string_view::remove_prefix`
  and view slicing.
- Replace: `std::strchr(stoppers, ch)` with `stoppers.find(ch)`.
- Preserve: stopper position, nested interpolation, `%?` line splitting,
  modifiers, formatting, shell command interpolation, prompted input, and
  current error paths.
- Tests: focused interpolation tests.

#### DINT-013 - Make C Buffer API A Wrapper

- Files: `libtrn/intrp.cpp`.
- Kind: compatibility wrapper.
- Function: legacy C `do_interp`.
- Depends on: `DINT-012-080`.
- Change: replace the old C implementation with a wrapper that creates a
  view cursor, calls the reference-cursor string API, copies the result
  into `dest`, preserves legacy overflow handling, and returns the cursor
  as a pointer into the original pattern.
- Keep: `interp` and C `interp_search` delegating through the legacy C
  `do_interp` wrapper.
- Tests: focused interpolation tests.

#### DINT-014 - Delegate String Overloads To New API

- Files: `libtrn/intrp.cpp`, `libtrn/include/trn/intrp.h`.
- Kind: overload cleanup.
- Functions: string `do_interp`, string `interp_search`.
- Depends on: `DINT-013`.
- Change: remove `interp_to_string` and make every string overload
  delegate to the reference-cursor `do_interp`.
- Change: make string `interp_search` accept `std::string_view cmd`.
- Keep: a temporary `const char *cmd` adapter only if needed for callers.
- Tests: focused interpolation tests.

### Tier 2 - Remove Obsolete Size API

These slices remove the fixed-buffer size from string call sites after
the string implementation is unbounded.

#### DINT-020 - Response Header String Callers

- Files: `libtrn/respond.cpp`.
- Kind: obsolete size argument removal.
- Functions: cancel, supersede, mail, forward, and news header builders.
- Depends on: `DINT-014`.
- Change: replace `do_interp(pattern, 5 * LINE_BUF_LEN)` with
  `do_interp(pattern)`.
- Tests: response/interpolation tests that cover generated headers.

#### DINT-021 - Remove size_t String Overload

- Files: `libtrn/include/trn/intrp.h`, `libtrn/intrp.cpp`,
  `tests/test_interp.cpp`.
- Kind: API removal.
- Function: `do_interp(std::string_view, std::size_t)`.
- Depends on: `DINT-020`.
- Change: delete the obsolete overload and update tests that were only
  proving the old fixed-buffer API shape.
- Tests: focused interpolation tests and header standalone test.

### Tier 3 - Stopper-aware Caller Migration

These slices move local callers that want the updated cursor directly to
the reference-cursor string API.

#### DINT-030 - Terminal Macro Parser

- Files: `libtrn/terminal.cpp`.
- Kind: stopper-aware caller migration.
- Function: `mac_line`.
- Depends on: `DINT-014`.
- Change: pass a `std::string_view` cursor to the new API with
  stoppers `" \t"`, then pass the returned string as the macro sequence
  and the remaining cursor, after horizontal-space skipping, as the macro
  definition.
- Tests: terminal macro tests.

#### DINT-031 - Utility Conditional Parser

- Files: `libtrn/util.cpp`.
- Kind: stopper-aware caller migration.
- Function: condition parsing around the `do_interp` stopper call.
- Depends on: `DINT-014`.
- Change: replace the caller-owned `g_buf` interpolation output with the
  string return value and use the updated view cursor for the condition
  remainder.
- Tests: focused tests for conditional parsing if available; otherwise
  add coverage before refactoring.

### Tier 4 - Legacy C API Reduction

These slices migrate every remaining direct caller of the C buffer API to
the string API.  They are ordered after the implementation slices because
each caller should delegate to the new string implementation, not to a
fresh wrapper around the old parser.

#### DINT-040 - Article First-line Interpolation

- Files: `libtrn/art.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `do_article`.
- Depends on: `DINT-014`.
- Change: replace the `g_art_line` interpolation destination for
  `g_first_line` with a local `std::string` and pass that text to
  `tree_puts`.
- Tests: existing article display coverage if present; otherwise add
  focused coverage first if the behavior is easy to isolate.

#### DINT-041 - Article Search Pattern Interpolation

- Files: `libtrn/artsrch.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `art_search`.
- Depends on: `DINT-014`.
- Change: replace the two `pat_buf` interpolation writes for `%\s` and
  `%\>f` with string API results, preserving the existing search pattern
  text.
- Tests: article search tests covering subject and author pattern
  construction if available; otherwise add focused coverage first if the
  behavior is easy to isolate.

#### DINT-042 - Cache Look-ahead Subject Interpolation

- Files: `libtrn/cache.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `look_ahead`.
- Depends on: `DINT-014`.
- Change: replace the `g_buf` interpolation write used to seed the
  look-ahead subject pattern with a local `std::string`.
- Tests: cache/look-ahead coverage if present; otherwise add focused
  coverage first if the behavior is easy to isolate.

#### DINT-043 - Option Startup Interpolation

- Files: `libtrn/opt.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `opt_init`.
- Depends on: `DINT-014`.
- Change: replace the `tcbuf` interpolation of `GLOBAL_INIT` with a local
  `std::string` and pass that to option-file processing.
- Tests: option initialization coverage if present; otherwise add focused
  coverage first if the behavior is easy to isolate.

#### DINT-044 - Reply Body Introduction Interpolation

- Files: `libtrn/respond.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `reply`.
- Depends on: `DINT-014`.
- Change: replace the `g_buf` interpolation of `YOUSAID` with a local
  `std::string` and write it with fmt.
- Tests: reply/header tests if present; otherwise add focused coverage
  first if the behavior is easy to isolate.

#### DINT-045 - Forward Body Marker Interpolation

- Files: `libtrn/respond.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `forward`.
- Depends on: `DINT-014`.
- Change: replace the `g_buf` interpolations of `FORWARDMSG` and
  `FORWARDMSGEND` with local `std::string` values, preserving the empty
  string checks and MIME boundary behavior.
- Tests: forward-message tests if present; otherwise add focused coverage
  first if the behavior is easy to isolate.

#### DINT-046 - Followup Attribution Interpolation

- Files: `libtrn/respond.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `followup`.
- Depends on: `DINT-014`.
- Change: replace the `g_buf` interpolation of `ATTRIBUTION` with a local
  `std::string` and write it with fmt.
- Tests: followup/header tests if present; otherwise add focused coverage
  first if the behavior is easy to isolate.

#### DINT-047 - Selector Mail Prompt Interpolation

- Files: `libtrn/rt-select.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `sel_prompt`.
- Depends on: `DINT-014`.
- Change: replace the `g_buf` interpolation of `g_mail_call` with the
  string API and use the result directly in the prompt format.
- Tests: selector prompt coverage if present; otherwise add focused
  coverage first if the behavior is easy to isolate.

#### DINT-048 - Terminal Edit Search Interpolation

- Files: `libtrn/terminal.cpp`.
- Kind: legacy `interp_search` caller migration.
- Function: `edit_buf`.
- Depends on: `DINT-014`.
- Change: replace both buffer-writing `interp_search` calls with the
  string API, preserving the full-buffer replacement and in-place suffix
  insertion behavior.
- Tests: terminal editing tests if present; otherwise add focused coverage
  first if the behavior is easy to isolate.

#### DINT-049 - Terminal Push String Interpolation

- Files: `libtrn/terminal.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `push_string`.
- Depends on: `DINT-014`.
- Change: replace the fixed `PUSH_SIZE` string buffer and NUL trimming
  with the string API result, then iterate the resulting string in
  reverse.
- Tests: terminal push-string coverage if present; otherwise add focused
  coverage first if the behavior is easy to isolate.

#### DINT-050 - Shell Quotechars Interpolation

- Files: `libtrn/util.cpp`.
- Kind: legacy `interp` caller migration.
- Function: `do_shell`.
- Depends on: `DINT-014`.
- Change: replace the fixed `g_buf` interpolation of `%I` with the string
  API, preserve the existing removal of the trailing interpolated
  character, and set `QUOTECHARS` from the resulting string.
- Tests: shell environment setup tests if present; otherwise add focused
  coverage first if the behavior is easy to isolate.

#### DINT-051 - Legacy Interpolation Tests

- Files: `tests/test_interp.cpp`.
- Kind: legacy C test migration.
- Function: test helper around `do_interp`.
- Depends on: `DINT-030`, `DINT-031`, `DINT-040`, `DINT-041`,
  `DINT-042`, `DINT-043`, `DINT-044`, `DINT-045`, `DINT-046`,
  `DINT-047`, `DINT-048`, `DINT-049`, `DINT-050`.
- Change: migrate tests that call the C buffer API to the public string
  API, using a reference `std::string_view` cursor for stopper tests.
- Tests: focused interpolation tests.

#### DINT-099 - Remove Legacy C APIs

- Files: `libtrn/include/trn/intrp.h`, `libtrn/intrp.cpp`.
- Kind: API removal.
- Functions: C `do_interp`, C `interp`, C `interp_search`.
- Depends on: `DINT-021`, `DINT-030`, `DINT-031`, `DINT-051`.
- Change: delete the C buffer declarations and wrappers after no
  production or test callers remain.
- Tests: full build and focused interpolation tests.
