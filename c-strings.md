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
  Do not treat `putchar` or `std::putchar` as C-string audit roots:
  they accept a single character, not a C string.
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
`Article` and `Subject` fields, `HashDatum` payloads, `parse_string`,
and `push_string`.
`CompiledRegex::m_exp_buf` and `m_alternatives` are regex bytecode and
internal cursors, not ordinary string storage.  Modernize that code by
hiding the engine internals, using view/string result contracts at the
public boundary, and replacing growable bytecode storage with standard
containers and offsets rather than treating bytecode as text.

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
- Fixed raw buffers: the local `opt_init` `argv` shim in
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
  `std::string` chunks.  The public article-buffer API returns text
  through caller-owned `std::string` storage.
- Unused overload/wrapper scan: no `finish_command(int)` wrapper remains.
  Keep `nntp_init_error`, `string_case_compare`,
  `string_case_equal`, `Tgetstr`, `line_ptr`, `line_offset`,
  `yes_or_no`, `empty`, `plural`, and `force_me`; they still have
  production/source callers or platform/API boundary use.
- Search API scan: article and newsgroup search now use their
  `std::string_view` command APIs directly.  Their raw-buffer
  compatibility wrappers are gone.
- Command dispatch scan: selector perform helpers, article shell escape
  and switcheroo dispatch, option-switch dispatch, and kill-file switch
  commands now pass command text as strings or views.
- Response command scan: save, reply, followup, and supersede operations
  now have `std::string_view` command entry points, and mailbox format
  detection uses owner-local string storage.
- Response wrapper scan: no no-argument response wrappers remain.
- Article body scan: response quoting, article search, and article pager
  display/scroll paths now use owned line storage from the article I/O
  boundary instead of mutating raw article buffers.
- Article-walk callback scan: `output_subject` still exposes the old
  `char *` callback shape even though it immediately casts the argument
  to `Article *`.  Direct production and test callers also cast
  `Article *` to `char *` just to call it.  Modernize the typed
  operation first, then update `article_walk` callback plumbing.
- Character substitution scan: `g_char_subst` is a borrowed cursor into
  mutable `g_charsets` storage.  Reset and cycle callers assign or
  increment the raw pointer, while display callers dereference it to get
  the current substitution mode.  Modernize this by exposing mode-state
  helpers and replacing the pointer with an owned index into
  `g_charsets`.
- Numeric command scan: `num_num` accepts command text directly and
  parses numeric range text from that view.
- Terminal input scan: `in_char` and `in_answer` return caller-owned
  command strings.  `finish_command` callers pass command text directly,
  `in_choice` returns edited choice text through caller-owned string
  storage, and typeahead cleanup now uses owner-local scratch storage.
- Interpolator init scan: `interp_init` still accepts a mutable
  termcap buffer plus size but immediately discards both arguments.
  Production and test callers still need the same buffer for `opt_init`
  and `term_set`, so the useful cleanup is limited to the
  `interp_init` signature and call sites.
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
  `do_article` now use `std::string_view` APIs for local cursor
  processing.  The prior article color and regex C-string bridge
  lambdas are gone.
- UTF helper scan: `put_char_adv` now consumes `std::string_view &`.
  The raw single-position overloads for `at_norm_char`,
  `byte_length_at`, `visual_width_at`, and `code_point_at` are gone.
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
  allocation-table initialization; the active `safe_realloc` caller is
  regex bytecode growth, not string storage.
- Fixed-size buffer scan: the remaining `char name[N]` hits are
  immutable labels, termcap test/API shims, lookup tables, regex parser
  state, or non-string byte arrays.  No new fixed-string-buffer slice is
  active from this pass.
- Global command buffer scan: no current active source/test hits remain.
- C text output scan: no active C `printf`, `fprintf`, or `fputs`
  calls remain.
- Article buffer scan: the public raw `read_art_buf(bool)` API is gone,
  but `g_art_buf` and its companion position globals are still public
  shared article-buffer state used by `art.cpp`, `ng.cpp`, and tests.
  That is a broad owner/encapsulation problem rather than a leaf
  C-string call-site cleanup.
- Regex scan: all production and test `CompiledRegex::execute` callers
  use the `std::string_view` boolean match API.  The old C-string
  wrapper is gone.  Bracket access returns `std::string_view` into match
  storage; the old file-scope scratch string is gone.  The remaining raw
  regex members and helper methods are internal engine storage that still
  leaks through the public struct and are tracked as active slices.
- MIME content-decoding paths now own local string storage for decoded
  lines.  HTML filtering has an owned public API and owned file-local
  output storage.
- MIME state scan: `mime_set_state(char *bp)` still uses mutable
  C-string input as a signaling mechanism by writing `'\0'` when the
  current line is consumed.  The existing `std::string &` overload
  delegates through `.data()` and then trims at the first NUL.  Replace
  this with a `std::string_view` input and an explicit boolean result
  that tells callers whether to suppress the line.
- Public header declaration scan: all remaining `char *` declarations in
  `libtrn/include/trn` are now represented by slices.  Existing slices
  cover article buffers, regex internals, character substitution,
  `interp_init`, `output_subject`, `article_walk`, and `mime_set_state`.
  New slices cover hash payloads, `argv` entry points, option and
  terminal scratch buffers, terminal capability strings, terminal
  input/output helpers, allocation helpers, and remaining small string
  helpers.
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
- A follow-up regex audit found that `CompiledRegex` still exposes
  C-style helper methods and raw implementation storage even though the
  direct match API is modern.  Those issues are tracked as Tier 1 and
  Tier 2 slices.

## Current C String Function Inventory

The current scan covers the source and test roots listed above.  Counts
below are identifier-aware call counts for `std::` calls and unqualified
C calls.  Comment text is excluded by inspection.  The scan excludes
legacy Configure scripts and `vcpkg`, but it does not preprocess
conditional blocks.  Exempt `parsedate.y` hits are listed in the source
map but are not included in the active counts below.

- C line input: `fgets` 2.
- Character byte operations: `memset` 1.

The scan found no current active source/test hits for `strcmp`,
`strcpy`, `strncpy`, `strcat`, `strncat`, `strncmp`, `strchr`,
`strrchr`, `strstr`, `strlen`, `strspn`, `strcspn`, `strpbrk`,
`strtok`, `sprintf`, `snprintf`, `sscanf`, `vsprintf`, `vsnprintf`,
`gets`, `fputs`, `puts`, `printf`, `std::printf`, `fprintf`,
`std::fprintf`, `memcpy`, `memmove`, `memcmp`, `memchr`, `atoi`,
`atol`, `std::atoi`, `std::atof`, `std::atol`, `std::strtol`,
`std::strtoul`, or `std::strtod`.

`fmt::printf` appears three times and `fmt::sprintf` appears three times.
These calls are not C buffer writes.  They are intentionally retained
where the format template is runtime printf-style text: article and
newsgroup prompts, terminal capability shims, and interpolation width
specifiers.

`putchar` and `std::putchar` are intentionally excluded from this audit:
they accept a single character, not a C string.

The `gets` hit is in the inactive `parsedate.y` `#ifdef TEST` harness
and is exempt from modernization by user direction.
The `fputs` spellings in `inews.cpp` are the local helper name
`inews_fputs`; the spellings in `color.cpp` and `kfile.cpp` are comment
text.
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

## Refactoring Slices

Slices are stable.  Do not renumber remaining slices when one is
completed; remove the completed slice.  Slice IDs are also monotonic:
never reuse a completed ID, even if that ID is no longer visible in this
file.  The next new slice ID is `CSTR-587`.  When adding slices, assign
IDs starting there and then update this allocator line past the highest
new ID.  The physical order is grouped by dependency tier: finish
earlier tiers first so later caller and shared-buffer slices have
cleaner helper and ownership contracts to build on.

Re-evaluate every slice after each scan; old deferrals are not binding.

### Tier 0 - Leaf Cleanup

These slices have no slice dependency.  They remove local C string
construction, comparison, or display roots without changing a larger
owner.

### Tier 1 - Helper And API Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

#### CSTR-566 - Give output_subject A Typed Article Interface

- Type: callback-shaped public function.
- Files: `libtrn/include/trn/ng.h`, `libtrn/ng.cpp`,
  `libtrn/ngstuff.cpp`, `tests/test_subject.cpp`.
- Function: `output_subject`.
- Dependencies: none.
- Instructions: change the public `output_subject` operation to take an
  `Article &` plus the existing flag.  Update direct production and test
  callers to pass `Article` directly.  Keep any required `article_walk`
  compatibility as a file-local callback adapter in `ng.cpp`, not as the
  public API shape.

#### CSTR-567 - Give article_walk A Typed Article Callback

- Type: callback plumbing using `char *` as erased storage.
- Files: `libtrn/include/trn/cache.h`, `libtrn/cache.cpp`,
  `libtrn/bits.cpp`, `libtrn/kfile.cpp`, `libtrn/ng.cpp`,
  `libtrn/rt-select.cpp`, article-walk tests.
- Function: `article_walk`.
- Dependencies: CSTR-566.
- Instructions: change `article_walk` to call callbacks as
  `bool callback(Article &, int)`, then update the callback family to
  accept `Article &` instead of `char *` and remove the internal casts.
  Remove the temporary `output_subject` callback adapter from CSTR-566.

#### CSTR-572 - Migrate Article-buffer mime_set_state Callers

- Type: raw article-buffer pointer callers.
- Files: `libtrn/artio.cpp`, article I/O MIME tests.
- Function: `mime_set_state`.
- Dependencies: none.
- Instructions: update article-buffer callers to pass a line view to the
  non-mutating `mime_set_state` API and use the returned boolean instead
  of checking whether the first character was changed to `'\0'`.  Do not
  preserve the old caller-visible buffer mutation.

#### CSTR-573 - Migrate String mime_set_state Callers

- Type: mutable string overload callers.
- Files: `libtrn/mime.cpp`, MIME decode tests.
- Function: `mime_set_state`.
- Dependencies: none.
- Instructions: update `std::string` callers to pass a view and clear or
  skip the owned line explicitly when the returned boolean says the MIME
  state consumed it.  Remove dependence on `.data()` and NUL trimming.

#### CSTR-576 - Introduce Program Argument Views

- Type: public `argv` pointer arrays.
- Files: `libtrn/include/trn/init.h`, `libtrn/include/trn/trn.h`,
  `libtrn/include/trn/opt.h`, `libtrn/init.cpp`, `libtrn/trn.cpp`,
  `libtrn/opt.cpp`, entry-point tests.
- Functions: `initialize`, `trn_main`, `opt_init`.
- Dependencies: none.
- Instructions: add a small argument-view type or helper that represents
  command-line arguments as `std::string_view` values after the C `main`
  boundary.  Migrate `initialize`, `trn_main`, and the `argv` half of
  `opt_init` to that typed view.  Keep the raw `main` boundary outside
  the public libtrn headers.

#### CSTR-577 - Replace Termcap Scratch Pointers With A Buffer Type

- Type: mutable fixed-size scratch-buffer parameters.
- Files: `libtrn/include/trn/opt.h`, `libtrn/include/trn/terminal.h`,
  `libtrn/opt.cpp`, `libtrn/terminal.cpp`, `libtrn/init.cpp`,
  interpolator and terminal tests.
- Functions: `opt_init`, `term_set`.
- Dependencies: none.
- Instructions: introduce a named termcap scratch-buffer type, such as
  an alias around `std::array<char, TCBUF_SIZE>`, and pass it by
  reference to `opt_init` and `term_set`.  Keep `.data()` calls local to
  the termcap implementation boundary.

#### CSTR-580 - Return Terminal Color Capabilities As Views

- Type: borrowed terminal capability result.
- Files: `libtrn/include/trn/terminal.h`, `libtrn/terminal.cpp`,
  `libtrn/color.cpp`, terminal/color tests.
- Function: `tc_color_capability`.
- Dependencies: CSTR-579.
- Instructions: change `tc_color_capability` to return
  `std::string_view`, using an empty view as the missing-capability
  sentinel.  Update color callers to test `.empty()` and pass views
  directly where possible.

#### CSTR-581 - Use Views For Public Termcap String Arguments

- Type: read-only termcap C-string parameters.
- Files: `libtrn/include/trn/terminal.h`, `libtrn/terminal.cpp`,
  `libtrn/sdisp.cpp`, terminal tests.
- Functions: `tgoto_string`, `tputs`.
- Dependencies: CSTR-579.
- Instructions: change trn-owned wrappers that consume termcap strings
  to accept `std::string_view`.  Keep any required `.c_str()` conversion
  local to calls into external termcap APIs.  For inactive `MSDOS`
  blocks, update the code so the blocks still build rather than deleting
  them.

#### CSTR-582 - Make save_typeahead Consume A String View

- Type: buffer-plus-size read-only input.
- Files: `libtrn/include/trn/terminal.h`, `libtrn/terminal.cpp`,
  `libtrn/trn.cpp`, terminal tests.
- Function: `save_typeahead`.
- Dependencies: none.
- Instructions: change `save_typeahead` to accept `std::string_view`
  input.  At callers, pass the already-owned command suffix as a view
  instead of passing `data()` plus an arbitrary buffer size.

#### CSTR-583 - Hide The read_tty Output Buffer Boundary

- Type: public caller-owned output buffer.
- Files: `libtrn/include/trn/terminal.h`, `libtrn/terminal.cpp`,
  terminal and command-input tests.
- Function: `read_tty`.
- Dependencies: none.
- Instructions: replace public `read_tty(char *addr, int size)` callers
  with typed helpers for the observed use cases, especially single
  character reads and owned string reads.  Keep the raw OS read buffer
  local to `terminal.cpp`; do not expose local string storage addresses
  through output parameters.

### Tier 2 - Tool-local And Owner-local Storage

These slices replace one parser or local owner of string storage.  Finish
them before broad global-buffer work and before removing helpers.

#### CSTR-557 - Use Standard Regex Fixed-size Bookkeeping

- Type: fixed-size C arrays and narrow count storage.
- Files: `libtrn/include/trn/search.h`, `libtrn/search.cpp`,
  regex tests.
- Variables: local `bracket`, `m_num_brackets`, fixed regex arrays that
  are not yet replaced by later slices.
- Dependencies: none.
- Instructions: replace fixed regex bookkeeping arrays with
  `std::array` where the size is fixed by `NBRA` or `NALTS`, and use an
  ordinary integer or size type for counts instead of `char`.  Do not
  convert bytecode storage to `std::string`; it is not text.

#### CSTR-558 - Store Regex Match Spans Without Raw Member Pointers

- Type: raw pointer match-span storage.
- Files: `libtrn/include/trn/search.h`, `libtrn/search.cpp`,
  regex bracket tests.
- Members: `m_bracket_start_list`, `m_bracket_end_list`,
  `m_bracket_str`.
- Dependencies: CSTR-557.
- Instructions: replace persistent raw start/end pointer member arrays
  with span state that cannot outlive the matched text accidentally.
  Prefer offsets or `std::string_view` values built during `execute`,
  then have `get_bracket` return a view over owned match storage.
  Reject any design that stores pointers into caller-local string data
  after `execute` returns.

#### CSTR-559 - Replace Regex Bytecode Reallocation With Vector Storage

- Type: growable bytecode storage and pointer cursor cleanup.
- Files: `libtrn/include/trn/search.h`, `libtrn/search.cpp`,
  regex compile and match tests.
- Members: `m_exp_buf`, `m_eb_len`, `m_alternatives`.
- Functions: `compile`, `grow_eb`, `execute`, `advance`, `back_ref`.
- Dependencies: CSTR-557, CSTR-558.
- Instructions: replace manual `safe_malloc`/`safe_realloc` bytecode
  storage with `std::vector<char>` and replace persistent alternative
  pointers with offsets or indices.  Keep pointer walking local to the
  regex VM only where it simplifies bytecode interpretation; no pointer
  into vector storage may escape as public API or persistent caller
  state.

### Tier 3 - Workflow Callers And Path Owners

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

#### CSTR-561 - Use Character-substitution Accessors At Read Sites

- Type: raw global cursor reads.
- Files: `libtrn/respond.cpp`, `libtrn/rt-util.cpp`,
  `libtrn/rt-wumpus.cpp`, related tests.
- Expressions: `*g_char_subst`, `g_char_subst[0]`.
- Dependencies: none.
- Instructions: replace read-only uses of the substitution cursor with
  the current-mode accessor.  Pass the returned character directly to
  `str_char_subst`; do not construct a string or string view when a
  single mode character is all the callee needs.

#### CSTR-562 - Use Character-substitution Accessors At Cycle Sites

- Type: raw global cursor mutation.
- Files: `libtrn/art.cpp`, `libtrn/ng.cpp`, related tests.
- Expressions: `g_char_subst = g_charsets.c_str()`,
  `++g_char_subst`.
- Dependencies: none.
- Instructions: replace manual reset and cycle logic with the
  char-substitution state helpers.  Preserve the existing wrap behavior:
  advancing past the end of `g_charsets` returns to the first configured
  mode.

#### CSTR-563 - Move Tests Off The Raw Character-substitution Pointer

- Type: test global cursor manipulation.
- Files: `tests/test_interp.cpp`, `tests/test_rt-util.cpp`,
  `tests/test_rt-wumpus.cpp`, `tests/test_scmd.cpp`,
  `tests/test_utf.cpp`.
- Expressions: direct `g_char_subst` assignment and `ValueSaver`
  storage.
- Dependencies: CSTR-561, CSTR-562.
- Instructions: update tests to use the same state helpers as production
  code.  Add a scoped test saver only if the helper API cannot express
  the existing setup/teardown cleanly.  Test code is held to the same
  standard as production code.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

#### CSTR-579 - Encapsulate Terminal Capability Globals

- Type: public borrowed terminal capability strings.
- Files: `libtrn/include/trn/terminal.h`, `libtrn/terminal.cpp`,
  terminal and color tests.
- Globals: `g_tc_BC`, `g_tc_UP`, `g_tc_CR`, `g_tc_VB`, `g_tc_CE`,
  `g_tc_CM`, `g_tc_HO`, `g_tc_IL`, `g_tc_CD`, `g_tc_SO`, `g_tc_SE`,
  `g_tc_US`, `g_tc_UE`, `g_tc_UC`.
- Dependencies: CSTR-577.
- Instructions: hide terminal capability storage behind named accessors
  or an owner object.  Public callers should consume `std::string_view`
  or typed helpers instead of reading raw borrowed pointers.  Keep the
  termcap-library pointer lifetime handling inside `terminal.cpp`.

#### CSTR-564 - Replace g_char_subst With Owned Index State

- Type: global borrowed pointer storage.
- Files: `libtrn/include/trn/charsubst.h`, `libtrn/charsubst.cpp`,
  `libtrn/opt.cpp`, character-substitution tests.
- Globals: `g_char_subst`, `g_charsets`.
- Dependencies: CSTR-561, CSTR-562, CSTR-563.
- Instructions: remove the exported `const char *g_char_subst` cursor
  and store the current substitution as an owned index into
  `g_charsets`.  When `g_charsets` is reassigned, normalize the index so
  the current-mode accessor remains valid.  Use an empty current mode as
  the sentinel if `g_charsets` is empty; do not retain pointer/null
  semantics.

#### CSTR-553 - Encapsulate Public Article Buffer State

- Type: public global raw buffer ownership.
- Files: `libtrn/include/trn/artio.h`, `libtrn/artio.cpp`,
  `libtrn/art.cpp`, `libtrn/ng.cpp`, article-buffer tests.
- Globals: `g_art_buf`, `g_art_buf_pos`, `g_art_buf_seek`,
  `g_art_buf_len`.
- Dependencies: none.
- Instructions: replace direct public access to article-buffer storage
  with owned accessors or owner-local state in `artio.cpp`.  Preserve
  existing behavior with tests before changing the storage shape.

#### CSTR-585 - Replace HashDatum char Pointer Payloads

- Type: generic hash payload stored as `char *`.
- Files: `libtrn/include/trn/hash.h`, `libtrn/hash.cpp`, hash owners,
  hash tests.
- Members: `HashDatum::dat_ptr`, `HashDatum::dat_len`.
- Dependencies: none.
- Instructions: audit each `HashDatum` owner and replace the misleading
  `char *` payload with typed owner-specific storage where practical.
  If a generic hash payload remains necessary during migration, use an
  explicitly type-erased pointer such as `void *` rather than a C-string
  pointer, and keep conversions at owner boundaries.

### Tier 5 - Helper Removal

These slices remove helpers only after every direct caller has moved to
owned strings or owner-specific storage.

#### CSTR-578 - Remove Null-aware empty C-string Helper

- Type: obsolete C-string algorithm helper.
- Files: `libtrn/include/trn/string-algos.h`, `libtrn/terminal.cpp`,
  `tests/test_string-algos.cpp`.
- Function: `empty(const char *)`.
- Dependencies: CSTR-579.
- Instructions: after terminal capability access no longer exposes raw
  nullable pointers, remove the null-aware `empty(const char *)` helper
  and its tests.  Use ordinary `.empty()` checks on strings or views.

#### CSTR-574 - Remove Legacy mime_set_state Overloads

- Type: obsolete mutable C-string and mutable string overloads.
- Files: `libtrn/include/trn/mime.h`, `libtrn/mime.cpp`, MIME tests.
- Function: `mime_set_state`.
- Dependencies: CSTR-572, CSTR-573.
- Instructions: delete the `char *` and `std::string &` overloads after
  all callers use the `std::string_view` boolean-result API directly.
  Keep only the non-mutating public signature.

#### CSTR-586 - Remove Public safe_malloc And safe_realloc Char APIs

- Type: public raw allocation helpers returning `char *`.
- Files: `libtrn/include/trn/util.h`, `libtrn/util.cpp`,
  remaining allocation owners and tests.
- Functions: `safe_malloc`, `safe_realloc`.
- Dependencies: CSTR-559, CSTR-585.
- Instructions: after regex bytecode and hash payload owners no longer
  need raw byte allocation helpers, remove the public `char *`
  allocation APIs.  Replace remaining fixed or dynamic arrays with
  standard containers, or keep a typed file-local allocation helper only
  where an external C API truly requires raw storage.

