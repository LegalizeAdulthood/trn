<!-- Copyright (c) 2026, Richard Thomson -->

# C String Audit

## Scope

Audited project C and C++ sources under the source root, excluding the
vendored `vcpkg` tree.  The audit looked for local raw C string pointers
and function parameters that can become `std::string_view` or
`std::string` without changing ownership boundaries.

Follow-up passes also look for fixed-length `char name[N]` buffers in
all storage classes and for functions that hide owned string allocation
behind a raw `char *` return.

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
- `char *` storage populated from `save_str` or `safe_copy` that can
  become owned `std::string` storage without pointer escape.
- `char *` results from functions that return owned raw strings from
  `save_str`, `safe_malloc`, `safe_realloc`, or another owning helper.
  Summarize the return ownership, then trace callers that store, use, and
  free the result locally.
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
  `snprintf`, `vsprintf`, `vsnprintf`, `fgets`, `fputs`, `puts`,
  `printf`, and `fprintf` as audit roots.
- C byte library calls on character storage.  Treat `memcpy`, `memmove`,
  `memset`, `memcmp`, and `memchr` as audit roots when the destination
  or compared data is string-like `char` storage.  Non-string table
  clearing or raw byte protocol work is a non-string buffer finding.

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

Do not self-defer findings.  If source still contains matching raw
string ownership, a fixed buffer, a raw return, or path storage, keep the
candidate visible as a slice until it is completed or the user explicitly
says to defer or remove it.

Existing good precedents:

- `libtrn/univ.cpp`, `univ_add_text_file`: accepts a legacy C string at
  the boundary, then uses `std::string_view` for slicing and
  `std::string` for owned path assembly.
- `util/util2.cpp`, `file_exp`: accepts a `std::string_view`, keeps
  mutable scratch storage local, and returns an owned `std::string`.
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
`g_buf`, `g_ser_line`, `Article` and `Subject` fields,
`HashDatum` payloads, `parse_string`, and `push_string`.
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

### `save_str` or `safe_copy` to `std::string`

Select when a raw pointer owns retained text, the same owner frees or
overwrites it, and callers only need read-only C-string access or local
mutable parsing.  Reject memory-pool strings and `char **` output
allocation APIs until that lifetime model changes.  Include struct/class
members that are assigned from `save_str`, `safe_malloc`, or an owning
raw-string return and destroyed by the same owner.

Refactor by replacing the owning `char *` with `std::string` or
`std::optional<std::string>` when null and empty are distinct.  Replace
`save_str`, `safe_malloc`, `safe_copy`, and matching `free` paths with
direct string assignment.  Use `c_str()` for legacy read-only APIs and
`data()` only for local mutable parsing with no pointer escape.

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
Examples include direct returns from `save_str`, `safe_malloc`,
`safe_realloc`, or a helper already classified as returning owned raw
string storage.  Record whether the function always returns owned
storage, conditionally returns owned storage, returns pooled storage,
returns borrowed/static storage, or mixes ownership modes.

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
`std::string`.  Update callers in the same slice so `save_str` is not
used just to satisfy the old parameter contract.

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

- `strcpy`, `strncpy`, `strcat`, `strncat`, `safe_copy`, `safe_cat`,
  `sprintf`, `snprintf`, `vsprintf`, and `vsnprintf`: construction into
  a C string buffer.  Prefer `std::string` or `fmt` when the destination
  is owned local storage.  Move with the owning storage when the
  destination is global, static, struct storage, or caller output.
- `strcmp` and `strncmp`: comparison of null-terminated text.  Prefer
  direct `std::string` or `std::string_view` comparison when the inputs
  are already strings or can be viewed by extent.
- `strchr`, `strrchr`, `strstr`, `strlen`, `strspn`, `strcspn`,
  `strpbrk`, and `strtok`: parsing, slicing, or length discovery.
  Prefer `std::string_view` operations when the code does not need to
  mutate the source.  Preserve mutable parsing only when the function
  intentionally edits a caller buffer.
- `fgets`: fixed-size line input.  Prefer `std::string` line input when
  truncation is arbitrary.  Keep fixed protocol buffers only when the
  size is meaningful.
- `fputs`, `puts`, `printf`, and `fprintf`: C string output or
  printf-format output.  Prefer `fmt::print` for formatted output and
  keep `fputs` only when plain C-string output is simpler and does not
  hide string construction.
- `memcpy`, `memmove`, `memset`, `memcmp`, and `memchr`: classify only
  when applied to character storage.  Convert string-like character
  storage with its owner; ignore non-string table initialization and
  binary buffers.

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

Do not replace a `fetch_lines` local with `std::string` by copying the
owned raw result and then freeing it; that turns one heap allocation into
two.  Either change the producer to build the `std::string` directly in
the same slice, or leave the local raw ownership alone.

For owned raw-return helpers, prefer changing the producer to return
`std::string` and updating all direct callers in the same slice.  Do not
add caller-only copies or wrapper APIs when the producer can construct
the owned string directly.

When converting an owned global or file-scope `char *` to `std::string`,
replace `save_str`, `safe_malloc`, and `safe_copy` storage updates with
direct string assignment.  Do not keep a `safe_copy` call that writes to
string storage, and do not allocate first and then assign to a string.
Use `c_str()` for legacy read-only C APIs.  Use `data()` only for local
mutable parsing while the `std::string` object remains alive and no
pointer escapes.

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

The current scan covers production code under `config`, `libtrn`,
`util`, `nntp`, `inews`, `nntplist`, `trn-artchk`, and `tool`.  It
does not include tests, generated files, or the vendored `vcpkg` tree.

- `save_str`: no production hits remain in the current tree.
- `safe_copy`: four hits remain: the helper declaration, the helper
  definition, and two call sites in two owner clusters.  The call
  sites are inventoried below.
- `safe_malloc`: remaining string-shaped owners are `g_head_buf` and
  `g_art_buf`.  Non-string owners include the `AddGroup` temporary
  pointer list, hash tables, regex bytecode, and generic allocation
  helpers.
- `safe_realloc`: string-shaped owners are `g_head_buf` and
  `g_art_buf`.  Regex bytecode remains a non-string owner.
- Direct environment C-string reads now remain only inside the config env
  wrapper implementation.
- Fixed raw buffers: current candidates include `g_buf`, `g_ser_line`,
  `g_art_line`, `g_head_buf`, `g_art_buf`, MIME HTML tag parser state,
  `uudecode` pending-line storage, selector command key storage,
  tree-indent storage, terminal push-string expansion storage, terminal
  command-input scratch, and the MSDOS `tgoto` static return buffer.
  Tiny UTF byte scratch buffers, translation tables, MIME decode tables,
  terminal pushback bytes, termcap storage, and regex bytecode arrays are
  non-string protocol or parser storage, not current string slices.
- Termcap capability globals are still exposed as mutable `char *` even
  though the code treats most capability text as read-only after
  discovery.  This is a const-correctness slice before the remaining
  MSDOS `tgoto` cleanup.
- INI and option helper classes own strings but still expose nullable
  `const char *` views through `c_str`-shaped accessors.  These are
  config or draft boundaries where absence is meaningful, so promote
  them to optional string views rather than plain strings.
- Several call sites now use `std::string` as a fixed-size C output
  buffer for `interp` or `do_interp`, then trim at the first NUL.  These
  are not raw arrays anymore, but they are still fixed-size C-string
  output contracts and should be converted one function at a time.
- Filename storage: newsrc fields are already `fs::path`.
  `read_auth_file` is still exposed as a nullable `const char *` path
  helper even though its callers already hold strings.  Score-file
  shortcut strings and universal-selector file strings still mix
  expansion templates, URLs, labels, and real paths, so they are not
  honest path-only candidates yet.
- `string_case_compare` production callers that already have strings or
  views now use the view overload instead of `c_str()`, `data()`, or
  pointer/length calls.  The remaining C-string overloads belong to the
  helper API and tests.
- `string_case_equal` production callers that already have strings or
  views now use the view overload instead of `c_str()`, `data()`, or
  pointer/length calls.  Remaining pointer/length calls belong to parser
  cursors and shared buffers and move with their owner slices.
- The comparison cleanup also narrowed local work inside `addng`,
  `ngdata`, `respond`, `rthread`, `rt-ov`, and `univ`.  Do not add
  separate slices for those completed `string_case_compare` call sites.
- Remaining literal tables include color object names, signal names,
  status labels, MIME entity mappings, and transliteration tables.  The
  useful current targets are the tables whose users already operate on
  views or compute lengths manually.

## Current `safe_copy` Inventory

The current tree has four `safe_copy` hits: the helper definition, the
helper declaration, and two call sites.  Keep each call site visible
until the owning storage or API changes.

- `libtrn/artio.cpp`, `read_art_buf`: compacts a mutable article buffer
  during word wrapping.  See `CSTR-033`.
- `libtrn/nntp.cpp`, `nntp_read_art`: compacts an NNTP protocol line.
  See `CSTR-036`.

## Current C String Function Inventory

The current scan covers the production roots listed above.  Counts below
are raw direct token counts for `std::` calls and unqualified C calls in
production code.

- Copy and concatenation: `strcpy` 30, `strncpy` 3, `strcat` 1.
- Comparison: `strcmp` 4, `strncmp` 23.
- Search and length: `strchr` 77, `strrchr` 5, `strstr` 2,
  `strlen` 74.
- Formatting into C buffers: `sprintf` 34, `snprintf` 2.
- C text I/O roots: `fgets` 29, `fputs` 206, `printf` 398,
  `fprintf` 55.
- Character byte operations: `memcpy` 7, `memset` 7, `memcmp` 1.

The scan found no current production hits for `strncat`, `strspn`,
`strcspn`, `strpbrk`, `strtok`, `vsprintf`, `vsnprintf`, `puts`,
`memmove`, or `memchr`.

High-count functions are not self-deferred.  They are grouped into
owner slices below because most calls sit on shared buffers such as
`g_buf`, `g_msg`, `g_ser_line`, article storage, terminal
storage, and parser workspaces.

## Refactoring Slices

Slices are stable.  Do not renumber remaining slices when one is
completed; remove the completed slice.  The physical order is grouped
by dependency tier: finish earlier tiers first so later caller and
shared-buffer slices have cleaner helper and ownership contracts to
build on.

### Tier 0 - Leaf Cleanup

These slices have no slice dependency.  They remove local C string
construction, comparison, or display roots without changing a larger
owner.

### Tier 1 - Helper And API Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

### Tier 2 - Tool-local And Owner-local Storage

These slices replace one owner of string storage.  Finish these before
broad global-buffer work.

#### CSTR-113 - Cancel Header Interpolation Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size string used as C output buffer.
- Function: `cancel_article`.
- Change: replace the `5 * LINE_BUF_LEN` header interpolation buffer
  with owned string construction for `CANCELHEADER`.  Preserve header
  file contents and failure handling.
- Tests: cancel response tests before refactor.

#### CSTR-114 - Supersede Header Interpolation Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size string used as C output buffer.
- Function: `supersede_article`.
- Change: replace the `5 * LINE_BUF_LEN` header interpolation buffer
  with owned string construction for `SUPERSEDEHEADER`.  Preserve body
  inclusion behavior.
- Tests: supersede response tests before refactor.

#### CSTR-115 - Reply Header Interpolation Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size string used as C output buffer.
- Function: `reply`.
- Change: replace the `5 * LINE_BUF_LEN` header interpolation buffer
  with owned string construction for `MAILHEADER`.  Preserve body
  quoting and header-display behavior.
- Tests: reply response tests before refactor.

#### CSTR-116 - Forward Header Interpolation Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size string used as C output buffer.
- Function: `forward`.
- Change: replace the `5 * LINE_BUF_LEN` header interpolation buffer
  with owned string construction for `FORWARDHEADER`.  Preserve MIME
  boundary detection and the non-`REGEX_WORKS_RIGHT` parsing path.
- Tests: forward response tests before refactor.

#### CSTR-117 - Followup Header Interpolation Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size string used as C output buffer.
- Function: `followup`.
- Change: replace the `5 * LINE_BUF_LEN` header interpolation buffer
  with owned string construction for `NEWSHEADER`.  Preserve body
  inclusion and new-topic prompting behavior.
- Tests: followup response tests before refactor.

#### CSTR-118 - Dead Article Copy Line Buffer

- Files: `libtrn/respond.cpp`.
- Kind: fixed-size line input string used as C output buffer.
- Function: `follow_it_up`.
- Change: replace the `CMD_BUF_LEN` `fgets` copy loop with
  `std::string` line input when appending a failed post to
  `dead.article`.  The current truncation is arbitrary file-copy
  chunking, not a meaningful line limit.
- Tests: failed-post dead-article tests before refactor.

### Tier 3 - Workflow Callers And Path Owners

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

#### CSTR-104 - MSDOS Tgoto Format Buffer

- Files: `libtrn/terminal.cpp`,
  `libtrn/include/trn/terminal.h`.
- Kind: static returned formatting buffer.
- Function: `tgoto` under `MSDOS`.
- Change: after `CSTR-106`, replace the static `gbuf[32]` returned
  buffer with a modern owner contract or remove the compatibility shim
  with the old DOS holdover cleanup.  Do not return a pointer into local
  string storage.
- Tests: build with the relevant feature setting if the shim remains.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

#### CSTR-031 - Global Command Buffer

- Files: `config/common.cpp`, `config/include/config/common.h`, many
  users.
- Kind: global fixed buffer `g_buf`.
- Function: storage-centered; no single function owns it.
- Change: replace the global line buffer with owned strings or scoped
  command objects.
- Tests: broad workflow required.

#### CSTR-033 - Article Body Wrap Buffer

- Files: `libtrn/artio.cpp`.
- Kind: mutable article-body buffer compaction.
- Function: `read_art_buf`.
- Change: convert article body storage to owned string storage before
  removing the in-place `safe_copy`.
- Tests: add wrapped article body coverage first.

#### CSTR-036 - NNTP Protocol Line Compaction

- Files: `libtrn/nntp.cpp`.
- Kind: protocol read buffer compaction.
- Function: `nntp_read_art`.
- Change: keep the protocol line in owned string storage once the NNTP
  read API no longer exposes a caller mutable buffer.
- Tests: run `test_nntp`.

#### CSTR-058 - Header Buffer Storage

- Files: `libtrn/head.cpp`, `libtrn/include/trn/head.h`,
  `libtrn/art.cpp`, `libtrn/artsrch.cpp`, `libtrn/nntp.cpp`,
  `libtrn/scorefile.cpp`.
- Kind: global growable header text buffer.
- Function: storage-centered; main writer is `parse_header`.
- Change: replace `g_head_buf` plus `s_head_buf_size` with owned string
  storage while preserving header offset metadata.  Update direct
  pointer arithmetic users in the same owner slice.
- Tests: header parsing, article display, score-file, and NNTP tests.

#### CSTR-076 - NNTP Server Line Buffer

- Files: `nntp/nntpclient.cpp`, `nntp/nntpinit.cpp`,
  `nntp/include/nntp/nntpclient.h`, `libtrn/nntp.cpp`,
  `inews/inews.cpp`, `nntplist/nntplist.cpp`,
  `trn-artchk/trn-artchk.cpp`.
- Kind: global fixed protocol/status buffer.
- Function: storage-centered `g_ser_line`.
- Change: separate NNTP status text from protocol line input/output so
  callers do not format commands or cache responses through one shared
  `char[NNTP_STRLEN]` buffer.  Include the local `b[NNTP_STRLEN]`
  buffers in `libtrn/nntp.cpp`, the `inet_ntop` logging scratch buffer,
  and legacy non-`INET6` raw-address name storage in `get_tcp_socket` in
  the same protocol owner review.
- Tests: NNTP, inews, nntplist, and trn-artchk tests.

#### CSTR-077 - Article Display Line Buffer

- Files: `libtrn/art.cpp`, `libtrn/include/trn/art.h`,
  `libtrn/respond.cpp`, `libtrn/decode.cpp`, `libtrn/uudecode.cpp`.
- Kind: global fixed article display/input buffer.
- Function: storage-centered `g_art_line`.
- Change: replace article display line storage with owned string or view
  based data flow after local decode/respond/uudecode buffer slices have
  reduced direct mutation.
- Tests: article display, MIME decode, response, and uudecode tests.

#### CSTR-119 - Terminal Command Input Buffers

- Files: `libtrn/terminal.cpp`,
  `libtrn/include/trn/terminal.h`, command-input callers.
- Kind: caller-owned fixed command buffers.
- Function: `get_cmd`, `get_anything`, `in_char`, and related callers.
- Change: replace terminal command input output buffers with owned
  string results after smaller terminal macro and push-string slices are
  complete.  Preserve typeahead, macro expansion, and mouse input
  behavior.
- Tests: terminal input, option editing, and selector tests.

### Tier 5 - Helper Removal

These slices remove helpers only after every direct caller has moved to
owned strings or owner-specific storage.

#### CSTR-048 - Safe Copy Helper Removal

- Files: `util/util2.cpp`, `util/include/util/util2.h`.
- Kind: obsolete bounded C-string copy helper.
- Function: `safe_copy`.
- Change: remove `safe_copy` only after `CSTR-033` and `CSTR-036` have
  removed every production call site.
- Tests: build.
