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
`g_buf`, `g_cmd_buf`, `g_ser_line`, `Article` and `Subject` fields,
`HashDatum` payloads, `parse_string`, `get_a_line` and `push_string`.
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

`libtrn/rcln.cpp` still contains obsolete C-string field names inside
the inactive `MCHASE` block.  That block does not compile today and
should be removed or overhauled with the old chase mechanism, not patched
as a local string modernization slice.

## Current Audit Summary

- `save_str`: no production hits remain in the current tree.
- `safe_copy`: 26 hits remain, including the helper declaration and
  definition.  The 24 call sites are inventoried below.
- `safe_realloc`: string-like storage remains in `get_a_line` and
  `grow_str`.
  Article, header, and regex buffers are storage/API slices, not local
  one-line changes.
- C string library calls: the current scan finds active `str*`,
  `sprintf`/`snprintf`, `fgets`/`fputs`, `printf`/`fprintf`, and
  character `mem*` roots.  These calls are counted below and mapped to
  slices by owner.
- Fixed buffers: remaining candidates include display lines, command
  text, directory/path storage, static return buffers, and global
  scratch buffers.  Protocol byte buffers, lookup tables, and caller
  output buffers stay with their owning API slices.
- Filename storage: current path candidates are concentrated in
  `decode.cpp`, `ngstuff.cpp`, `opt.cpp`, `respond.cpp`, `url.cpp`, and
  call sites that still round-trip through `file_exp` and mutable
  buffers.

## Current `safe_copy` Inventory

The current tree has 26 `safe_copy` hits: the helper definition, the
helper declaration, and 24 call sites.  The call sites are still audit
roots.  Keep each one visible until the owning storage or API changes.

- `libtrn/art.cpp`, FROM header display: copies to `g_art_line` before
  `extract_name` mutates the display copy.  See `CSTR-026`.
- `libtrn/artio.cpp`, `read_art_buf`: compacts a mutable article buffer
  during word wrapping.  See `CSTR-033`.
- `libtrn/charsubst.cpp`, `str_char_subst`: the buffer overload remains
  for `compress_subj` while it builds its result in `g_buf`.  See
  `CSTR-031`.
- `libtrn/intrp.cpp`, `do_interp`: five scratch-buffer copies remain.
  See `CSTR-029`.
- `libtrn/mime.cpp`, `MimeSection::mime_description`: truncates an
  attachment display line.  See `CSTR-015`.
- `libtrn/ng.cpp`, `output_subject`: truncates subject output through
  `tmpbuf`.  See `CSTR-010`.
- `libtrn/ngstuff.cpp`, `switcheroo`: copies `g_buf` into a macro
  scratch buffer.  See `CSTR-028`.
- `libtrn/nntp.cpp`, `nntp_read_art`: compacts an NNTP protocol line.
  See `CSTR-036`.
- `libtrn/rcln.cpp`, `NewsgroupData::check_expired`: patches
  `m_rc_line` through raw mutable storage.  See `CSTR-011`.
- `libtrn/respond.cpp`, `save_article`: four save, pipe, and command
  copies remain.  See `CSTR-020` and `CSTR-021`.
- `libtrn/rt-util.cpp`, `compress_subj`: compresses subject display text
  in `g_buf`.  See `CSTR-031`.
- `libtrn/sacmd.cpp`, `s_art_cmd`: fakes a save command in `g_buf`.
  See `CSTR-025`.
- `libtrn/url.cpp`, `fetch_ftp`: three static path and identity copies
  remain.  See `CSTR-018`.
## Current C String Function Inventory

The current scan covers `libtrn`, `util`, and `config` source and public
headers.  Counts below include direct `std::` calls and unqualified C
calls in production code.

- Copy and concatenation: `strcpy` 105, `strncpy` 5, `strcat` 13.
- Comparison: `strcmp` 12, `strncmp` 26.
- Search and length: `strchr` 108, `strrchr` 14, `strstr` 2,
  `strlen` 128.
- Formatting into C buffers: `sprintf` 128, `snprintf` 1.
- C text I/O roots: `fgets` 33, `fputs` 198, `printf` 494,
  `fprintf` 37.
- Character byte operations: `memcpy` 7, `memset` 6, `memcmp` 1.

The scan found no current production hits for `strncat`, `strspn`,
`strcspn`, `strpbrk`, `strtok`, `vsprintf`, `vsnprintf`, `memmove`,
or `memchr`.

High-count functions are not self-deferred.  They are grouped into
owner slices below because most calls sit on shared buffers such as
`g_buf`, `g_msg`, `g_cmd_buf`, article storage, terminal storage, and
parser workspaces.

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

### Tier 1 - Helper And Parser Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

### Tier 2 - Owned Storage And Local Callers

These slices use Tier 1 results or replace one owner of string storage.
Finish these before broad global-buffer work.

### CSTR-010 - Subject Output Line

- Files: `libtrn/ng.cpp`.
- Kind: arbitrary fixed display buffer and `safe_copy`.
- Function: `output_subject`.
- Change: use `fmt::format` plus `std::string` for the default subject
  path.  Keep the `g_subj_line` interpolation branch unchanged until
  `interp` can write to owned string storage.
- Tests: add output coverage first; preserve the order of subject lookup
  and interpolation.

### CSTR-011 - Expired Article Newsrc Rewrite

- Files: `libtrn/rcln.cpp`.
- Kind: `std::string` storage patched through raw pointers.
- Function: `NewsgroupData::check_expired`.
- Change: construct the replacement `m_rc_line` with string operations
  instead of `safe_malloc`, `strcpy`, and `safe_copy`.
- Tests: add newsrc rewrite coverage first if not already present.

### CSTR-015 - MIME Description Output

- Files: `libtrn/mime.cpp`, `libtrn/artio.cpp`,
  `libtrn/include/trn/mime.h`.
- Kind: caller output buffer with arbitrary truncation.
- Function: `MimeSection::mime_description`.
- Change: return `std::string` for the description and apply display
  truncation explicitly at the call site if it is meaningful.
- Tests: add attachment-description coverage first if missing.

### CSTR-017 - HTTP Fetch Buffers

- Files: `libtrn/url.cpp`.
- Kind: shared static request and read buffer.
- Function: `fetch_http`.
- Change: use an owned request string for `GET`, and a local byte/string
  buffer for socket reads.  Do not use shared `s_url_buf`.
- Tests: add or run URL fetch tests with a fake connection if available.

### CSTR-018 - FTP Fetch Command Storage

- Files: `libtrn/url.cpp`.
- Kind: static path, user, host, and command buffers.
- Function: `fetch_ftp`.
- Change: use `std::string` for `path`, `username`, `userhost`, and the
  escaped shell command.  Preserve the slash split and validation.
- Tests: build with `USE_FTP` coverage if available.

### CSTR-019 - Decode Piece Directory Storage

- Files: `libtrn/decode.cpp`.
- Kind: static returned path buffer and mutable directory parameter.
- Functions: `decode_mkdir`, `decode_rmdir`, and `decode_piece`.
- Change: make `decode_mkdir` return `std::string` with empty string as
  failure.  Make `decode_rmdir` accept owned or view path text.  Keep the
  string alive in `decode_piece`.
- Tests: add decode-piece directory behavior coverage first if missing.

### CSTR-026 - Article FROM Display Buffer

- Files: `libtrn/art.cpp`, `libtrn/include/trn/art.h`.
- Kind: global display buffer `g_art_line`.
- Function: article display path handling FROM headers.
- Change: use local owned display text for the FROM transformation
  before printing.  Do not mutate shared article text.
- Tests: add article header display coverage first.

### CSTR-028 - Switcheroo Macro Scratch

- Files: `libtrn/ngstuff.cpp`.
- Kind: fixed macro and current-directory buffers.
- Function: `switcheroo`.
- Change: replace the saved current directory with filesystem path
  storage.  Leave `mac_line` output buffer until that API is changed.
- Tests: add switch command directory coverage first.

### CSTR-032 - UUDecode Prescan Message

- Files: `libtrn/uudecode.cpp`, `libtrn/respond.cpp`, and
  `libtrn/include/trn/uudecode.h`.
- Kind: filename output through a `char **` backed by global `g_msg`.
- Functions: `uue_prescan` and its two direct callers.
- Change: write filename metadata to caller-owned `std::string` storage
  instead of copying it into `g_msg`.
- Tests: run UUE prescan coverage before and after.

### CSTR-043 - Active Group Lookup Buffer

- Files: `libtrn/datasrc.cpp`, `libtrn/include/trn/datasrc.h`.
- Kind: caller output buffer using `sprintf`, `fgets`, `strncmp`,
  `strrchr`, and `memcpy`.
- Function: `DataSource::find_active_group`.
- Change: return owned active-line text or fill caller-owned
  `std::string` storage instead of copying through `outbuf`.
- Tests: run `test_datasrc` before and after; add refetch/cache coverage
  for active-line updates if missing.

### CSTR-044 - Group Description Lookup Buffer

- Files: `libtrn/datasrc.cpp`.
- Kind: `g_buf` line construction with `snprintf`, `strcat`, and
  `fputs`.
- Function: `DataSource::find_group_desc`.
- Change: construct the group-description line in `std::string` and
  append/store that string without routing through `g_buf`.
- Tests: run `test_datasrc` before and after.

### CSTR-046 - Close Match Newsgroup Storage

- Files: `libtrn/datasrc.cpp`.
- Kind: `char **` array and C string parsing with `strchr` and
  `strcmp`.
- Functions: `find_close_match`, `check_distance`, and `get_near_miss`.
- Change: replace `s_newsgroup_ptrs` with vector-backed string or
  string-view storage owned by the close-match scan.
- Tests: add close-match coverage first if missing.

### CSTR-048 - Scorefile Line Parser Views

- Files: `libtrn/scorefile.cpp`.
- Kind: line parsing with `strchr`, `strlen`, and in-place terminators.
- Function: `sf_do_line`.
- Change: use views for the score, header, and pattern fields where the
  parser only slices text.  Keep owned strings for values retained in
  `ScoreFileEntry`.
- Tests: run `test_scorefile` before and after.

### CSTR-053 - Article Tree Line Storage

- Files: `libtrn/rt-wumpus.cpp`.
- Kind: static tree buffers and copied line storage.
- Functions: `cache_tree` and `tree_puts`.
- Change: replace `s_tree_buff` and `s_tree_lines` raw allocations with
  owned strings.  Preserve the current visual tree output exactly.
- Tests: add tree rendering coverage first if missing.

### CSTR-054 - Terminal Key And Choice Formatting

- Files: `libtrn/terminal.cpp`.
- Kind: prompt/key formatting through `g_buf`, `g_cmd_buf`, `strcpy`,
  `strcat`, `sprintf`, `strlen`, and `strncmp`.
- Functions: keymap display and choice input helpers.
- Change: refactor one helper at a time to local `std::string` or `fmt`
  output while leaving termcap storage and typeahead buffers alone.
- Tests: add terminal helper coverage first if practical; otherwise run
  the focused terminal tests that exist.

### Tier 3 - Workflow Callers

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

### CSTR-020 - Save Article Extract Branch

- Files: `libtrn/respond.cpp`.
- Kind: local, static, and global command buffers.
- Function: `save_article`.
- Change: refactor only the `cmd == 'e'` branch to use strings for the
  expanded destination, custom extractor command, and directory text.
  Do not store pointers into temporary strings.
- Tests: add save/extract coverage first, with isolated output files.

### CSTR-021 - Save Article Pipe And Normal Save Branches

- Files: `libtrn/respond.cpp`.
- Kind: local destination buffer and `g_cmd_buf` command construction.
- Function: `save_article`.
- Change: refactor the pipe and normal-save branches after `CSTR-020`.
  Keep destination ownership in `std::string` and use `fmt` for command
  construction where formatting remains.
- Tests: add pipe and normal-save coverage first.

### CSTR-025 - Selector Extract Command Handoff

- Files: `libtrn/sacmd.cpp`, `libtrn/respond.cpp`.
- Kind: command text faked in global `g_buf`.
- Function: `s_art_cmd`.
- Change: after `save_article` has a string command entry point, pass
  the extract command text without copying into `g_buf`.
- Tests: add selector extract coverage first.

### CSTR-051 - Selector Status Message Storage

- Files: `libtrn/rt-select.cpp`.
- Kind: `g_msg` and `g_cmd_buf` construction with `strcpy`, `sprintf`,
  `strcat`, and `strlen`.
- Functions: selector status and prompt display helpers.
- Change: replace one selector status helper at a time with
  `std::string` or `fmt::format`, then pass owned text to display
  helpers.  Do not store pointers to temporary string data.
- Tests: add selector status coverage first if missing.

### CSTR-052 - Top-level News Source Display

- Files: `libtrn/trn.cpp`.
- Kind: `g_msg` and `g_buf` construction with `strcpy`, `sprintf`,
  `strcat`, and `strlen`.
- Function: news source information display path.
- Change: build each display paragraph as `std::string` with `fmt`, then
  print it without global scratch buffers.
- Tests: add display-output coverage first if missing.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

### CSTR-029 - Interpolation Scratch Copies

- Files: `libtrn/intrp.cpp`.
- Kind: local and static scratch buffers in one large function.
- Function: `do_interp`.
- Change: split small helper operations out first, then replace the
  five remaining `safe_copy` scratch paths with owned strings or
  string views where no pointer escapes.
- Tests: run `test_interp` before and after each helper extraction.

### CSTR-031 - Global Command And Message Buffers

- Files: `config/common.cpp`, `config/include/config/common.h`, many
  users.
- Kind: global fixed buffers `g_msg`, `g_buf`, and `g_cmd_buf`.
- Function: storage-centered; no single function owns it.
- Change: replace one global buffer at a time with owned string or
  scoped command/message objects.  Start only after local slices above
  have reduced direct writers.
- Tests: broad workflow required.

### CSTR-033 - Article Body Wrap Buffer

- Files: `libtrn/artio.cpp`.
- Kind: mutable article-body buffer compaction.
- Function: `read_art_buf`.
- Change: convert article body storage to owned string storage before
  removing the in-place `safe_copy`.
- Tests: add wrapped article body coverage first.

### CSTR-036 - NNTP Protocol Line Compaction

- Files: `libtrn/nntp.cpp`.
- Kind: protocol read buffer compaction.
- Function: `nntp_read_art`.
- Change: keep the protocol line in owned string storage once the NNTP
  read API no longer exposes a caller mutable buffer.
- Tests: run `test_nntp`.
