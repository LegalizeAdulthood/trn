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

### Tier 2 - Remove Obsolete Size API

These slices remove the fixed-buffer size from string call sites after
the string implementation is unbounded.

### Tier 3 - Stopper-aware Caller Migration

These slices move local callers that want the updated cursor directly to
the reference-cursor string API.

### Tier 4 - Legacy C API Reduction

These slices migrate every remaining direct caller of the C buffer API to
the string API.  They are ordered after the implementation slices because
each caller should delegate to the new string implementation, not to a
fresh wrapper around the old parser.

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
- Depends on: `DINT-030`, `DINT-031`, `DINT-040`, `DINT-047`,
  `DINT-048`, `DINT-049`, `DINT-050`.
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
