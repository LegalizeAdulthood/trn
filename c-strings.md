<!-- Copyright (c) 2026, Richard Thomson -->

# C String Audit

## Scope

Audited project C and C++ sources under the source root, including test
code, and excluding the vendored `vcpkg` tree.  Test code is held to the
same modernization standard as production code.  The audit looked for
local raw C string pointers and function parameters that can become
`std::string_view` or `std::string` without changing ownership
boundaries.

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
- Mutable-looking `char *` function parameters that are only read,
  parsed, sliced, compared, or forwarded to read-only callees.  Treat
  these as `std::string_view` candidates even when the parameter is the
  only string parameter and is not paired with a caller output buffer.
- Local `char *` cursor aliases into owned `std::string` storage.  Treat
  these as local string-processing candidates when the function walks the
  pointer, writes temporary NULs, or uses C string library calls after
  copying input into owned storage.
- `char *` and `const char *` struct/class/union members.  Classify the
  member as owned, borrowed, interior, output, pooled, or static/global
  storage before deciding whether it is a string candidate.
- `const char *` values that can become `std::string_view` because the
  local code only reads, slices, compares, or forwards text by extent.
  Include helper parameters that feed any non-zero C string or byte
  function family and then influence owned `std::string` storage, cursor
  slicing, parsing decisions, or downstream output.
- C-style overloads, wrappers, and helpers that take or return
  `char *` or `const char *` when a `std::string` or
  `std::string_view` API already exists nearby.  Check whether
  production code calls them.  If only tests call them, update the tests
  to the modern API unless the wrapper preserves a real public API or
  deliberate compatibility boundary.
- When a reusable helper already expresses the string operation, prefer
  adding `std::string_view` overloads to that helper before rewriting
  call sites.  Do not inline helper behavior with ad hoc local scans or
  loops just because a caller has moved from C strings to views.
- Exported and internal helpers that take caller-provided `char *`
  output buffers paired with `const char *` input and return a byte
  count or string length.  Treat these as candidates for a
  `std::string`-returning API unless callers truly require in-place
  decode into a protocol buffer.
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
  For every function family with a non-zero count, inspect representative
  hits until each storage/dataflow shape is classified as an active
  slice, an existing slice dependency, inactive preprocessor code, a
  platform/API boundary, or non-string byte/protocol storage.
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

For `printf`/`std::printf` conversion, create one slice per source file.
Do not split file-level printf conversion into one-call slices.  Convert
all remaining printf output in the named source file in that slice.  If a
source file is too broad or risky to convert as one slice, ask before
splitting it.
If several remaining source files have five or fewer printf calls each,
combine them into one reviewable slice instead of leaving one-line
changes in separate slices.

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

For every C string or byte function family with a non-zero count, trace
each distinct dataflow shape backward to the storage origin and forward
to how the result is used.  The audit result for that family must say
which hits become explicit slices and which are inactive code,
platform/API boundaries, terminal/protocol byte storage, non-string data,
or already covered by another open slice.  Do not leave a non-zero count
as an unexplained inventory item.
Inactive preprocessor blocks are still source.  Unless the user
explicitly exempts the file, fix or modernize inactive blocks in place
instead of deleting them merely to remove an audit hit.

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
  If the argument is a `const char *` parameter or local C-string pointer
  that is parsed, sliced, measured, copied into, assigned to, or used to
  size owned `std::string` storage, make it an explicit
  `std::string_view` slice.  Check whether callers are passing
  `std::string::c_str()` only to satisfy the old signature.
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

The current scan covers source and test code under `config`, `libtrn`,
`util`, `nntp`, `inews`, `nntplist`, `trn-artchk`, `tool`, `wildmat`,
`parsedate`, and `main.cpp`.  It excludes generated files, legacy
Configure scripts, and the vendored `vcpkg` tree.

- `safe_malloc` and `safe_realloc`: no remaining string-shaped owner is
  tracked here.  Non-string owners include hash-table internals, regex
  bytecode, and generic allocation helpers.  No string-copy allocation
  helpers are present in production roots.
- Direct environment C-string reads remain only inside the environment
  wrapper implementation.
- Fixed raw buffers: the current string-shaped fixed-buffer candidate is
  `g_buf` covered by `CSTR-486`.  The local `opt_init` `argv` shim in
  `tests/test_interp.cpp` remains an API-boundary fixture because
  `opt_init` still accepts `char *argv[]`.  Translation tables,
  terminal pushback bytes, termcap storage, keymap type bytes, regex
  bytecode arrays, bounded UTF byte sequences, and terminal-capability
  fixture storage are non-string protocol, parser, or global-boundary
  storage.
- Direct `assign(data(), size())` and whole-string
  `std::string_view{data(), size()}` scans have no remaining production
  hits.
- The legacy C-buffer `do_interp`, `interp`, `interp_search`,
  `interp_backslash`, `normalize_refs`, and raw-buffer `nntp_gets`
  overloads are gone.
- Article input scan: production callers outside `artio.cpp` use the
  string-reading API.  Low-level article chunk readers now own
  `std::string` chunks.  Raw `read_art_buf(bool)` calls remain only in
  `artio.cpp` internals and in the implementation of the string reader.
- Unused overload/wrapper scan: no `finish_command(int)` wrapper remains.
  Keep `nntp_init_error`, `string_case_compare`,
  `string_case_equal`, `Tgetstr`, `line_ptr`, `line_offset`,
  `yes_or_no`, `empty`, `plural`, `force_me`, and `at_grey_space`;
  they still have production/source callers or platform/API boundary
  use.
- Search API scan: article and newsgroup search now use their
  `std::string_view` command APIs directly.  Their raw-buffer
  compatibility wrappers are gone.
- Command dispatch scan: selector perform helpers, article shell escape
  and switcheroo dispatch, option-switch dispatch, and kill-file switch
  commands now pass command text as strings or views.
- Response command scan: save, reply, followup, and supersede operations
  now have `std::string_view` command entry points.  The perform
  save/view path no longer stages command text in `g_buf`, and mailbox
  format detection uses owner-local string storage.
- Response wrapper scan: no no-argument response wrappers remain.
- Article body scan: response quoting, article search, and article pager
  display/scroll paths now use owned line storage from the article I/O
  boundary instead of mutating raw article buffers.
- Numeric command scan: `num_num` now accepts command text directly and
  parses numeric range text from that view instead of `g_buf`.
- Terminal input scan: `in_char` and `in_answer` return caller-owned
  command strings while still staging `g_buf` for legacy callers.
  `store_command` no longer writes `g_buf`, `finish_command` callers
  pass command text directly, `in_choice` returns edited choice text
  through caller-owned string storage, and typeahead cleanup now uses
  owner-local scratch storage.
- Regex API scan: `CompiledRegex::compile` now accepts
  `std::string_view` directly.  The C-string and `std::string`
  compatibility overloads are gone, and production regex compile callers
  no longer pass `.c_str()`.
- Literal-only local pointer scan found no current Tier 0 leaf slices.
  `do_newsgroup`, `s_search`, and `sa_refresh_bot` now use
  `std::string_view`, `std::string`, or direct `fmt` output for the
  prior local message-selection cases.
- C numeric conversion scan found no current production hits.
- Caller-output helper signature scan found no remaining public
  production helper signatures.  The HTML filter file-local output
  cursor has been migrated to owned string storage; remaining
  caller-output buffers are lower-level I/O or protocol boundaries.
- Mutable input parameter scan found no new leaf slice after
  `parse_line` moved to `std::string_view`.  Remaining raw input
  parameters are covered by existing add-newsgroup, shell, UTF, article
  display, regex-bytecode, or platform/API boundary buckets.
- Additional local cursor scan: `perform`, `print_lines`, and
  `do_article` still walk local raw pointers over owned string storage
  or internal article storage.
- UTF helper scan: `byte_length_at`, `code_point_at`,
  `visual_width_at`, `put_char_adv`, and `dectrl` still expose or depend
  on raw C-string cursor APIs.  These should be refactored bottom-up so
  display loops can consume `std::string_view` cursors.
- Shell helper scan: `do_shell` still takes raw C strings and forces
  many callers to pass `.c_str()` even when they already own
  `std::string` command text.
- Newsgroup data scan: `rc_line_data` and `rc_numbers_data` have no
  production callers.  Both mutable raw accessors can be removed.
- Newsgroup add scan: `add_newsgroup` now takes `std::string_view` and
  callers pass owned `std::string` storage directly.
- Non-zero C function dataflow scan: the remaining search/length
  production hit is exempt by user direction.  The only other `strlen`
  spelling is comment text.  No current helper-parameter copy-to-string
  leaf slice remains.
- Non-zero line-input, byte, and allocation-helper scans: the remaining
  `fgets` calls are low-level `FILE *` input boundaries behind
  string-returning article input APIs; the remaining `memset` is hash
  allocation-table initialization; `safe_malloc` and `safe_realloc` are
  hash, AddGroup pointer table, regex bytecode, or generic allocator
  internals.
- Fixed-size buffer scan: the remaining `char name[N]` hits are
  immutable labels, termcap test/API shims, lookup tables, regex
  bytecode, or non-string byte arrays.  No new fixed-string-buffer slice
  is active from this pass.
- Global command buffer scan: only `ask_memorize`, `in_char`, and
  `in_answer` still actively touch `g_buf`; score-file comment text and
  the declaration/definition remain until those users are removed.
- Non-zero C output calls are inventory until each source location is
  promoted to an explicit function-level slice.  Do not add broad
  coverage slices for output-call families.
- MIME content-decoding paths now own local string storage for decoded
  lines.  HTML filtering has an owned public API and owned file-local
  output storage.
- NNTP response parsing no longer uses `sscanf`.  The shared
  `g_ser_line` status owner is now `std::string`; remaining NNTP line
  storage is protocol/body input rather than status-text storage.
- Article display/copy paths now use `std::string` for the shared
  article line.  Low-level article input APIs now return
  `std::string`; the remaining `fgets` calls are internal `FILE *`
  boundaries.
- No active `strcpy`, `strncpy`, `std::sprintf`, or `gets` production
  hits remain in this scan.
- The `strcmp` and `gets` hits in `parsedate.y` are exempt from
  modernization by user direction.
- No active `strchr` or `strlen` production hits remain.  The `strlen`
  spelling in `charsubst.cpp` is comment text, not a call.
- Filename storage already uses modern path or view signatures for most
  owners.  `ScoreFile::fname` remains `std::string` because it is a
  score-file cache key that can hold a URL as well as a local path.
  Other filename strings are already `std::string`/`fs::path` values or
  cross C `FILE*` APIs.

## Current C String Function Inventory

The current scan covers the source and test roots listed above.  Counts
below are identifier-aware call counts for `std::` calls and unqualified
C calls.  Comment text is excluded by inspection.  The scan excludes
legacy Configure scripts and `vcpkg`, but it does not preprocess
conditional blocks.  Exempt `parsedate.y` hits are listed in the source
map but are not included in the active counts below.

- C line input: `fgets` 2.
- C text output: `fprintf`/`std::fprintf` 13.
- Character output: `putchar`/`std::putchar` 81.
- Character byte operations: `memset` 1.

The scan found no current active source/test hits for `strcmp`,
`strcpy`, `strncpy`, `strcat`, `strncat`, `strncmp`, `strchr`,
`strrchr`, `strstr`, `strlen`, `strspn`, `strcspn`, `strpbrk`,
`strtok`, `sprintf`, `snprintf`, `sscanf`, `vsprintf`, `vsnprintf`,
`gets`, `fputs`, `puts`, `printf`, `std::printf`, `memcpy`, `memmove`,
`memcmp`, `memchr`, `atoi`, `atol`, `std::atoi`, `std::atof`,
`std::atol`, `std::strtol`, `std::strtoul`, or `std::strtod`.

`fmt::sprintf` appears three times.  These calls are not C buffer
writes.  They are tracked only where the format template itself should
be modernized.

The `gets` hit is in the inactive `parsedate.y` `#ifdef TEST` harness
and is exempt from modernization by user direction.
The `fputs` spellings in `kfile.cpp` are comment text.
The `strlen` spellings in `charsubst.cpp` are comment text.

## Current C Function Source Map

- `strcmp`: `parsedate/parsedate.y` has 1 exempt `#ifdef TEST`
  harness hit.
- `fgets`: `libtrn/artio.cpp`, `read_art_chunk`; `libtrn/nntp.cpp`,
  `read_art_file_chunk`.  These are internal `FILE *` boundaries behind
  string-returning APIs.
- `gets`: `parsedate/parsedate.y` `#ifdef TEST` harness.  Exempt from
  modernization by user direction.
- `memset`: `libtrn/hash.cpp`, `hash_create`.  This is hash
  allocation-table initialization.
- `printf`/`std::printf`: `parsedate/parsedate.y` has 5 exempt
  `#ifdef TEST` harness hits.
- `fprintf`/`std::fprintf`: `config/include/config/common.h` 1,
  `libtrn/color.cpp` 2, `decode.cpp` 1, `head.cpp` 1, `nntp.cpp` 1,
  `opt.cpp` 2, `respond.cpp` 2, `scoresave.cpp` 1, `terminal.cpp` 1,
  and `univ.cpp` 1.  Split into explicit slices before editing.
- `putchar`/`std::putchar`: `libtrn/include/trn/terminal.h` 1,
  `art.cpp` 12, `charsubst.cpp` 5, `color.cpp` 1, `datasrc.cpp` 4,
  `final.cpp` 1, `init.cpp` 1, `kfile.cpp` 1, `ng.cpp` 4,
  `rcstuff.cpp` 3, `rt-page.cpp` 8, `rt-select.cpp` 5,
  `rt-util.cpp` 6, `score.cpp` 2, `sdisp.cpp` 3, `smisc.cpp` 1,
  `terminal.cpp` 21, and `utf.cpp` 2.  Promote concrete source
  locations to explicit function-level slices before editing.

## Refactoring Slices

Slices are stable.  Do not renumber remaining slices when one is
completed; remove the completed slice.  Slice IDs are also monotonic:
never reuse a completed ID, even if that ID is no longer visible in this
file.  The next new slice ID is `CSTR-533`.  When adding slices, assign
IDs starting there and then update this allocator line past the highest
new ID.  The physical order is grouped by dependency tier: finish
earlier tiers first so later caller and shared-buffer slices have
cleaner helper and ownership contracts to build on.

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

#### CSTR-529 - Convert Remaining Small `std::fprintf` Calls

- Files: `config/include/config/common.h`, `libtrn/color.cpp`,
  `libtrn/decode.cpp`, `libtrn/head.cpp`, `libtrn/nntp.cpp`,
  `libtrn/opt.cpp`, `libtrn/respond.cpp`, `libtrn/scoresave.cpp`,
  `libtrn/terminal.cpp`, `libtrn/univ.cpp`.
- Kind: C text output.
- Functions: assertion reporting, color validation, decode total output,
  NNTP diagnostics, option save headers, mailbox separators, score-save
  line output, color-limit diagnostics, and universal selector header
  output.
- Dependencies: none.
- Change: replace live `std::fprintf` calls with `fmt` output.  Preserve
  target streams and literal output text.
- Tests: full build and affected focused tests where available.

### Tier 1 - Helper And API Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

No current slices.

### Tier 2 - Tool-local And Owner-local Storage

These slices replace one parser or local owner of string storage.  Finish
them before broad global-buffer work and before removing helpers.

No current slices.

### Tier 3 - Workflow Callers And Path Owners

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

No current slices.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

#### CSTR-530 - Remove `g_buf` From Memorize Prompt

- Files: `libtrn/ng.cpp`.
- Kind: global command buffer read.
- Function: `ask_memorize`.
- Dependencies: none.
- Change: use the string returned by `in_char` for the selected command
  character instead of reading `*g_buf`.
- Tests: memorize-thread and memorize-subject prompt tests.

#### CSTR-531 - Stop Mirroring `in_char` Into `g_buf`

- Files: `libtrn/terminal.cpp`.
- Kind: global command buffer write.
- Function: `in_char`.
- Dependencies: `CSTR-530`.
- Change: return the stored command string without copying it into the
  global command buffer.
- Tests: `TerminalTest` in-char cases and full command-input tests.

#### CSTR-532 - Stop Mirroring `in_answer` Into `g_buf`

- Files: `libtrn/terminal.cpp`.
- Kind: global command buffer write.
- Function: `in_answer`.
- Dependencies: none.
- Change: return the stored command string without copying it into the
  global command buffer.
- Tests: `TerminalTest` in-answer cases and followup prompt tests.

### Tier 5 - Helper Removal

These slices remove helpers only after every direct caller has moved to
owned strings or owner-specific storage.

#### CSTR-412 - Remove Public Raw Article Buffer API

- Files: `libtrn/artio.cpp`, `libtrn/include/trn/artio.h`,
  `tests/test_artio.cpp`.
- Kind: raw string return helper.
- Function: `read_art_buf(bool)`.
- Dependencies: none.
- Change: after production callers use owned line storage, remove the
  public `char *` article-buffer overload or make it file-local
  implementation detail.  Keep `read_art_buf(std::string &, bool)` as
  the caller-facing API and update tests to validate the string result.
- Tests: `ArticleIoTest` read-buffer cases.

#### CSTR-486 - Remove Global `g_buf`

- Files: `config/common.cpp`, `config/include/config/common.h`, all
  remaining production users.
- Kind: final global storage removal.
- Function: `g_buf`.
- Dependencies: `CSTR-530` through `CSTR-532`.
- Change: delete the global command buffer after all remaining users own
  their storage locally.  Do not replace it with another global string.
- Tests: full build and full test workflow.
