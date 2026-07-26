<!-- Copyright (c) 2026, Richard Thomson -->

# C String Audit

## Scope

Audited project C and C++ sources under the source root, excluding the
vendored `vcpkg` tree.  The audit looked for local raw C string pointers
and function parameters that can become `std::string_view` or
`std::string` without changing ownership boundaries.

Follow-up passes also look for fixed-length `char name[N]` buffers in
all storage classes and for functions that hide owned string allocation
behind a raw `char *` return.  They also look for obsolete C-style
overloads and wrappers left behind after migration to `std::string` or
`std::string_view`.

## Audit Criteria

Each rerun should check these scenarios:

Run every scan from the innermost lexical scope outward:

1. Local variables.
2. Function parameters.
3. Function return values.
4. Struct/class/union members.
5. Static file-scope variables.
6. Global variables.

- `char *` values that can become `const char *` because the local code
  never writes through the pointer.
- `char *` and `const char *` struct/class/union members.  Classify the
  member as owned, borrowed, interior, output, pooled, or static/global
  storage before deciding whether it is a string candidate.
- `const char *` values that can become `std::string_view` because the
  local code only reads, slices, compares, or forwards text by extent.
- C-style overloads, wrappers, and helpers that take or return
  `char *` or `const char *` when a `std::string` or
  `std::string_view` API already exists nearby.  Check whether
  production code calls them.  If only tests call them, decide whether
  the tests preserve a real public API or only stale compatibility.
- Owned `char *` storage that can become `std::string` storage without
  pointer escape.
- `char *` results from functions that return owned raw strings from
  allocation helpers.  Summarize the return ownership, then trace callers
  that store, use, and free the result locally.
- Arrays of `T` where `T` is not `char` and the array is resized with
  `safe_realloc`.  Classify element ownership, then convert the owning
  array storage to `std::vector<T>` when the array is local to one
  owner.
- Fixed-length `char name[N]` buffers in local, static local,
  file-scope, global, and struct/class storage.  Do not limit the scan to
  local automatic variables.
- Filename variables that compose, normalize, query, remove, rename, or
  create paths and can become `std::filesystem::path`.
- C string library calls.  Treat `strcpy`, `strncpy`, `strcat`,
  `strncat`, `strcmp`, `strncmp`, `strchr`, `strrchr`, `strstr`,
  `strlen`, `strspn`, `strcspn`, `strpbrk`, `strtok`, `sprintf`,
  `snprintf`, `sscanf`, `vsprintf`, `vsnprintf`, `fgets`, `gets`,
  `fputs`, `puts`, `printf`, and `fprintf` as audit roots.
- C byte library calls on character storage.  Treat `memcpy`, `memmove`,
  `memset`, `memcmp`, and `memchr` as audit roots when the destination
  or compared data is string-like `char` storage.  Non-string table
  clearing or raw byte protocol work is a non-string buffer finding.
- C numeric conversion calls that consume C strings.  Treat `atoi`,
  `atol`, `atof`, `strtol`, `strtoul`, and `strtod` as audit roots
  when the source text is already a `std::string`,
  `std::string_view`, or sliced C string.

When `next` finds no remaining slices, rerun the audit against the
current source and look for new opportunities before treating the plan
as empty.

On every audit rerun, start from the current source and re-evaluate every
prior concern that still exists in the tree.  Do not assume older
judgments remain valid after code changes.

Do not keep a chronological history of audit reruns in this document.
Record durable rules, current findings, and open slices.  When a slice is
completed, remove it from the slice list instead of moving it into
`Findings`.

When removing a completed slice, also remove that slice ID from the
`Dependencies` lists of all remaining slices.  Dependency lists should
name only remaining blockers.  If a completed slice was part of a range,
rewrite the range or list so it only names incomplete slices.

When removing a completed slice empties a dependency tier, rerun the
full audit across all audit criteria against the current source.  Do not
only look for new slices in the tier that was emptied.  Prioritize any
new slices by dependency tier before continuing with the next tier.

Do not self-defer findings.  If source still contains matching raw
string ownership, a fixed buffer, a raw return, or path storage, keep the
candidate visible as a slice until it is completed or the user explicitly
says to defer or remove it.

When updating the C string function inventory, keep only nonzero counts
in the bullet list.  Move every zero-count function name into the
`The scan found no current production hits` sentence below the bullets.

Existing good precedents:

- `libtrn/univ.cpp`, `univ_add_text_file`: accepts a legacy C string at
  the boundary, then uses `std::string_view` for slicing and
  `std::string` for owned path assembly.
- `util/util2.cpp`, `file_exp`: accepts a `std::string_view`, uses
  `std::string_view` slices and `std::find_if` scans for prefix
  parsing, and returns an owned `std::string`.
- `libtrn/terminal.cpp`, `set_macro`: accepts string views and creates
  owned strings only when a null-terminated value is needed.
- `libtrn/autosub.cpp`, `match_list`: passes comma-delimited pattern
  views to `newsgroup_comp` without per-token allocation.
- `libtrn/datasrc.h`, `DataSource`: runtime text fields use
  `std::string` with empty string as the missing-value sentinel.

## Findings

Most raw string pointers are not local cleanup targets yet.  They are
owned buffers, caller-owned mutable buffers, struct fields, termcap and
NNTP API boundaries, or cursor outputs such as `char **`.  Examples are
`g_buf`, `Article` and `Subject` fields, `HashDatum` payloads,
`parse_string`, and `push_string`.
`CompiledRegex::m_exp_buf` and `m_alternatives` are regex bytecode and
internal cursors, not ordinary string storage.

The useful local targets fall into five groups:

- Read-only labels selected from string literals, then printed.
- Bounded tokens cut out of a larger C string.
- Read-only pointer plus length pairs where the length is the text
  extent, not an output limit.
- Temporary strings that must be null-terminated before calling legacy
  regex, hash, file, or shell helpers.
- Pointers returned by legacy helpers that are immediately copied into
  `std::string` globals or fields.
- Owned raw-string results that callers store in local `char *`
  variables and later free.
- Borrowed static-buffer return helpers that format text into shared
  storage and return a pointer to that storage.

Use `std::string_view` only while no callee needs ownership or a
guaranteed terminator.  When a sliced token flows to a C API, build a
local `std::string` and pass `c_str()`.  If a string literal assignment
warning can be fixed by making the target `const char *` instead of
`char *`, prefer that simpler const-correct fix.

Reject any change that lets the address of local string storage escape
the function.  This includes:

- Output parameters.
- Assignment to global variables.
- Assignment to file-scope static variables.
- Passing to any function that stores the pointer in static or global
  storage.

## Finding Types

### `char *` to `const char *`

Select when a pointer is assigned string literals or borrowed read-only
storage, the local code never writes through it, and no callee requires a
mutable pointer.  Include struct/class members when every use treats the
member as borrowed read-only storage.

Refactor by changing the local declaration, parameter, or return type to
`const char *`.  Prefer this over `std::string_view` when the value is a
literal-only selection, a null sentinel, or a legacy C API input such as
`perror`.  For struct/class members, treat the change as a
storage-centered slice and update all direct readers and writers.

### `const char *` to `std::string_view`

Select when code only reads, slices, compares, scans, or forwards text by
extent.  Reject null-sentinel APIs unless the slice also removes or
replaces the sentinel intentionally.

Refactor by changing the function signature or local variable to
`std::string_view`, then use `empty`, `front`, `remove_prefix`, `substr`,
and direct comparison.  Build a local `std::string` only when a callee
needs a null terminator, and pass `c_str()` for `std::string` values.

### Unused C-style Overloads And Wrappers

Select when a `char *` or `const char *` overload, wrapper, or helper is
unused by production code, or when every production caller can already
use a nearby `std::string` or `std::string_view` API.  Include helpers
that only delegate to modern APIs and legacy overloads used only by
tests.

Refactor by deleting unused helpers and overloads.  If a C-style wrapper
is still needed for a real C boundary, command-line entry point, external
library callback, or active migration path, keep it visible as a wrapper
slice and migrate its callers bottom-up.  Do not keep an overload solely
because a test still calls it; update or remove stale compatibility
tests when the production API no longer exists.

### Owned `char *` Storage To `std::string`

Select when a raw pointer owns retained text, the same owner frees or
overwrites it, and callers only need read-only C-string access or local
mutable parsing.  Reject memory-pool strings and `char **` output
allocation APIs until that lifetime model changes.  Include struct/class
members that are assigned from owning allocation helpers or an owning
raw-string return and destroyed by the same owner.

Refactor by replacing the owning `char *` with `std::string` or
`std::optional<std::string>` when null and empty are distinct.  Replace
allocation, copy, and matching `free` paths with direct string
assignment.  Use `c_str()` for legacy read-only APIs and `data()` only
for local mutable parsing with no pointer escape.

Favor an empty `std::string` sentinel over `std::optional<std::string>`
when an empty string has no valid meaning for the result.  This is the
preferred shape for runtime storage.  Use
`std::optional<std::string>` only when empty string is valid payload that
must be distinguished from absence, or at parse/config boundaries where
the code needs to preserve whether a field was present.

Do not introduce `std::optional<std::string>` merely because the current
C-string code compares a pointer to `nullptr`.  First decide whether the
empty string has a valid meaning.  If it does not, use plain
`std::string` and replace `nullptr` checks with `empty()` checks.

### Owning Raw-string Returns

Select when a function returns a `char *` that is owned by the caller.
Examples include direct returns from allocation helpers or a helper
already classified as returning owned raw string storage.  Record
whether the function always returns owned storage, conditionally returns
owned storage, returns pooled storage, returns borrowed/static storage,
or mixes ownership modes.

Refactor bottom-up.  For callers with local acquire/use/free flow,
replace the local `char *` with `std::string` and remove the `free`.
When a function always returns owned text, add or migrate to an owning
`std::string` return.  For mixed APIs such as copy/no-copy flags, split
the API or add a clearly named owning-string helper before changing
callers.  Reject slices where the returned pointer escapes, is stored in
global/static storage, or is passed to a function that stores it.

### Owning Raw-string Parameters

Select when a function parameter is documented or implemented as
caller-allocated owned text, especially when the callee frees it after
copying or parsing.  Treat mixed APIs as higher-risk: if some cases
borrow and some cases consume ownership, make the signature explicit
rather than preserving the hidden ownership branch.

Refactor by changing the parameter to `std::string_view` when the callee
only reads or copies the text.  If the callee needs ownership, take
`std::string`.  Update callers in the same slice so they do not allocate
just to satisfy the old parameter contract.

### Borrowed Static-buffer Returns

Select when a function formats text into a static, file-scope, or global
buffer and returns a pointer to that buffer.  These are not owning raw
returns, but they still hide string data behind shared storage.

Refactor by returning `std::string` for formatted text or by writing to a
caller-provided output abstraction when the caller truly controls
storage.  Convert immediate display callers to consume the string in the
same expression or same local scope.

### `safe_realloc` Arrays

Select when code owns a growable array of `T` where `T` is not `char`,
the array storage is resized with `safe_realloc`, and the array lifetime
has a clear owner.  Include `char **` arrays because the element type is
`char *`, not `char`.  Reject byte buffers, caller output storage, and
arrays whose ownership or element lifetime is split across unrelated
owners.

Refactor by replacing the owning `T *` plus count and capacity fields
with `std::vector<T>`.  When a slice promotes a member of a
`safe_realloc`-grown struct array to a non-trivial C++ type, migrate the
owning array to `std::vector` in the same slice.

### Fixed-length C Buffers

Select when a `char name[N]` buffer is only owned string storage,
formatted text, token storage, command text, or a cache that can be
represented by `std::string`.  Scan automatic locals, static locals,
file-scope statics, globals, extern arrays, and struct/class fields.
Reject caller output buffers, parser compaction, protocol byte buffers,
returned static storage, and fixed-width display or file-format fields.

Refactor owned text to `std::string` and formatted text to
`fmt::format`.  Local automatic buffers can often be one-function
slices.  Static, file-scope, global, and struct buffers are
storage-centered slices and must update all direct readers and writers.
When a fixed-size buffer is replaced by a string built with appends,
reserve the previous fixed buffer size before appending.
Before editing, classify truncation.  Preserve meaningful truncation;
remove arbitrary fixed-buffer truncation when ordinary behavior remains
covered.

### Buffer Plus Size

Select `std::string_view` when a pointer plus size is a read-only text
extent.  Select a byte container or span-shaped interface instead for
mutable buffers, transport bytes, or output capacity.

Refactor by passing the view through the call chain and using
`data()`/`size()` only for callees that consume the data immediately.
Do not treat an output limit as a string extent.

### Copy and Concat

Select owned local construction chains such as `strcpy` plus `strcat`,
`sprintf` append chains, or `std::string` append chains that are already
building one owned string.  Reject writes into caller buffers, globals,
static return buffers, parser workspaces, and protocol buffers.

Refactor plain ownership changes to `std::string`; refactor formatted
construction to `fmt::format` or `fmt::format_to`.  Defer C-buffer
`sprintf` sites until the target buffer itself is converted.

### Filename Variables

Select `std::filesystem::path` when a function composes, normalizes,
queries, creates, removes, renames, or creates parents for a real
filesystem path and the path object does not escape.

Refactor by adding `namespace fs = std::filesystem;` in that source file
and using `fs::path` and `fs::` operations.  Do not convert one-shot
`fopen` or `freopen` filenames if the result is only
`path.string().c_str()`.

### Formatted Output

Select `fmt` when a function formats owned text or writes formatted
output directly to `stdout` or `stderr`.  Reject runtime printf-style
format strings until the format string source is audited.

Refactor to `fmt::format`, `fmt::format_to`, or `fmt::print`, and link
`fmt::fmt` privately to the target.  Do not add `/utf-8`; the fmt overlay
port controls that behavior.

### C String Library Calls

Select every C string function call as an audit root, even when no raw
`char *` declaration is nearby.  Classify by what the call proves about
the storage:

- `strcpy`, `strncpy`, `strcat`, `strncat`, `sprintf`, `snprintf`,
  `vsprintf`, and `vsnprintf`: construction into a C string buffer.
  Prefer `std::string` or `fmt` when the destination is owned local
  storage.  Move with the owning storage when the destination is global,
  static, struct storage, or caller output.
- `strcmp` and `strncmp`: comparison of null-terminated text.  Prefer
  direct `std::string` or `std::string_view` comparison when the inputs
  are already strings or can be viewed by extent.
- `strchr`, `strrchr`, `strstr`, `strlen`, `strspn`, `strcspn`,
  `strpbrk`, and `strtok`: parsing, slicing, or length discovery.
  Prefer `std::string_view` operations when the code does not need to
  mutate the source.  Preserve mutable parsing only when the function
  intentionally edits a caller buffer.
- `sscanf`: parsing from null-terminated text.  Prefer
  `std::string_view` tokenization and `std::from_chars` when the source
  is already a view or owned string.
- `fgets` and `gets`: fixed-size line input.  Prefer `std::string` line
  input when truncation is arbitrary.  Keep fixed protocol buffers only
  when the size is meaningful.
- `fputs`, `puts`, `printf`, and `fprintf`: C string output or
  printf-format output.  Prefer `fmt::print` for formatted output and
  keep `fputs` only when plain C-string output is simpler and does not
  hide string construction.
- `memcpy`, `memmove`, `memset`, `memcmp`, and `memchr`: classify only
  when applied to character storage.  Convert string-like character
  storage with its owner; ignore non-string table initialization and
  binary buffers.

### C Numeric Conversion Calls

Select when code parses numbers by passing `c_str()`, `data() + n`, or a
temporary string to `std::atoi`, `std::atol`, or another C numeric
conversion.  This is especially useful when the source text is already a
`std::string_view` or a full `std::string`.

Refactor to `std::from_chars` over the known string extent.  Preserve
documented or tested leading-space behavior explicitly when the old
`atoi`/`atol` call relied on it.  Keep pointer-based conversions inside
mutable parser buffers until the owning parser slice converts the
buffer.

## General Refactoring Rules

Do not promote simple output-only helper parameters when the only local
effect is replacing `fputs` or `printf` with `fwrite` or a temporary
`std::string`.  Keep the C-string signature when it is simpler and no
string slicing or ownership improvement results.

Classify copy/concat hits by destination.  Owned local construction can
become `std::string` or `fmt::format`.  Caller output buffers, parser
compaction, global display buffers, protocol buffers, static returned
storage, and struct-owned C buffers are not local string/fmt slices.

Do not convert a filename to `std::filesystem::path` when the only
result is calling `path.string().c_str()` for a single C API.  Keep
protocol strings, shell commands, macro templates, URL text, NNTP names,
and environment interpolation text as strings until a function has
separated out a real filesystem path.

When a source file uses `std::filesystem`, add a file-scope namespace
alias `namespace fs = std::filesystem;` and qualify filesystem types and
functions with `fs::`, for example `fs::path` and `fs::remove`.

Do not introduce a local variable for a value used only once.  Do not
hoist a conversion into a local variable only because it appears in
mutually exclusive branches; each execution path still uses it once.
Do not introduce a local `std::string` only to hold `path.string()`.
Use `path.string().c_str()` at the call site and let the compiler handle
common subexpression elimination.

Keep call sites simple.  If a function argument can be implicitly
converted to the parameter type, pass the argument directly instead of
explicitly constructing a temporary of that parameter type.

When a full `std::string` is converted to `std::string_view`, construct
the view directly from the string.  Do not write
`std::string_view{text.data(), text.size()}` or
`std::string_view{text.c_str(), text.size()}` unless the view is
intentionally a substring or a non-string extent.  Simplify full-string
cases as soon as they are encountered; do not wait for a separate slice.

Do not convert `std::string_view` to `fmt::string_view` just to call a
fmt API that already accepts `std::string_view`.  Pass the
`std::string_view` directly and remove any helper lambda whose only
purpose was the conversion.

When a one-line lambda has degenerated into delegating to one function
call, inline the delegated call at the use site.  Keep a small lambda
only when it is used repeatedly and its name captures a local parsing
rule or lifetime contract.

Prefer `operator=` over `std::string::assign` when the right-hand side is
already a suitable string value or string view and the assignment
preserves the same extent.  Use `assign` only when its explicit range,
count, or iterator overload is doing real work.

When replacing a nullable C-string result with an owned `std::string`,
map `nullptr` failure to an empty string.  Callers that need to branch on
failure must check `empty()` instead of comparing to `nullptr`.  Pass
`c_str()` to legacy C APIs only when the pointer is consumed during the
same full expression; otherwise keep an owned `std::string` in scope.

Do not add an unused wrapper or bridge function.  Convert at least one
caller in the same slice, or keep the raw acquire/copy/free flow local
to the function being refactored.

Do not hide work in a self-chosen deferral list.  Record candidates as
slices even when they need owner judgment.  Add a deferred item only when
the user explicitly says that exact item should be deferred.

For owned raw-return helpers, prefer changing the producer to return
`std::string` and updating all direct callers in the same slice.  Do not
add caller-only copies or wrapper APIs when the producer can construct
the owned string directly.

When converting an owned global or file-scope `char *` to `std::string`,
replace allocation/copy storage updates with direct string assignment.
Do not allocate first and then assign to a string.  Use `c_str()` for
legacy read-only C APIs.  Use `data()` only for local mutable parsing
while the `std::string` object remains alive and no pointer escapes.

Before refactoring a slice, check whether tests cover the behavior being
changed.  If coverage is missing, first add tests for the current
behavior and run those newly added tests before changing the production
code.  Then refactor and rerun the tests to verify the behavior is
unchanged.

When a candidate buffer currently truncates text, classify the
truncation before editing.  Meaningful truncation must remain part of
the slice.  Arbitrary fixed-buffer truncation can be removed as part of
the string refactor when the ordinary behavior remains covered.

When fmt is available, use `fmt::format` for owned formatted strings,
`fmt::format_to` for appending formatted text to an existing string, and
`fmt::print` for formatted output to `stdout` or `stderr`.  Link
`fmt::fmt` privately to the target whose source file uses it.  Do not
add `/utf-8` just to satisfy fmt; the fmt overlay port exports
`FMT_UNICODE=0` and omits fmt's `/utf-8` interface option when the
manifest disables fmt's default `utf8` feature.  Do not use fmt for
simple path joining, plain string append, parser cursor work, or
caller-owned output buffers.  Leave runtime printf-style format strings
alone until the format string itself is audited; use `fmt::runtime` only
when keeping runtime formatting is intentional.  Do not create fmt
string-building slices for C-buffer `sprintf` sites; convert those when
the C-style string buffer itself is refactored.

## Current Audit Summary

The current scan covers source code under `config`, `libtrn`, `util`,
`nntp`, `inews`, `nntplist`, `trn-artchk`, `tool`, `wildmat`,
`parsedate`, and `main.cpp`.  It does not include tests, generated
files, legacy Configure scripts, or the vendored `vcpkg` tree.

- `safe_malloc` and `safe_realloc`: no remaining string-shaped owner is
  tracked here.  Non-string owners include AddGroup scratch storage,
  hash-table internals, regex bytecode, and generic allocation helpers.
  No string-copy allocation helpers are present in production roots.
- Direct environment C-string reads remain only inside the environment
  wrapper implementation.
- Fixed raw buffers: the current string-shaped fixed-buffer candidate is
  `g_buf`.  Tiny UTF byte scratch buffers, translation tables, terminal
  pushback bytes, termcap storage, keymap type bytes, and regex bytecode
  arrays are non-string protocol or parser storage.
- The legacy C-buffer `do_interp`, `interp`, `interp_search`,
  `interp_backslash`, `normalize_refs`, and raw-buffer `nntp_gets`
  overloads are gone.
- Article input scan: all production callers outside `artio.cpp` use the
  string-reading API.  The raw `read_art(char *, int)` helper is already
  private to `artio.cpp`; it remains only inside the string reader and
  article-buffer fill code.
- Unused overload/wrapper scan: `finish_command(int)` still has
  production callers that read or store command text through `g_buf`.
  Keep `nntp_init_error`, `string_case_compare`,
  `string_case_equal`, `Tgetstr`, `line_ptr`, `line_offset`,
  `yes_or_no`, `empty`, `plural`, `force_me`, and `at_grey_space`;
  they still have production/source callers or platform/API boundary
  use.
- Search API scan: article and newsgroup search now have
  `std::string_view` command APIs with raw-buffer compatibility wrappers
  for remaining callers.  Several callers now build local `std::string`
  commands and then create writable buffers only to call those APIs.
- Literal-only local pointer scan found local message selections in
  `do_newsgroup`, `s_search`, and `sa_refresh_bot`.
- C numeric conversion scan found `std::atoi` and `std::atol` calls that
  still convert strings or views back to C pointers.  Simple bounded
  parse sites are Tier 0 or Tier 1 slices; mutable parser-buffer sites
  are grouped with their owning parser slices.
- MIME content-decoding paths now own local string storage for decoded
  lines.  Remaining MIME work is in parser helpers that still expose
  mutable pointers.
- NNTP response parsing no longer uses `sscanf`.  The shared
  `g_ser_line` status owner is now `std::string`; remaining NNTP line
  storage is protocol/body input rather than status-text storage.
- Article display/copy paths now use `std::string` for the shared
  article line.  The low-level `read_art(char *, int)` API remains for
  protocol/body buffers and local fixed-size output loops.
- No active `strcpy`, `strncpy`, `strcmp`, `std::sprintf`, or `gets`
  production hits remain in this scan.
- The two `strstr` hits are in an inactive `#ifdef UNDEF` block in
  `sacmd.cpp`; they remain in the lexical inventory but are not active
  slices.
- Filename storage already uses modern path or view signatures for most
  owners.  Other filename strings are already `std::string`/`fs::path`
  values or cross C `FILE*` APIs.

## Current C String Function Inventory

The current scan covers the source roots listed above.  Counts below
are lexical, identifier-aware source counts for `std::` calls and
unqualified C calls.  The scan excludes test trees, legacy Configure
scripts, and `vcpkg`, but it does not preprocess conditional blocks.

- Search and length: `strchr` 23, `strstr` 2, `strlen` 19.
- C line input: `fgets` 4.
- C text output: `fputs` 164, `printf`/`std::printf` 313,
  `fprintf`/`std::fprintf` 13.
- Character output: `putchar`/`std::putchar` 86.
- Character byte operations: `memcpy` 1, `memset` 4, `memcmp` 1.
- C numeric conversion calls: `std::atoi` 29 and `std::atol` 19.

The scan found no current production hits for `strcpy`, `strncpy`,
`strcat`, `strncat`, `strcmp`, `strncmp`, `strrchr`, `strspn`,
`strcspn`, `strpbrk`, `strtok`, `sprintf`, `snprintf`, `sscanf`,
`vsprintf`, `vsnprintf`, `gets`, `puts`, `memmove`, `memchr`,
`std::atof`, `std::strtol`, `std::strtoul`, or `std::strtod`.

`fmt::sprintf` appears three times.  These calls are not C buffer
writes.  They are tracked only where the format template itself should
be modernized.

## Refactoring Slices

Slices are stable.  Do not renumber remaining slices when one is
completed; remove the completed slice.  The physical order is grouped by
dependency tier: finish earlier tiers first so later caller and
shared-buffer slices have cleaner helper and ownership contracts to
build on.

Global command buffer work must be split by function.  Prefer leaves
that only use the first command character before command loops that pass
the full buffer into downstream dispatch.  Treat terminal command input
and global scratch-buffer storage as criteria, not implementation
slices.

The terminal command-input slices remove direct `get_cmd(g_buf)`
callers.  Do not replace a read with `get_cmd()` plus an immediate copy
back to `g_buf` unless the copy is temporary scaffolding for a following
slice.  When the called command dispatcher still reads `g_buf`, first
move that dispatcher to accept command text or a command character.

The global scratch-buffer slices remove other uses of `g_buf` as scratch
storage.  Do not replace `g_buf` with one global `std::string`.  Move
storage to the owning function, parser, or data object.  Re-evaluate
every slice after each scan; old deferrals are not binding.

### Tier 0 - Leaf Cleanup

These slices have no slice dependency.  They remove local C string
construction, comparison, or display roots without changing a larger
owner.

#### CSTR-309 - Selector Search Error Message View

- Files: `libtrn/scmd.cpp`.
- Kind: literal-only local pointer cleanup.
- Function: `s_search`.
- Dependencies: none.
- Change: replace `error_msg` with `std::string_view` and use `fmt` for
  the touched no-match output.
- Tests: selector search no-match tests.

#### CSTR-310 - Score Display Order View

- Files: `libtrn/sadisp.cpp`.
- Kind: literal-only local pointer cleanup.
- Function: `sa_refresh_bot`.
- Dependencies: none.
- Change: replace `order` with `std::string_view` and use `fmt` for the
  touched order output.
- Tests: score display refresh tests.

#### CSTR-311 - Newsgroup Unread Prompt Views

- Files: `libtrn/ng.cpp`.
- Kind: literal-only local pointer cleanup.
- Function: `do_newsgroup`.
- Dependencies: none.
- Change: replace `u_prompt` and `u_help_thread` with
  `std::string_view`, use `empty()` for thread-help presence, and use
  `fmt` for the touched help output.
- Tests: article-mode unread and newsgroup unread prompt tests.

#### CSTR-312 - Article Check Column Parse

- Files: `trn-artchk/trn-artchk.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `main`.
- Dependencies: none.
- Change: parse `argv[2]` through a bounded view and `std::from_chars`
  instead of `std::atoi`.
- Tests: trn-artchk command-line validation tests.

#### CSTR-313 - Article Check Server Port Parse

- Files: `trn-artchk/trn-artchk.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `main`.
- Dependencies: none.
- Change: parse the port view after `;` or `:` with
  `std::from_chars`; use the already built `server_name`/string storage
  instead of making a C-string pointer.
- Tests: trn-artchk server configuration tests.

#### CSTR-314 - Inews Server Port Parse

- Files: `inews/inews.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `main`.
- Dependencies: none.
- Change: parse the port view after `;` with `std::from_chars` instead
  of `std::atoi(g_server_name.c_str() + separator + 1)`.
- Tests: inews server configuration tests.

#### CSTR-315 - NNTP List Server Port Parse

- Files: `nntplist/nntplist.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `main`.
- Dependencies: none.
- Change: parse the port view after `;` or `:` with
  `std::from_chars` instead of
  `std::atoi(s_server_name.c_str() + separator + 1)`.
- Tests: nntplist server configuration tests.

#### CSTR-316 - Environment Net Speed Parse

- Files: `util/env.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `env_init`.
- Dependencies: none.
- Change: parse `NETSPEED` with `std::from_chars` over
  `net_speed` instead of `std::atoi(net_speed.c_str())`, preserving the
  `f` and `s` shortcuts.
- Tests: environment configuration tests.

#### CSTR-317 - INI Condition Numeric Parse

- Files: `libtrn/util.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `check_ini_cond`.
- Dependencies: none.
- Change: parse numeric operands from `condition_text` and
  `cond_cursor` with `std::from_chars`; remove the temporary string made
  only for `std::atoi`.
- Tests: INI condition tests.

#### CSTR-318 - Local Active Times Parse

- Files: `libtrn/addng.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `new_local_groups`.
- Dependencies: none.
- Change: parse the timestamp from
  `active_times_view.substr(name_end + 1)` with `std::from_chars`
  instead of `std::atol(active_times_line.c_str() + name_end + 1)`.
- Tests: local newsgroup discovery tests.

#### CSTR-319 - Killfile Thread Age Parse

- Files: `libtrn/kfile.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `kill_file_init`.
- Dependencies: none.
- Change: parse the thread command age from `command.substr(1)` with
  `std::from_chars`; remove `command.data() + 1` and `std::atol`.
- Tests: kill-file thread command aging tests.

#### CSTR-320 - Killfile THRU Article Parse

- Files: `libtrn/kfile.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `do_kill_file`.
- Dependencies: none.
- Change: parse the THRU article number from a bounded view after the rc
  name with `std::from_chars`; remove pointer-offset `std::atol`.
- Tests: kill-file THRU tests.

#### CSTR-321 - Cache Header Counts Parse

- Files: `libtrn/cache.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `Article::set_cached_line`.
- Dependencies: none.
- Change: parse line and byte counts directly from the `line` view with
  `std::from_chars`; remove `std::string{line}.c_str()` temporaries.
- Tests: article cache header tests.

#### CSTR-322 - URL Port Parse

- Files: `libtrn/url.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `parse_url`.
- Dependencies: none.
- Change: parse the port from `rest.substr(0, port_len)` with
  `std::from_chars`; remove the single-use `std::string port`.
- Tests: URL parsing tests.

#### CSTR-323 - INI Group Index Parse

- Files: `libtrn/rcstuff.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `parse_newsrc_ini`.
- Dependencies: none.
- Change: parse `section_name.substr(6)` with `std::from_chars` instead
  of constructing a temporary string for `std::atoi`.
- Tests: newsrc INI group parsing tests.

#### CSTR-324 - Newsrc Lock PID Parse

- Files: `libtrn/rcstuff.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `set_lock`.
- Dependencies: none.
- Change: parse `pid_line` with `std::from_chars` instead of
  `std::atol(pid_line.c_str())`.
- Tests: newsrc lock-file tests.

#### CSTR-325 - Relocation Command Number Parse

- Files: `libtrn/rcstuff.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `relocate_newsgroup`.
- Dependencies: none.
- Change: parse `full_command` with `std::from_chars` instead of
  `std::atol(full_command.c_str())`.
- Tests: newsgroup relocation command tests.

#### CSTR-326 - Overview Article Number Parse

- Files: `libtrn/rt-ov.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `ov_data`.
- Dependencies: none.
- Change: parse the overview article number from the line view with
  `std::from_chars` instead of `std::atol(line.c_str())`.
- Tests: overview data parsing tests.

#### CSTR-327 - Decode Piece Total Parse

- Files: `libtrn/decode.cpp`.
- Kind: C numeric conversion cleanup.
- Function: `decode_piece`.
- Dependencies: none.
- Change: parse `total_line` with `std::from_chars` instead of
  `std::atoi(total_line.c_str())`; keep the existing non-negative
  clamp.
- Tests: split decode piece tests.

### Tier 1 - Helper And API Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

#### CSTR-328 - MIME Content Type View Parser

- Files: `libtrn/mime.cpp`.
- Kind: local C-string parser cleanup.
- Function: `MimeSection::mime_parse_type`.
- Dependencies: none.
- Change: keep the parsed MIME type as a `std::string_view` while
  checking prefixes and suffixes, and parse `number` and `total`
  parameters with `std::from_chars`.
- Tests: MIME content-type and partial-message tests.

#### CSTR-329 - Header Custom Line View Scan

- Files: `libtrn/head.cpp`.
- Kind: C string library call cleanup.
- Function: `get_header_num`.
- Dependencies: none.
- Change: scan `g_head_buf` as string-view lines and remove temporary
  NUL mutation, `strchr`, and `strlen`.
- Tests: custom header detection tests.

#### CSTR-330 - Score Extra Header View Scan

- Files: `libtrn/scorefile.cpp`.
- Kind: C string library call cleanup.
- Function: `sf_get_extra_header`.
- Dependencies: none.
- Change: iterate `g_head_buf` as string-view lines and use
  `find`/`substr` instead of `const char *` cursors and `strchr`.
- Tests: score extra-header matching tests.

#### CSTR-331 - Remote XHDR Line View Parser

- Files: `libtrn/head.cpp`.
- Kind: local C-string parser cleanup.
- Function: `prefetch_remote_lines`.
- Dependencies: none.
- Change: parse NNTP XHDR response lines with views, remove CR
  trimming by NUL mutation, and parse the leading article number with
  `std::from_chars`.
- Tests: remote header prefetch tests.

#### CSTR-332 - Global Option Value View Parser

- Files: `libtrn/opt.cpp`.
- Kind: local C-string parser cleanup.
- Function: `apply_global_option`.
- Dependencies: none.
- Change: remove the local `value_text.c_str()` staging variable where
  callees already accept views or strings, and replace numeric
  `std::atoi` uses with bounded parsing.
- Tests: option parsing tests.

### Tier 2 - Tool-local And Owner-local Storage

These slices replace one parser or local owner of string storage.  Finish
them before broad global-buffer work and before removing helpers.

#### CSTR-333 - RC Numbers To Bits View Parser

- Files: `libtrn/bits.cpp`.
- Kind: owner-local parser cleanup.
- Function: `rc_to_bits`.
- Dependencies: none.
- Change: parse newsrc number ranges with `std::string_view` and
  `std::from_chars`; remove comma-to-NUL mutation, `strchr`, and
  `std::atol` from the local copy.
- Tests: rc-to-bits and range parsing tests.

#### CSTR-334 - Add Read Article View Parser

- Files: `libtrn/rcln.cpp`.
- Kind: owner-local parser cleanup.
- Function: `add_art_num`.
- Dependencies: none.
- Change: parse the rc-number list with views and `std::from_chars`
  while keeping the existing rc-line rewrite semantics.
- Tests: mark-read and xref-chase tests.

#### CSTR-335 - Remove Read Article View Parser

- Files: `libtrn/rcln.cpp`.
- Kind: owner-local parser cleanup.
- Function: `sub_art_num`.
- Dependencies: none.
- Change: parse the rc-number list with views and `std::from_chars`
  while keeping the existing unread and range-splitting semantics.
- Tests: unmark-read and xref-chase tests.

#### CSTR-336 - Expired Article Range View Rewrite

- Files: `libtrn/rcln.cpp`.
- Kind: owner-local parser cleanup.
- Function: `NewsgroupData::check_expired`.
- Dependencies: none.
- Change: parse and rewrite remaining rc ranges with bounded views
  instead of `char *` cursors, `strchr`, `strlen`, and `std::atol`.
- Tests: expired-article rc-line rewrite tests.

#### CSTR-337 - Newsgroup Read-state View Query

- Files: `libtrn/rcln.cpp`.
- Kind: owner-local parser cleanup.
- Function: `was_read_group`.
- Dependencies: none.
- Change: query rc ranges with `std::string_view` and
  `std::from_chars` instead of borrowed C-string cursors and
  `std::atol`.
- Tests: read-state query and xref-chase tests.

#### CSTR-338 - Score Save Line View Parser

- Files: `libtrn/scoresave.cpp`.
- Kind: owner-local parser cleanup.
- Function: `sc_sv_use_line`.
- Dependencies: none.
- Change: parse score-save command runs with string-view cursors and
  `std::from_chars`; remove temporary NUL mutation and `std::atoi`.
- Tests: score-save load tests.

### Tier 3 - Workflow Callers And Path Owners

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

#### CSTR-306 - Article Search Callers

- Files: `libtrn/ng.cpp`, `libtrn/kfile.cpp`, `libtrn/rt-select.cpp`.
- Kind: shared command/search buffer caller.
- Function: article search command callers.
- Dependencies: none.
- Change: update direct callers to pass strings or views to
  `art_search`.  Remove local writable staging buffers and the
  `stage_legacy_article_command` path when each caller no longer needs
  `g_buf` to fake an article search command.  Delete the temporary raw
  `art_search` compatibility wrapper after callers have moved.
- Tests: article search, kill-file command, and article-mode command
  tests.

#### CSTR-307 - Newsgroup Search Callers

- Files: `libtrn/trn.cpp`, `libtrn/rt-select.cpp`.
- Kind: shared command/search buffer caller.
- Function: newsgroup search command callers.
- Dependencies: none.
- Change: update direct callers to pass strings or views to
  `newsgroup_search`.  Remove the local `trn.cpp` writable staging
  wrapper and any selector staging that only existed for the old mutable
  search API.  Delete the temporary raw `newsgroup_search`
  compatibility wrapper after callers have moved.
- Tests: newsgroup search, selector search, and top-level newsgroup
  command tests.

#### CSTR-298 - Command Dispatch Scratch Buffer

- Files: `libtrn/ngstuff.cpp`, `libtrn/kfile.cpp`,
  `libtrn/rt-select.cpp`, `libtrn/score.cpp`, `libtrn/scorefile.cpp`.
- Kind: shared command scratch buffer.
- Function: `perform`, kill-file command dispatch, and score command
  dispatch.
- Dependencies: CSTR-306, CSTR-307.
- Change: stop copying command text into `g_buf` for dispatch.  Pass
  owned strings or string views through the call chain and keep any
  fallback copy local to the function being migrated.
- Tests: perform command tests, kill-file command tests, and score
  command tests.

#### CSTR-299 - Response Command And File Scratch Buffer

- Files: `libtrn/respond.cpp`.
- Kind: shared command and file-copy buffer.
- Function: response command handling and temporary header/body copy
  paths.
- Dependencies: CSTR-298.
- Change: separate response command text from file-copy line storage.
  Use owned strings for command parsing and owner-specific buffers for
  file copy paths; do not store either in `g_buf`.
- Tests: response command tests and saved-header/body output tests.

### Tier 5 - Helper Removal

These slices remove helpers only after every direct caller has moved to
owned strings or owner-specific storage.

#### CSTR-301 - Remove Global `g_buf`

- Files: `config/common.cpp`, `config/include/config/common.h`, all
  remaining production users.
- Kind: final global storage removal.
- Function: `g_buf`.
- Dependencies: CSTR-298, CSTR-299, CSTR-306, CSTR-307.
- Change: delete the global command buffer after all remaining users own
  their storage locally.  Do not replace it with another global string.
- Tests: full build and full test workflow.
