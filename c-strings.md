<!-- Copyright (c) 2026, Richard Thomson -->

# C String Audit

## Scope

Audited project C and C++ sources under the source root, excluding the
vendored `vcpkg` tree.  The audit looked for local raw C string pointers
and function parameters that can become `std::string_view` or
`std::string` without changing ownership boundaries.

A follow-up pass also looked for local `char name[N]` buffers whose only
job is to hold an owned snapshot or locally formatted text before a
read-only, non-storing API call.

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

The useful local targets fall into four groups:

- Read-only labels selected from string literals, then printed.
- Bounded tokens cut out of a larger C string.
- Read-only pointer plus length pairs where the length is the text
  extent, not an output limit.
- Temporary strings that must be null-terminated before calling legacy
  regex, hash, file, or shell helpers.
- Pointers returned by legacy helpers that are immediately copied into
  `std::string` globals or fields.

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

The Ubuntu build log for run `29094736532` adds a current
`-Wwrite-strings` cleanup track.  It reports 141 unique warning sites.
The generated `config.h` sites map back to assigning source code.  The
`nntplist.cpp::main` cases are local fixes; `data_source_init` belongs
to the deferred INI value-storage overhaul.

After the hash key, comparator, color, autosubscribe, option-header,
quote, NNTP list, MIME, selection-order, `SourceFile::open`
`fetch_cmd`, and `set_newsgroup_to_do` helpers were promoted, a rerun
finds a small safe bottom-up batch.  The new targets are leaf helpers
that compare or print text locally, plus callers that need only local
owned strings before invoking legacy regex or diagnostic APIs.

The MIME cap, autosubscribe, option, NNTP command, universal selector,
and command-line switch call-site checks did not expose more
one-function slices.  Callers pass literals, global buffers, mutable
cursors, nullable sentinels, or values that cross factory, static, or
global storage boundaries.

After completing the header-validation and X mouse suffix slices, another
pointer-oriented rerun did not find a new one-function modernization
slice that meets the current rules.  The remaining pointer candidates
either make the code noisier than the C-string form, require a
coordinated helper/API change, or cross storage, cursor,
nullable-sentinel, output-buffer, or transport boundaries.

The local-buffer pass found a small safe batch.  The useful targets are
owned snapshots of NNTP command/reply text and short formatted strings
that are consumed before the function returns.  Arrays passed to callees
that fill them, static buffers whose address is returned, protocol
scratch buffers, and global display buffers remain out of scope.

A fresh buffer pass, ignoring the old defer list, found more local
buffers that now have safe string-shaped replacements.  The strongest
new candidates build NNTP command text, prompt/default text, lower-case
comparison keys, or a universal-file label split inside one function.
Most remaining buffers are still output buffers, mutable command
cursors, returned static storage, protocol byte buffers, or global
display buffers.  Returned static buffers are better handled as
API-return slices, not as local `std::string` temporaries.

The pass also distinguished meaningful truncation from arbitrary
fixed-buffer truncation.  Meaningful truncation encodes a protocol
extent, screen/display width, file-format field, or caller output-buffer
contract; preserve and test it.  Arbitrary truncation exists only
because the current code used a fixed-size scratch array; when replacing
that array with owned string storage, prefer removing the accidental
limit after tests cover normal current behavior.  Do not add boundary
tests solely to preserve a fixed-buffer artifact.

A formatting pass checked how much construction and output would be
simplified by adding `libfmt`.  The tree does not currently depend on or
use fmt.  Current C-style formatting and output usage is large: about
223 `std::sprintf`/`std::snprintf` sites, 637 `std::printf`/
`std::fprintf` sites, 223 `std::strcpy`/`std::strcat` sites, and 77
`std::strncpy`/`safe_copy` sites in the source directories.

That first count did not include owned string construction already using
`std::string`.  A follow-up scan found 29 `.append()` sites, 9
`std::to_string` sites, and many `+=` hits.  The `+=` count is noisy
because it includes numeric and pointer increments, but real
string-construction examples include NNTP command strings, NNTP error
messages, environment assignments, temp filename suffixes, score
filenames, and universal selector filenames.

`libfmt` would help most where code mixes literals, values, widths, and
numbers into messages or command strings.  It would turn many
`sprintf`/`strlen` append chains into `fmt::format`,
`fmt::format_to`, or `fmt::print`.  It would help less with simple path
append, view append, mutable parser buffers, caller-owned output
buffers, protocol byte buffers, global display buffers, and returned
static storage.  Treat fmt adoption as a separate formatted-output and
formatted-string track, not as a substitute for ownership refactors.

The global-storage pass found owned `char *` variables that can become
`std::string` when callers only need null-terminated read access or
temporary mutable access.  Good candidates store filenames, extracted
paths, server names, or cached return strings.  Rejected globals are
mutable protocol buffers, borrowed cursors, termcap storage, pointer
offset tricks, arrays with parallel ownership state, and output
parameters whose callees allocate through `char **`.

The main future opportunity is the case-insensitive comparison helper
family.  View-ready overloads could remove temporary strings in
`mime_types_match`, `color_rc_attribute`, `nntp_list`, and `set_header`,
but the generated header templates and the manual implementation must be
changed together.  Treat that as a helper-family slice, not isolated
call-site churn.

Do not promote simple output-only helper parameters when the only local
effect is replacing `fputs` or `printf` with `fwrite` or a temporary
`std::string`.  Keep the C-string signature when it is simpler and no
string slicing or ownership improvement results.

The remaining broad hits were rejected because the pointer is a mutable
cursor, a nullable sentinel, an output buffer, a byte transport buffer,
global or static storage, or part of an already-deferred ownership
mechanism.

The filesystem-path pass found candidates where a filename is composed,
suffixed, checked, removed, renamed, or used to create parent directories
inside one function.  The project already builds as C++17 and already uses
`std::filesystem`, so these can be local cleanups when the path does not
escape.

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
string-building slices for C-buffer `sprintf`, `strcpy`, or `strcat`
sites; convert those when the C-style string buffer itself is
refactored.

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
`fmt::print`, but C-buffer `sprintf`, `strcpy`, and `strcat` sites stay
with their C-string buffer slices.

### Global String Storage Slices

These slices replace owned global or file-scope `char *` storage with
`std::string`.  They are ordered from local storage with no public
declaration toward globals that cross headers or preserve nullable
state.  These slices are storage-centered because the declaration and
all direct assignments must change together.

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

### Local Modernization Slices

BUF-06. `libtrn/terminal.cpp`, `arrow_macros`: remove the `lbuf[256]`
copy of each terminal arrow capability.  Pass the literal MSDOS escape
strings or the `Tgetstr` result directly to `set_macro`, which accepts
views and copies the macro definition.

BUF-07. `libtrn/scorefile.cpp`, `sf_check_extra_headers`: replace the
static lower-case `lbuf` with a local `std::string` comparison key.
The truncation is arbitrary: the helper only lowercases a score header
name for lookup, and score-file input is already line-buffer bounded.
Compare the full available header text.

BUF-08. `libtrn/scorefile.cpp`, `sf_add_extra_header`: replace the
static `lbuf` used to append `:` for `set_line_type` with local
`std::string` storage.  Keep the lower-case saved header behavior and
compare against known header names using the full available header
text.  The truncation is arbitrary scratch-buffer space for the appended
colon, not a semantic score-header limit.

BUF-09. `libtrn/scmd.cpp`, `s_match_description`: replace the static
`lbuf` description copy with local string storage, lowercase that
string, and search it with string APIs.  The copied-description
truncation is arbitrary; `trunc=false` means the caller is not asking
for display-width truncation.  The separate `s_search_text` command
buffer limit remains out of this slice.

BUF-10. `libtrn/scorefile.cpp`, `sf_open_file`: replace the URL
scratch `lbuf[1024]` with local owned string storage before calling
`url_get`.  The temporary URL string is consumed in the same function;
the stored score-file name and temporary downloaded filename remain
owned by the existing score-file state.  The URL truncation is
arbitrary fixed scratch space, not a meaningful URL or score-file
limit.

### Filesystem Path Slices

## Defer

- Global mutable buffers and cursors such as `g_art_buf`, `g_head_buf`,
  `g_trn_access_mem`, `g_mime_getc_line`, `g_host_name`, and `s_str` in
  `rt-wumpus.cpp` are not string candidates.  Code writes through them
  or treats them as interior pointers.
- Global pointer arrays and pointer-offset storage such as
  `g_newsgroup_to_do`, `s_tree_lines`, `g_sel_grp_display_mode`, and
  `g_sel_art_display_mode` need ownership-model slices before they can
  become strings.
- `libtrn/univ.cpp`, `s_univ_begin_label` and `s_univ_line_desc`: null
  versus empty string is parse state.  Promote only with
  `std::optional<std::string>` or a separate presence flag.
- `libtrn/terminal.cpp`, termcap strings, mouse-button strings, color
  capability strings, and exported size variables: these are borrowed,
  allocated, or rewritten through terminal, option, or environment
  helpers.
- `tool/util3.cpp`, `s_nntp_password`: `read_auth_file` allocates
  through a `char **` output parameter.  Change that helper first.
- Pure C-API pass-through filenames such as one-shot `fopen` or `freopen`
  calls are not useful path slices unless the same function also composes,
  normalizes, queries, removes, or renames the file.
- `DataSource` filename members, `Newsrc` filename members, response-file
  globals, decode part-file state, and score-file table storage need
  coordinated storage changes before `std::filesystem::path` is an
  improvement.
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
  diagnostic path.  Promote only with an overload or a broader call-site
  audit that preserves that behavior.
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
- `libtrn/datasrc.cpp`, `SourceFile::append`: `bp` is mutable line
  storage and the key length is an interior slice used before the line is
  compacted and stored.
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
- Struct fields in `Article`, `Subject`, `UniversalItem`, score files,
  and termcap storage: those are ownership model changes, not local
  function cleanups.
