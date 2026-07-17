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

The current scan covers production code under `config`, `libtrn`,
`util`, and the directly used NNTP line helper.  It does not include
tests, generated files, or the vendored `vcpkg` tree.

- `save_str`: no production hits remain in the current tree.
- `safe_copy`: six hits remain, including the helper declaration and
  definition.  The four call sites are inventoried below.
- `safe_malloc`: twenty-six production hits remain in the current
  library scan.  String-like local owners are `initialize`, `rc_to_bits`,
  `tree_puts`, `save_options`, `parse_mouse_buttons`, `get_a_line`,
  `g_head_buf`, and `g_art_buf`.  Non-string owners include hash tables,
  selector page storage, regex bytecode, HTML block arrays, and pointer
  arrays.
- `safe_realloc`: seven production hits remain.  String-like owners are
  `get_a_line`, the NNTP inline line reader, `g_head_buf`, and
  `g_art_buf`.  Regex bytecode remains a non-string owner.
- C string library calls: the current scan finds active `str*`,
  `sprintf`, `fgets`/`fputs`, `printf`/`fprintf`, and character `mem*`
  roots.  These calls are counted below and mapped to slices by owner.
- Fixed buffers: remaining candidates include local display strings,
  interpolation scratch storage, selector search text, message and
  command globals, header/body caches, and line readers.  Protocol byte
  buffers, lookup tables, termcap storage, and caller output buffers stay
  with their owning API slices.
- Filename storage: current path candidates are concentrated in
  `decode_piece`, KILL-file editing/appending, option saving, score-file
  loading, and functions that still compose filenames through `g_buf` or
  `g_cmd_buf`.

## Current `safe_copy` Inventory

The current tree has six `safe_copy` hits: the helper definition, the
helper declaration, and four call sites.  The call sites are still audit
roots.  Keep each one visible until the owning storage or API changes.

- `libtrn/artio.cpp`, `read_art_buf`: compacts a mutable article buffer
  during word wrapping.  See `CSTR-033`.
- `libtrn/charsubst.cpp`, `str_char_subst`: the buffer overload remains
  for `compress_subj` while it builds its result in `g_buf`.  See
  `CSTR-031`.
- `libtrn/nntp.cpp`, `nntp_read_art`: compacts an NNTP protocol line.
  See `CSTR-036`.
- `libtrn/rt-util.cpp`, `compress_subj`: compresses subject display text
  in `g_buf`.  See `CSTR-031`.

## Current C String Function Inventory

The current scan covers `libtrn`, `util`, and `config` source and public
headers.  Counts below include direct `std::` calls and unqualified C
calls in production code.

- Copy and concatenation: `strcpy` 80, `strncpy` 5, `strcat` 6.
- Comparison: `strcmp` 11, `strncmp` 24.
- Search and length: `strchr` 98, `strrchr` 9, `strstr` 2,
  `strlen` 107.
- Formatting into C buffers: `sprintf` 97.
- C text I/O roots: `fgets` 33, `fputs` 194, `printf` 471,
  `fprintf` 36.
- Character byte operations: `memcpy` 5, `memset` 6, `memcmp` 1.

The scan found no current production hits for `strncat`, `strspn`,
`strcspn`, `strpbrk`, `strtok`, `snprintf`, `vsprintf`, `vsnprintf`,
`memmove`, or `memchr`.

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

#### CSTR-038 - Initialization Scratch Buffer

- Files: `libtrn/init.cpp`.
- Kind: local fixed scratch buffer allocated with `safe_malloc`.
- Function: `initialize`.
- Change: replace the heap-allocated `tcbuf` with local fixed storage
  such as `std::array<char, TCBUF_SIZE>`, then pass `.data()` to the
  existing initialization APIs.  Remove the matching `std::free`.
- Tests: build and startup-oriented tests.

#### CSTR-039 - Newsrc Bitmap Scratch Line

- Files: `libtrn/bits.cpp`.
- Kind: local mutable scratch copy.
- Function: `rc_to_bits`.
- Change: replace the `g_buf`/`safe_malloc` split with an owned
  `std::string` that reserves the old line-buffer size, appends the
  trailing comma, and uses `data()` only for local range parsing.
- Tests: `test_bits`.

#### CSTR-040 - Tree Header Line Copy

- Files: `libtrn/rt-wumpus.cpp`.
- Kind: local owned line copy.
- Function: `tree_puts`.
- Change: replace `safe_malloc(len + 2)` and the matching `free` with
  owned `std::string` storage reserved to `len + 2`.  Keep mutable
  parsing local and do not let `data()` escape.
- Tests: `test_rt-wumpus`.

#### CSTR-041 - Article End Prompt Text

- Files: `libtrn/art.cpp`.
- Kind: function-local static fixed buffer.
- Function: `do_article`.
- Change: assign `g_prompt` from `fmt::format` instead of formatting into
  static `prompt_buf[64]`.
- Tests: article display/interpolator tests.

#### CSTR-042 - Selector Search Text

- Files: `libtrn/scmd.cpp`.
- Kind: file-scope fixed string buffer.
- Functions: `scmd_match_description_for_test`, `s_match_description`,
  and `s_search`.
- Change: replace `s_search_text[LINE_BUF_LEN]` with `std::string`.
  Preserve lower-casing and remove the arbitrary truncation unless tests
  prove the limit is meaningful.
- Tests: `test_scmd`.

#### CSTR-043 - Selector Order String

- Files: `libtrn/rt-page.cpp`, `libtrn/include/trn/rt-page.h`,
  `libtrn/opt.cpp`.
- Kind: borrowed static-buffer return through `g_buf`.
- Function: `get_sel_order`.
- Change: return `std::string` from `get_sel_order` and build the value
  with `fmt::format` or simple string append.  Callers already return
  owned `std::string` from `option_value`.
- Tests: `test_sw`.

#### CSTR-044 - Option Header Lists

- Files: `libtrn/opt.cpp`.
- Kind: borrowed static-buffer return through `g_buf`.
- Functions: `hidden_list`, `magic_list`.
- Change: return `std::string` and append directly to owned storage
  instead of formatting into `g_buf + strlen(g_buf)`.
- Tests: option value tests.

### Tier 1 - Helper And Parser Foundations

These slices change lower-level helper, parser, or storage contracts
that later caller slices can consume directly.

#### CSTR-045 - Host Name Match Storage

- Files: `libtrn/intrp.cpp`, `libtrn/include/trn/intrp.h`,
  `libtrn/Article.cpp`, `libtrn/respond.cpp`.
- Kind: global interior pointer into a function-local static buffer.
- Function: `interp_init`.
- Change: replace `g_host_name` with owned `std::string` containing the
  suffix used for local-post matching.  Replace null checks with
  `empty()` where needed and pass `c_str()` to legacy helpers.
- Tests: article local-author and response tests.

#### CSTR-046 - Character Substitution Buffer Overload

- Files: `libtrn/charsubst.cpp`, `libtrn/include/trn/charsubst.h`,
  `libtrn/rt-util.cpp`, `libtrn/include/trn/rt-util.h`,
  `libtrn/rt-page.cpp`.
- Kind: buffer-output API and global display buffer.
- Function: `compress_subj`.
- Change: make `compress_subj` return `std::string`, use the existing
  `str_char_subst(std::string_view, char_int)` overload, and remove the
  buffer-output `str_char_subst` overload.  Preserve meaningful subject
  truncation to the requested display width.
- Tests: add `compress_subj` coverage before the refactor, then run
  `test_rt-util` and selector display tests.

#### CSTR-047 - Read-Line Ownership Contract

- Files: `libtrn/util.cpp`, `libtrn/include/trn/util.h`,
  `nntp/include/nntp/nntpclient.h`, `libtrn/rcstuff.cpp`,
  `libtrn/rt-ov.cpp`, `libtrn/head.cpp`.
- Kind: owning raw-string return and side-effect length globals.
- Functions: `get_a_line` and `nntp_get_a_line`.
- Change: replace the caller-allocated growable buffer contract with an
  owned string result and explicit length from the returned string.
  Update direct callers in the same slice so no wrapper is left unused.
- Tests: newsrc read tests, overview tests, and header prefetch tests.

#### CSTR-048 - Safe Copy Helper Removal

- Files: `util/util2.cpp`, `util/include/util/util2.h`.
- Kind: obsolete bounded C-string copy helper.
- Function: `safe_copy`.
- Change: remove `safe_copy` only after `CSTR-033`, `CSTR-036`, and
  `CSTR-046` have removed every production call site.
- Tests: build.

### Tier 2 - Owned Storage And Local Callers

These slices use Tier 1 results or replace one owner of string storage.
Finish these before broad global-buffer work.

#### CSTR-049 - Option File Contents Buffer

- Files: `libtrn/opt.cpp`.
- Kind: owned raw file buffer.
- Function: `save_options`.
- Change: replace `safe_malloc`/`safe_free` with owned `std::string`
  file contents, using `data()` only for local parsing.  Consider the
  existing `file_contents` helper if the current text behavior matches.
- Tests: option save tests.

#### CSTR-050 - User Real Name Scratch

- Files: `util/env.cpp`.
- Kind: global scratch buffer and local heap buffer.
- Function: `set_user_name`.
- Change: build `g_real_name` with `std::string` instead of `g_buf`,
  `strcat`, `fgets(g_buf, ...)`, and the Windows `safe_malloc` display
  name path.  Preserve platform feature-guarded behavior.
- Tests: environment tests.

#### CSTR-051 - Posting Host Scratch

- Files: `util/env.cpp`.
- Kind: caller-provided mutable scratch buffer.
- Function: `set_p_host_name`.
- Change: build the local host and posting host in owned strings.  Keep
  platform APIs that require mutable output buffers local to the call and
  remove use of `g_buf` for domain-name composition where possible.
- Tests: environment tests.

#### CSTR-052 - Score File Line Input

- Files: `libtrn/scorefile.cpp`.
- Kind: fixed-size line input already wrapped in `std::string`.
- Function: `sf_open_file`.
- Change: replace the `std::string(LINE_BUF_LEN, '\0')` plus
  `fgets(line.data(), ...)` pattern with normal text-line input.  First
  classify whether the `LINE_BUF_LEN - 4` truncation is meaningful for
  score-file contents.
- Tests: `test_scorefile`.

#### CSTR-053 - Newsgroup Display Subject Line

- Files: `libtrn/ng.cpp`.
- Kind: local fixed buffer and interpolation output.
- Function: `output_subject`.
- Change: replace `tmpbuf[256]` with owned string storage for the
  generated subject line.  If `interp` still requires caller output
  storage, keep a local string buffer alive for the full print call and
  avoid changing operation order.
- Tests: `test_subject`.

### Tier 3 - Workflow Callers

These slices clean up workflows after their helper/storage dependencies
are available.  Keep the listed order inside dependent families.

#### CSTR-054 - Decode Piece Paths And Messages

- Files: `libtrn/decode.cpp`.
- Kind: path/message construction through `g_buf` and `g_msg`.
- Function: `decode_piece`.
- Change: use `fs::path` for part files and the `CT` side file, and use
  `std::string`/`fmt` for status messages.  Remove arbitrary path text
  assembly in `g_buf` and prefer `fs::remove`.
- Tests: MIME decode tests.

#### CSTR-055 - KILL File Editing Paths

- Files: `libtrn/kfile.cpp`.
- Kind: path and shell command construction through `g_buf` and
  `g_cmd_buf`.
- Function: `edit_kill_file`.
- Change: keep the selected KILL filename in `fs::path` or
  `std::string`, use `make_dir` through the path/string value, and build
  the editor command with `fmt::format`.
- Tests: KILL file tests.

#### CSTR-056 - KILL File Append Paths

- Files: `libtrn/kfile.cpp`.
- Kind: path construction through `g_cmd_buf`.
- Function: `kill_file_append`.
- Change: keep the target KILL filename in `fs::path` or `std::string`
  and stop using `g_cmd_buf` as the path owner.
- Tests: KILL file append tests.

#### CSTR-057 - Interpolation Scratch Formatting

- Files: `libtrn/intrp.cpp`.
- Kind: local fixed interpolation scratch buffers.
- Function: `do_interp`.
- Change: replace residual `scrbuf[8192]`, `spfbuf[512]`, and the
  static `%y` `tmpbuf[1024]` with owned `std::string` storage.  Keep
  runtime printf-style formatting isolated and do not let local string
  pointers escape.
- Tests: interpolation tests.

### Tier 4 - Broad Shared Buffers

These slices should wait until earlier tiers have reduced direct callers
and clarified ownership at the edges.

#### CSTR-031 - Global Command And Message Buffers

- Files: `config/common.cpp`, `config/include/config/common.h`, many
  users.
- Kind: global fixed buffers `g_msg`, `g_buf`, and `g_cmd_buf`.
- Function: storage-centered; no single function owns it.
- Change: replace one global buffer at a time with owned string or
  scoped command/message objects.  Start only after local slices above
  have reduced direct writers.
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
