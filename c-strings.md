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
`get_a_line`, `file_exp`, and `push_string`.

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

## Refactoring Slices

Each slice centers on one function.  Add local includes and update the
matching declaration as needed.  The list is ordered from simpler local
helpers toward callers that can pass string views through once lower
helpers accept them.

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

### Filesystem Path Slices

56. In `libtrn/util.cpp`, change only `temp_filename` so it builds
    the temporary filename with `std::filesystem::path` from
    `g_tmp_dir` and the generated basename.  Keep the return type and
    ownership unchanged by saving the final path string.
57. In `libtrn/last.cpp`, change only `write_last` so the `.pid` temporary
    filename is derived from a local path and the remove/rename
    operations use `std::filesystem`.  Leave the file-scope
    `s_last_file` storage unchanged.
58. In `libtrn/opt.cpp`, change only `opt_init` so the `%+`
    directory and configured option filename are local paths for
    directory and existence checks.  Keep `g_ini_file` as a string and
    pass a string only to `opt_file`.
59. In `libtrn/scorefile.cpp`, change only `sf_append` so the
    expanded score filename is held as a local path after `file_exp`.
    Use filesystem parent creation for the containing directory, then
    convert to a string for `fopen`.
60. In `libtrn/kfile.cpp`, change only `rewrite_kill_file` so the expanded
    kill-file name is a local path for parent directory creation,
    removal, and file creation.  Keep existing command formatting and
    output text as strings.
61. In `libtrn/kfile.cpp`, change only `write_global_thread_commands`
    so the global thread kill-file path is a local path for directory
    creation, removal, and append/rewrite open logic.  Do not change
    hash walking or the command strings written to the file.
62. In `libtrn/opt.cpp`, change only `save_options` so the primary, `.new`,
    and `.old` option filenames are local paths.  Use filesystem
    remove/rename for the final replacement sequence, while converting
    to strings for the existing low-level `open`, `read`, and `write`
    calls.
63. In `libtrn/univ.cpp`, change only `univ_edit_new_user_file` so the
    `%+/univ/usertop` filename is a local path.  Use filesystem parent
    creation and convert to a string for the existing `fopen` calls; do not
    change the returned universal-file name.
64. In `libtrn/univ.cpp`, change only `univ_add_text_file` so relative text
    file names are appended with `std::filesystem::path` instead of manual
    slash search and string concatenation.  Preserve the current
    behavior for filenames with no directory component.

## Defer

- Pure C-API pass-through filenames such as one-shot `fopen` or `freopen`
  calls are not useful path slices unless the same function also composes,
  normalizes, queries, removes, or renames the file.
- `file_exp` remains a template and interpolation boundary with static
  internal storage.  Wrap its result in a path only when the expanded
  value is consumed locally and no pointer to the local string can
  escape.
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
- `util/util2.cpp`, `file_exp`: it returns a static C buffer and feeds
  `do_interp`.  Convert only with a broader filename-expansion slice.
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
