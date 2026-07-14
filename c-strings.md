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

## Findings

Most raw string pointers are not local cleanup targets yet.  They are
owned buffers, caller-owned mutable buffers, struct fields, termcap and
NNTP API boundaries, or cursor outputs such as `char **`.  Examples are
`g_buf`, `g_cmd_buf`, `g_ser_line`, `Article` and `Subject` fields,
`UniversalData` union fields, `HashDatum` payloads, `parse_string`,
`get_a_line` and `push_string`.

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
when an empty string has no valid meaning for the result.  Use
`std::optional<std::string>` only when empty string is valid payload that
must be distinguished from absence.

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

### Owning Raw-string Return Slices

#### `prefetch_lines` Ownership Split

- Files: `libtrn/head.cpp`, `libtrn/include/trn/head.h`.
- Finding: copy and no-copy call sites are split, but the implementation
  still uses a runtime `copy` flag.
- Change: add an owning `std::string` helper for the copy path and make
  the no-copy helper always return borrowed storage.
- Data flow: no-copy remote fallback currently calls
  `save_current_header_line`; remove that ownership ambiguity here.
- Callers: no-copy wrappers feed `fetch_subj`, `fetch_from`,
  `fetch_xref`, article search, and scorefile matching.

#### `fetch_subj_copy` String Return

- Files: `libtrn/include/trn/head.h`, `libtrn/head.cpp`.
- Finding: the copy wrappers still return caller-owned `char *`.
- Change: convert `fetch_subj_copy` and `prefetch_lines_copy` to
  `std::string` after the owning helper exists.
- Data flow: direct owned-copy consumers are `decode_subject` and the
  `%s`/`%S` interpolation path in `do_interp`.

#### `mp_fetch_lines` Pool Path

- Files: `libtrn/head.cpp`.
- Finding: pool-owned header text duplicates the header span extraction
  used by `fetch_lines`.
- Change: keep the memory-pool interface, but build header text through
  the shared span/string helper.
- Data flow: returned storage remains pool-owned.

#### `do_interp` Subject Copy

- Files: `libtrn/intrp.cpp`.
- Finding: `%s` and `%S` hold a `fetch_subj_copy` raw result and free it
  at function exit.
- Change: use local `std::string` storage for the copied subject.
- Data flow: use mutable `data()` only while applying `subject_has_re`
  and the local `- (nf` trimming hack.

#### `read_auth_file` Password

- Files: `util/util2.cpp`, `util/include/util/util2.h`.
- Finding: password contents are returned through allocated `char *`
  storage and freed by callers.
- Change: return `std::string` and use empty string for "not found".
- Data flow: update callers that receive and free the password.

#### `decode_subject` Filename

- Files: `libtrn/decode.cpp`, `libtrn/include/trn/decode.h`,
  `libtrn/respond.cpp`.
- Finding: `decode_subject` keeps a static owned subject buffer and
  returns an interior filename pointer.
- Change: use local `std::string` subject storage and return an owned
  filename string.
- Data flow: empty string means "no filename"; update the two
  `respond.cpp` callers that currently check for `nullptr`.

### Safe-realloc Array Slices

#### Inactive `export_var` Environment Table

- Files: `util/env.cpp`.
- Finding: the inactive branch resizes `char **environ` with
  `safe_realloc`.
- Change: delete the branch or replace it with modern storage.
- Data flow: if kept, use stable owned strings for `putenv`.

### Copy/Concat Slices

#### `univ_page_file` Pager Command

- Files: `libtrn/univ.cpp`, `libtrn/include/trn/univ.h`.
- Finding: pager command construction writes through `sprintf` and
  `strcat` into `g_cmd_buf`.
- Change: accept `std::string_view` for `fname` and build the command
  with `fmt::format`.
- Data flow: pass the formatted command to `do_shell` while owned
  storage is still alive.

#### `UniversalItem::univ_article_desc`

- Files: `libtrn/univ.cpp`, `libtrn/include/trn/univ.h`.
- Finding: article descriptions are returned through static buffers.
- Change: return `std::string`, use ordinary string truncation, and
  build the result with `fmt::format`.
- Data flow: remove static `dbuf`, `sbuf`, and `fbuf` storage.

#### `nntp_xhdr` Commands

- Files: `libtrn/head.cpp`.
- Finding: `XHDR` commands are built with `sprintf` into `g_ser_line`.
- Change: use `fmt::format` or an owned command string sent to
  `nntp_command`.
- Data flow: keep NNTP command storage alive only for the call.

### Fixed-buffer Storage Slices

#### `univ_use_file` Line Buffer

- Files: `libtrn/univ.cpp`.
- Finding: universal selector input uses static `lbuf[LINE_BUF_LEN]`.
- Change: read into owned line storage before calling `univ_do_line`.
- Truncation: current fixed-size truncation appears arbitrary.

#### `Article::get_cached_line` Numeric Buffers

- Files: `libtrn/Article.cpp`, `libtrn/include/trn/Article.h`.
- Finding: `LINES_LINE` and `BYTES_LINE` use static numeric buffers.
- Change: split cached header lines from formatted numeric article
  fields.
- Data flow: callers that need numeric text should receive owned or
  caller-local formatting.

#### Subject Description Buffers

- Files: `libtrn/sadesc.cpp`, `libtrn/include/trn/sadesc.h`.
- Finding: subject and article description helpers use file-scope static
  buffers.
- Change: return owned `std::string` values.
- Data flow: update display callers to consume string results locally.

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

#### `ScoreFileEntry` Text Fields

- Files: `libtrn/include/trn/scorefile.h`, `libtrn/scorefile.cpp`.
- Finding: `ScoreFileEntry::str1` and `str2` retain rule text as raw
  pointers.
- Change: replace retained rule text with owned string storage.
- Data flow: update score matching, printing, include/exclude, and
  cleanup paths together.

#### `ScoreFile::lines`

- Files: `libtrn/include/trn/scorefile.h`, `libtrn/scorefile.cpp`.
- Finding: score-file cache lines are `std::vector<char *>`.
- Change: replace with `std::vector<std::string>`.
- Data flow: update cache fill, read, and cleanup paths.

#### Keymap Macro Strings

- Files: `libtrn/terminal.cpp`.
- Finding: `KeyMap::km_str` owns macro strings with `save_str`/`free`.
- Change: replace macro string entries with owned `std::string` storage.
- Data flow: update keymap union ownership and macro display together.

#### Exported Environment Values

- Files: `util/env.cpp`.
- Finding: `export_var` leaks a `save_str` buffer to keep `putenv`
  storage alive.
- Change: add a stable owned environment-string table.
- Data flow: preserve existing `un_export` and `re_export` semantics.

### Universal Selector Storage Slices

#### Universal Item Description

- Files: `libtrn/include/trn/univ.h`, `libtrn/univ.cpp`.
- Finding: `UniversalItem::m_desc` is owned nullable `char *` storage.
- Change: convert the default description field to nullable owned string
  storage.
- Data flow: update `univ_add`, `univ_close`, null tests, and
  `UniversalItem::univ_article_desc`.

#### Universal Parse Labels

- Files: `libtrn/univ.cpp`.
- Finding: `s_univ_begin_label` and `s_univ_line_desc` are parse-state
  raw pointers.
- Change: use owned optional strings or an explicit presence flag.
- Data flow: update label matching and description defaults in
  `univ_use_file` and `univ_do_line`.

#### Universal Mask Loading

- Files: `libtrn/univ.cpp`, `libtrn/include/trn/univ.h`.
- Finding: `univ_mask_load` receives a mutable mask only because
  `univ_use_group_line` tokenizes by writing terminators.
- Change: accept `std::string_view` and tokenize without mutating the
  caller's buffer.
- Data flow: removes the startup `save_str("*")` allocation.

#### Universal Payload Strings

- Files: `libtrn/include/trn/univ.h`, `libtrn/univ.cpp`,
  `libtrn/rt-select.cpp`.
- Finding: `UniversalData` payload fields retain owned raw strings.
- Change: replace payload string fields with owned C++ string storage.
- Data flow: update `UN_DEBUG1`, group masks, config files, newsgroups,
  virtual articles, virtual groups, and text files together.
- Ownership: update `univ_add_*`, `univ_free_data`, selector readers,
  and hash-key lifetime in the same storage-centered work.

### Static-Linkage Slices

These slices move top-level functions that are declared in public headers
but referenced only inside their implementation file.  For each slice,
remove the listed declarations from the public header, add file-scope
forward declarations near the top of the implementation file, and make
both declarations and definitions `static`.

### Filesystem Path Slices
