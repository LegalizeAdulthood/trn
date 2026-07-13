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

## Audit Criteria

Each rerun should check these scenarios:

- `char *` values that can become `const char *` because the local code
  never writes through the pointer.
- `const char *` values that can become `std::string_view` because the
  local code only reads, slices, compares, or forwards text by extent.
- `char *` storage populated from `save_str` or `safe_copy` that can
  become owned `std::string` storage without pointer escape.
- Filename variables that compose, normalize, query, remove, rename, or
  create paths and can become `std::filesystem::path`.

When `next` finds no remaining slices, rerun the audit against the
current source and look for new opportunities before treating the plan
as empty.

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
simplified by adding `libfmt`; fmt is now available to the core targets.
Current C-style formatting and output usage remains large: this rerun
found 196 `std::sprintf` sites, 2 `std::snprintf` sites, and 597
`std::printf`/`std::fprintf` sites in the source directories.

A current copy/concat pass after removing home-grown `List` storage
found 238 non-test source hits: 137 `std::strcpy`, 30 `std::strcat`, 12
`std::strncpy`, and 59 `safe_copy` hits.  There were no `std::strncat`
hits in the source directories.

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

A follow-up ownership pass over `save_str`, `mp_save_str`, and
`safe_copy` found 74 `save_str` or `mp_save_str` occurrences and 59
`safe_copy` occurrences.  The useful retained-storage candidates share a
simple shape: a raw pointer is assigned from `save_str`, later freed by
the same owner, and exposed mostly through read-only C-string use.

Most `safe_copy` sites are not ownership candidates.  They write scratch
buffers, command buffers, parser cursors, caller-owned output storage, or
`putenv` strings whose lifetime must remain controlled by an environment
table.  `mp_save_str` pool-owned strings should also stay with the pool
until that lifetime model is intentionally replaced.

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

The copy/concat audit classifies hits by destination.  Owned local
construction can become `std::string` or `fmt::format`.  Caller output
buffers, parser compaction, global display buffers, protocol buffers,
static returned storage, and struct-owned C buffers are not local
string/fmt slices.

`libtrn/addng.cpp:418`, `std::strcpy(g_buf, "*")`, is a real hit, but
it writes the global NNTP query buffer used immediately by `nntp_list`.
Converting that one assignment to string/fmt would only add churn; this
belongs with the global-buffer ownership work.

The best copy/concat candidates are local owned construction sites:
`util/util2.cpp::file_exp`, `util/env.cpp::set_p_host_name`,
`libtrn/scoresave.cpp::sc_sv_save_file`, `libtrn/util.cpp::edit_file`,
`libtrn/scorefile.cpp::sf_edit_file`, and the shortcut line construction
in `libtrn/scorefile.cpp::sf_append`.
`libtrn/decode.cpp::decode_mkdir` and
`libtrn/sadesc.cpp::sa_get_desc` are possible only as return-storage API
slices, because they currently return global or static C-string storage.

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
string-building slices for C-buffer `sprintf` sites; convert those when
the C-style string buffer itself is refactored.

After the home-grown `List` storage was removed, the old object-lifetime
blocker disappeared.  `MimeCapEntry` and `DataSource` retained strings
are already owned by `std::string` or `std::optional<std::string>`, so
they are no longer audit targets.  `SourceFile` now owns metadata lines
in `std::vector<std::string>` and appends new lines through
`std::string_view`.

The newly unblocked retained-storage targets were remaining `Article`
cached header strings and `NewsgroupData::m_rc_line`.  `Article` is now
stored in a `std::map<ArticleNum, Article>`, so `Article` objects have
ordinary construction and destruction.  Cached `Article` header strings
now use optional owned string storage, and the message-id hash has an
explicit pending-id wrapper for global thread commands.
`NewsgroupData::m_rc_line` now owns its `.newsrc` line as `std::string`
while preserving the existing hidden-delimiter and offset model.

A rerun after the `.newsrc` line storage slice found no new broad
retained-storage family.  The remaining direct retained raw pointers are
still score-file storage, universal-selector union storage, terminal
capability/keymap storage, search-regex internals, option value arrays,
NNTP protocol globals, and the `Subject::m_str` hash-key layout.

The rerun did find a small bottom-up batch of local `save_str` scratch
copies whose pointers do not escape.  These are good one-function
slices: `terminal.cpp::edit_buf`, `mime.cpp::mime_init`,
`bits.cpp::chase_xref`, `rt-select.cpp::select_option`,
`respond.cpp::reply`, and `respond.cpp::forward`.

`libtrn/rcln.cpp` still contains obsolete C-string field names inside
the inactive `MCHASE` block.  That block does not compile today and
should be removed or overhauled with the old chase mechanism, not patched
as a local string modernization slice.

After the local `save_str` scratch slices were completed, another rerun
found a command-list scratch family.  These copies are short-lived
owned command strings passed to legacy command performers.  They do not
escape the caller, but the performers still take mutable `char *`
parameters, so use local `std::string` storage and pass `data()`.

### Explicit Criteria Rerun

The explicit criteria pass was rerun against the current source after
the `.newsrc` line-storage and home-grown `List` removals.

- `char *` to `const char *`: two new one-function leaf slices were
  found.  `nntp_handle_auth_err` stores authentication strings in local
  variables that are only null-checked and formatted.  `get_tcp_socket`
  stores string-literal failure causes only for `perror`.
- `const char *` to `std::string_view`: no new simple one-function slice
  was found.  Remaining candidates are null sentinels, C API boundaries,
  encoded-text cursors, output-only helpers, command parsers, or helper
  families that must change with their callers.
- `save_str` and `safe_copy` ownership: the only new safe local scratch
  family remains the six `save_str` copies listed below.  Other hits
  write caller buffers, globals, static storage, parser buffers, command
  buffers, score/universal storage, keymaps, or memory-pool storage.
- Filename variables to `std::filesystem::path`: no new one-function
  path slice was found.  Remaining candidates are stored filename
  fields, backup/rollback rename sequences, protocol or shell text,
  global temp-file state, nullable source-file APIs, or one-shot C API
  filenames where `path.string().c_str()` would add noise.

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

### Local Save-String Slices

#### Article Search Command List

In `libtrn/artsrch.cpp`, `art_search` builds a local command list with
several `save_str` assignments.  Replace it with local `std::string`
storage, preserve the `k` to `j` grandfather clause, and pass `data()`
only to legacy performers.

### Copy/Concat Slices

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
- `libtrn/include/trn/scorefile.h`, scorefile table strings: these mix
  `save_str`, `mp_save_str`, memory pools, reallocating arrays, and
  copied entries.
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
