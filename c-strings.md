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

## Refactoring Slices

Most slices center on one function.  Add local includes and update the
matching declaration as needed.  The list is ordered from simpler local
helpers toward callers that can pass string views through once lower
helpers accept them.

### Libfmt Formatting Slices

These slices are prepended before more string-building work.  Start with
dependency support, then use fmt only in functions that are formatting
text.  For string building, include only sites that already build an
owned `std::string`.  Direct `printf`/`fprintf` output can move to
`fmt::print`, but C-buffer `sprintf` sites stay with their C-string
buffer slices.

### Copy/Concat Slices

### Fixed-buffer Storage Slices

#### `univ_vg_add_article` Subject and Author Storage

- Files: `libtrn/univ.cpp`.
- Finding: the function fetches borrowed subject and author pointers, then
  stores both values into owned `std::string` fields.  It also copies the
  subject into an unused local `char lbuf[70]`.
- Change: use owned strings from `fetch_subj_copy` and
  `prefetch_lines_copy`, remove the dead local buffer copy, and store the
  owned values directly.
- Data flow: preserve the `<No Author>` fallback.

#### `prefetch_lines` Static Header Buffer

- Files: `libtrn/head.cpp`, `libtrn/include/trn/head.h`, remaining
  callers of `fetch_subj`, `fetch_from`, and `fetch_xref`.
- Finding: after copy-oriented callers are migrated, `prefetch_lines` and
  the inline fetch helpers should only serve cache-prefetch side effects
  or reviewed borrowed-buffer uses.
- Change: shrink or remove the borrowed-buffer API once those remaining
  uses are classified.
- Data flow: preserve callers that intentionally prefetch headers for
  cache population.

#### `secs_to_text` Interval Text

- Files: `libtrn/util.cpp`, `libtrn/include/trn/util.h`,
  `libtrn/opt.cpp`, `libtrn/trn.cpp`.
- Finding: interval display text is formatted into `g_buf` and returned
  as `const char *`.
- Change: return `std::string` and use `fmt::format` for the composed
  text.
- Data flow: literal results such as `never` and `missing` become normal
  string return values.

#### Subject Description Buffers

- Files: `libtrn/sadesc.cpp`, `libtrn/include/trn/sadesc.h`.
- Finding: subject and article description helpers use file-scope static
  buffers.
- Change: return owned `std::string` values.
- Data flow: update display callers to consume string results locally.

#### Character Substitution Status

- Files: `libtrn/charsubst.cpp`, `libtrn/include/trn/charsubst.h`.
- Finding: under `USE_UTF_HACK`, `current_char_subst` formats status
  text into static `show[50]`.
- Change: return `std::string` and use owned formatted text.
- Truncation: preserve the current bounded display text unless tests or
  user guidance say the limit is arbitrary.

#### Easy-score Command Buffer

- Files: `libtrn/score-easy.cpp`, `libtrn/include/trn/score-easy.h`.
- Finding: `s_sc_e_newline` is fixed-size command assembly storage.
- Change: use `std::string` for command construction.
- Data flow: preserve callers that read the current easy-score command.

#### Saved-score Line Buffer

- Files: `libtrn/scoresave.cpp`.
- Finding: `s_line_buf` stores saved-score line construction.
- Change: use owned string construction if the line does not escape as
  an output buffer.
- Data flow: classify output ownership before editing.

#### Mouse Modes Storage

- Files: `libtrn/terminal.cpp`, `libtrn/include/trn/terminal.h`,
  `libtrn/opt.cpp`.
- Finding: `g_mouse_modes` is a fixed global character array.
- Change: use `std::string` storage and direct assignment.
- Data flow: update option handling and terminal readers together.

#### NNTP Last Command Snapshot

- Files: `nntp/nntpclient.cpp`, `nntp/include/nntp/nntpclient.h`.
- Finding: `g_last_command` is fixed global command snapshot storage.
- Change: replace with `std::string`.
- Data flow: update extern users and command logging together.

### Global String Storage Slices

These slices replace owned global or file-scope `char *` storage with
`std::string`.  They are ordered from local storage with no public
declaration toward globals that cross headers or preserve nullable
state.  These slices are storage-centered because the declaration and
all direct assignments must change together.  For `save_str` and
`safe_copy` ownership slices, replace owned `char *` storage, direct
`save_str` assignments, and matching `free` paths with `std::string`
storage.  Use `std::optional<std::string>` or a separate presence flag
when null and empty are distinct states.

#### Selection Display Modes

- Files: `libtrn/opt.cpp`, `libtrn/include/trn/rt-page.h`.
- Finding: display modes depend on `save_str("*...") + 1` and pointer
  decrement before free.
- Change: replace `g_sel_grp_display_mode` and
  `g_sel_art_display_mode` with owned string storage.
- Data flow: update all mode readers and cleanup together.

#### Option Saved and Default Values

- Files: `libtrn/opt.cpp`, `libtrn/include/trn/opt.h`.
- Finding: `g_option_saved_vals` and `g_option_def_vals` are owning raw
  string arrays.
- Change: replace them with vectors of owned optional strings.
- Data flow: update `apply_global_option`, `option_value`, and cleanup
  paths together.

#### Scorefile Abbreviations

- Files: `libtrn/scorefile.cpp`.
- Finding: `s_sf_abbr` owns file abbreviation strings with
  `save_str`/`free`.
- Change: replace the table with an array of optional strings.
- Data flow: update the `file` command path and abbreviation readers.

#### Keymap Macro Strings

- Files: `libtrn/terminal.cpp`.
- Finding: `KeyMap::km_str` owns macro strings with `save_str`/`free`.
- Change: replace macro string entries with owned `std::string` storage.
- Data flow: update keymap union ownership and macro display together.

#### Subject Text Storage

- Files: `libtrn/cache.cpp`, `libtrn/include/trn/Subject.h`,
  subject readers in `libtrn`.
- Finding: `Subject::m_str` owns decoded subject text in heap memory
  allocated by `safe_malloc`; the hash key uses the `m_str + 4`
  interior string.
- Change: replace `m_str` with `std::string` after replacing
  `safe_malloc`/`memset` subject construction with normal C++ object
  construction.
- Data flow: update hash keys and all `m_str + 4` readers in the same
  storage slice.

#### Exported Environment Values

- Files: `util/env.cpp`.
- Finding: `export_var` leaks a `save_str` buffer to keep `putenv`
  storage alive.
- Change: add a stable owned environment-string table.
- Data flow: preserve existing `un_export` and `re_export` semantics.

### Universal Selector Storage Slices

#### Universal Parse Labels

- Files: `libtrn/univ.cpp`.
- Finding: `s_univ_begin_label` and `s_univ_line_desc` are parse-state
  raw pointers.
- Change: use owned optional strings or an explicit presence flag.
- Data flow: update label matching and description defaults in
  `univ_use_file` and `univ_do_line`.

### Static-Linkage Slices

These slices move top-level functions that are declared in public headers
but referenced only inside their implementation file.  For each slice,
remove the listed declarations from the public header, add file-scope
forward declarations near the top of the implementation file, and make
both declarations and definitions `static`.

### Filesystem Path Slices

#### `sf_get_filename` Scorefile Hierarchy Path

- Files: `libtrn/scorefile.cpp`.
- Finding: scorefile paths are assembled with string append and then
  trimmed by searching for `/` and `.`.
- Change: use `fs::path` for the score directory join and keep group
  name slicing separate from path assembly.
- Data flow: return the existing string form only at the boundary used
  by scorefile readers.

#### `sf_edit_file` Scorefile Edit Path

- Files: `libtrn/scorefile.cpp`.
- Finding: scorefile edit paths are built as strings, expanded, passed
  to `make_dir`, and then edited.
- Change: use `fs::path` for local/global scorefile selection and parent
  creation where possible.
- Data flow: keep `edit_file` and `file_exp` boundaries explicit and use
  `string().c_str()` only at those legacy calls.

#### `sc_sv_save_file` Saved-score Replacement

- Files: `libtrn/scoresave.cpp`.
- Finding: saved-score temp filenames are string-concatenated and then
  passed to POSIX `remove` and `rename`.
- Change: use `fs::path`, `fs::remove`, and `fs::rename` for the save
  and replace operation.
- Data flow: preserve the existing temporary file name beside the target
  save file.

#### `SourceFile::open` and `SourceFile::end_append`

- Files: `libtrn/datasrc.cpp`, `libtrn/include/trn/datasrc.h`.
- Finding: source-file paths are passed as `const char *` through open,
  refetch, and timestamp update paths.
- Change: accept `const fs::path &` or `std::filesystem::path` where the
  path is used for filesystem operations.
- Data flow: keep NNTP fetch command text as `std::string_view`; do not
  mix protocol text with filesystem path storage.

#### Newsrc File Rotation

- Files: `libtrn/rcstuff.cpp`, `libtrn/include/trn/rcstuff.h`.
- Finding: newsrc path strings are passed to POSIX `remove` and
  `rename` during save and rollback.
- Change: use `fs::path`, `fs::remove`, and `fs::rename` for the file
  rotation operations.
- Data flow: retain existing `Newsrc` stored path strings until the
  whole structure is ready for path member storage.
