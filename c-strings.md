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

On every audit rerun, re-evaluate deferred items against the current
source.  Do not assume older deferrals remain ineligible after code
changes.

Do not keep a chronological history of audit reruns in this document.
Record durable rules, current findings, open slices, and deferrals.
When a slice is completed, remove it from the slice list instead of
moving it into `Findings`.

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
`UniversalItem` unions, `HashDatum` payloads, `parse_string`,
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

OR-01. `libtrn/util.cpp`, `temp_filename`: change the owned
`save_str` return to `std::string`.  Update callers in `datasrc.cpp`,
`scorefile.cpp`, and `univ.cpp` to keep the result as owned string
storage and pass `c_str()` only to immediate C APIs.

### Safe-realloc Array Slices

### Copy/Concat Slices

### Fixed-buffer Storage Slices

FB-01. `libtrn/sw.cpp`, `decode_switch`: replace the four-byte
`tmpbuf2` formatting buffer used for `OI_NEWS_SEL_ORDER` with an owned
`std::string` or `fmt::format` result passed by `c_str()`.

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

### Ubuntu `-Wwrite-strings` Slices

These slices are prepended to remove the current Ubuntu build warnings.
Prefer `std::string_view` or `std::string`.  Use `const char *` only
where a null sentinel or legacy C API makes a view a poor fit.

### Static-Linkage Slices

These slices move top-level functions that are declared in public headers
but referenced only inside their implementation file.  For each slice,
remove the listed declarations from the public header, add file-scope
forward declarations near the top of the implementation file, and make
both declarations and definitions `static`.

### Filesystem Path Slices

## Defer

- Global mutable buffers and cursors such as `g_art_buf`, `g_head_buf`,
  `g_trn_access_mem`, `g_mime_getc_line`, `g_host_name`, and `s_str` in
  `rt-wumpus.cpp` are not string candidates.  Code writes through them
  or treats them as interior pointers.
- `safe_copy` into `g_cmd_buf`, `g_msg`, `g_art_line`, stack arrays, and
  similar buffers is scratch-buffer work, not owned storage.
- Remaining global pointer arrays and pointer-offset storage such as
  `s_tree_lines`, `g_sel_grp_display_mode`, and `g_sel_art_display_mode`
  need ownership-model slices before they can become strings.
- `Subject::m_str` in `libtrn/include/trn/Subject.h` stores hash keys at
  `m_str + 4`, so key lifetime and lookup behavior must change with the
  storage.
- Option saved/default values and selected display mode strings in
  `libtrn/opt.cpp` use arrays of raw pointers and pointer arithmetic
  before freeing display mode strings.
- `libtrn/univ.cpp`, `s_univ_begin_label` and `s_univ_line_desc`: null
  versus empty string is parse state.  Promote only with
  `std::optional<std::string>` or a separate presence flag.
- `nntp/nntpclient.cpp`, `g_last_command`: it is an owned command
  snapshot, but the extern array API crosses NNTP files.
- `libtrn/terminal.cpp`, borrowed termcap strings, mouse-button strings,
  terminal keymap strings, `g_mouse_modes`, and exported size variables:
  these are allocated, union-backed, or rewritten through terminal,
  option, or environment helpers.
- `tool/util3.cpp`, `s_nntp_password`: `read_auth_file` allocates
  through a `char **` output parameter.  Change that helper first.
- `util/env.cpp` stores strings for `putenv`; changing that safely needs
  an owned environment table, not isolated `std::string` locals.
- Short-lived command-list copies in `artsrch.cpp`, `ngsrch.cpp`, and
  `ngstuff.cpp` are local cleanup opportunities, but they do not drive a
  retained-storage migration.
- Pure C-API pass-through filenames such as one-shot `fopen` or `freopen`
  calls are not useful path slices unless the same function also composes,
  normalizes, queries, removes, or renames the file.
- `libtrn/rcstuff.cpp`, newsrc filename fields and backup/rollback
  operations: the fields are already `std::string`, but the remove,
  rename, and `safe_link` calls are a coordinated file-state sequence,
  not isolated one-function path cleanup.
- `libtrn/datasrc.cpp`, `data_source_init`, and
  `nntp/nntpclient.cpp`, `nntp_server_name`: the server name is mostly
  read-only, but it crosses file-reference expansion, nullable server
  state, and `g_ser_line` return storage.  Refactor the API as a
  coordinated const-correct slice.
- Response-file globals, decode part-file state, score-file table
  storage, and remaining filename buffers need coordinated storage
  changes before `std::filesystem::path` is an improvement.
- URL, NNTP, MIME, shell-command, and macro-template strings should stay
  as strings unless the code has first separated the filename part from
  protocol or command text.
- The `IniWords` / `vals` mechanism, including `data_source_init`,
  `prep_ini_words`, `ini_values`, `set_options`, and `g_options_ini`.
  The storage and ownership model needs an overhaul; do not patch it
  with local string buffers or one-function string-literal slices.
- `libtrn/terminal.cpp`, `print_lines`: the cursor is logically const,
  but it flows through `put_char_adv(char **)`.  Make `put_char_adv`
  const-friendly first.
- `libtrn/mempool.cpp`, `mp_save_str`: it has an explicit `nullptr`
  diagnostic path, and pool-owned strings should stay with the pool until
  that lifetime model is intentionally replaced.  Promote only with an
  overload or a broader call-site audit that preserves that behavior.
- `libtrn/include/trn/univ.h`, universal selector strings: the data is
  stored in a union, so `std::string` requires a variant or manual
  lifetime redesign.
- `libtrn/include/trn/scorefile.h`, remaining scorefile table strings:
  `ScoreFileEntry` strings and `ScoreFile::lines` mix `save_str`,
  `mp_save_str`, memory pools, reallocating arrays, and copied entries.
- `libtrn/util.cpp`, INI parsing helpers: they update caller `char **`
  cursors and write into caller buffers.
- `libtrn/sw.cpp`, `decode_switch`: now passes newsgroup patterns to
  view-ready `set_newsgroup_to_do`, but the function is still a command
  cursor parser.  Many switch arms pass interior C strings to option,
  environment, and header parsers.
- `config/string_case_compare.cpp`, length-limited overloads: `len` is
  a comparison limit, not a guaranteed extent for both inputs.  Add
  separate string-view overloads rather than blindly wrapping
  `const char *` plus `len`.
- `nntp/nntpinit.cpp`, `ConnectionFactory`, `server_init`,
  `get_tcp_socket`, and `nntp_connect`: the machine and service strings
  cross the resolver and connection-factory boundary.  Refactor them as
  a coordinated API slice, not as isolated temporary strings.
- `nntp/nntpclient.cpp`, `nntp_at_list_end`: the null pointer sentinel
  is part of the current API and the function updates NNTP command
  state as a side effect.
- `nntp/include/nntp/nntpclient.h`, `INNTPConnection::write`: the pair
  is a byte-buffer transport boundary.  Prefer `std::span` if this
  interface is modernized.
- `libtrn/datasrc.cpp`, `SourceFile::open`, `filename` and `server`:
  both are nullable file/server sentinels.  The completed slice covers
  only the non-stored `fetch_cmd` text.
- `libtrn/rt-select.cpp`, `univ_visit_group`: the implementation only
  forwards to the view-ready `univ_visit_group_main`, but the wrapper
  currently maps `nullptr` to an empty group name.  Promote only with an
  intentional removal of that nullable sentinel.
- `libtrn/rt-page.cpp`, `display_group`: `len` is used for display
  padding while `group` still flows to C string description and output
  helpers.
- `libtrn/scmd.cpp`, `s_finish_cmd`: `nullptr` is an intentional command
  sentinel.  Promote only with a broader command-input API cleanup.
- `libtrn/univ.cpp`, universal file and label loaders: several inputs are
  copied into or assigned through global state.  Keep these out of local
  view-only slices until that ownership model is cleaned up.
- `libtrn/utf.cpp`, byte and visual-width helpers: the `const char *`
  parameters are cursors into encoded text, not whole string extents.
- `libtrn/rt-util.cpp`, `compress_from`: `size` is a display width, not
  the length of `from`.
- `inews/inews.cpp`, `inews_fputs`: this is an output helper, but its
  NNTP branch currently calls `INNTPConnection::write(buff, 0, ec)`.
  Fix and test the intended transport behavior before changing the
  signature.
- `libtrn/trn.cpp`, `set_newsgroup_name`: `nullptr` currently means
  clear the current group.  Promote only with an explicit overload or
  optional-style API that preserves the clearing behavior.
- `libtrn/terminal.cpp`, `save_typeahead`: the buffer plus size are an
  output cursor into caller storage, not a string extent.
- `libtrn/terminal.cpp`, `in_char`: only `prompt` is a view candidate;
  `dflt` still flows into `set_def(char *, const char *)`.  Split this
  only after the default-value path is made view-friendly.
- `config/string_case_compare.cpp` and
  `config/cmake/string_case_compare.*.h.in`: adding string-view overloads
  would remove some local temporary strings, but the manual
  implementation and both generated-header templates must change
  together.
- Remaining struct fields with retained raw string storage are ownership
  model changes, not local function cleanups.
