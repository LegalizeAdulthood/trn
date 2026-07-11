<!-- Copyright (c) 2026, Richard Thomson -->

# C String Audit

## Scope

Audited project C and C++ sources under the source root, excluding the
vendored `vcpkg` tree.  The audit looked for local raw C string pointers
that can become `std::string_view` or `std::string` without changing
function boundaries.

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

After the hash key and comparator API promotions, the hash table itself
is no longer the limiting boundary.  The rerun found callers that still
accept C strings only to create bounded views, call promoted lookup
helpers, or build temporary command strings for legacy formatting.

The strongest new opportunities are:

- Local pointer-plus-length pairs where the pair already denotes a
  string extent.
- Function parameters that flow only to `std::string_view` callees,
  hash lookups, or owned `std::string` assignments.
- NNTP command builders that can accept views after making local
  null-terminated strings for `sprintf`, diagnostics, or transport.

The current rerun, after completing slices 10 and 11, found another
bottom-up batch: leaf copy helpers, bounded token helpers, display
helpers, and wrappers that already copy into `std::string` or heap
storage.  The remaining `std::string_view{ptr, len}` hits are still
mutable source buffers, cache append boundaries, or already-deferred
ownership changes.

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

SL-22. `libtrn/sacmd.cpp`: move declarations from
    `libtrn/include/trn/sacmd.h`.
    Functions: `sa_art_cmd`, `sa_art_cmd_prim`, `sa_extract_start`,
    `sa_wrap_next_author`.

SL-23. `libtrn/samain.cpp`: move declarations from
    `libtrn/include/trn/samain.h`.
    Functions: `sa_add_ent`, `sa_clean_ents`, `sa_init_arts`,
    `sa_init_context`, `sa_init_ents`, `sa_init_mode`.

SL-24. `libtrn/sathread.cpp`: move declarations from
    `libtrn/include/trn/sathread.h`.
    Functions: `sa_get_subj_thread`.

SL-25. `libtrn/scan.cpp`: move declarations from
    `libtrn/include/trn/scan.h`.
    Functions: `s_clean_contexts`, `s_init_context`.

SL-26. `libtrn/scmd.cpp`: move declarations from
    `libtrn/include/trn/scmd.h`.
    Functions: `s_backward_search`, `s_do_cmd`, `s_forward_search`,
    `s_jump_num`, `s_look_ahead`, `s_match_description`, `s_search`.

SL-27. `libtrn/score.cpp`: move declarations from
    `libtrn/include/trn/score.h`.
    Functions: `sc_rescore_arts`, `sc_score_art_basic`.

SL-28. `libtrn/scorefile.cpp`: move declarations from
    `libtrn/include/trn/scorefile.h`.
    Functions: `score_match`, `sf_add_extra_header`,
    `sf_check_extra_headers`, `sf_cmd_fname`, `sf_do_command`,
    `sf_do_file`, `sf_do_line`, `sf_exclude_file`, `sf_freeform`,
    `sf_get_filename`, `sf_get_line`, `sf_grow`, `sf_missing_score`,
    `sf_print_match`.

SL-29. `libtrn/scoresave.cpp`: move declarations from
    `libtrn/include/trn/scoresave.h`.
    Functions: `sc_sv_add`, `sc_sv_del_group`, `sc_sv_get_file`,
    `sc_sv_make_line`, `sc_sv_use_line`.

SL-30. `libtrn/sdisp.cpp`: move declarations from
    `libtrn/include/trn/sdisp.h`.
    Functions: `s_ref_entry`, `s_refresh_bot`, `s_refresh_description`,
    `s_refresh_ent_zone`, `s_refresh_status`, `s_refresh_top`.

SL-31. `libtrn/sorder.cpp`: move declarations from
    `libtrn/include/trn/sorder.h`.
    Functions: `s_sort_basic`.

SL-32. `libtrn/spage.cpp`: move declarations from
    `libtrn/include/trn/spage.h`.
    Functions: `s_clean_page`.

SL-33. `libtrn/sw.cpp`: move declarations from
    `libtrn/include/trn/sw.h`.
    Functions: `save_init_environment`.

SL-34. `libtrn/terminal.cpp`: move declarations from
    `libtrn/include/trn/terminal.h`.
    Functions: `alarm_catcher`, `circfill`, `edit_buf`, `reprint`,
    `xmouse_on`.

SL-35. `libtrn/univ.cpp`: move declarations from
    `libtrn/include/trn/univ.h`.
    Functions: `univ_add`, `univ_add_debug`, `univ_add_file`,
    `univ_add_group`, `univ_add_mask`, `univ_add_text`,
    `univ_add_text_file`, `univ_add_virt_num`,
    `univ_add_virtual_group`, `univ_open`, `univ_use_group_line`,
    `univ_use_pattern`.

SL-36. `libtrn/url.cpp`: move declarations from
    `libtrn/include/trn/url.h`.
    Functions: `fetch_ftp`, `fetch_http`, `parse_url`.

SL-37. `nntp/nntpinit.cpp`: move declarations from
    `nntp/include/nntp/nntpinit.h`.
    Functions: `get_tcp_socket`.

### Local Modernization Slices

17. `libtrn/color.cpp`, `color_string`: promote `str` to
    `std::string_view`.  Strip a trailing newline by view, use `fwrite`
    for normal color output, and create a local `std::string` only for
    `under_print`.

18. `libtrn/autosub.cpp`, `match_list`: promote `s` to
    `std::string_view`.  Keep pattern slicing as views and create a
    local `std::string` only for `CompiledRegex::execute`.

19. `libtrn/autosub.cpp`, `auto_subscribe`: promote `name` to
    `std::string_view`.  Pass the view to `match_list`; environment
    values remain C strings at the boundary.

20. `libtrn/opt.cpp`, `set_header`: promote `s` to
    `std::string_view`.  Use view length for header prefix comparisons
    and copy into `g_user_header_type[i].name` when saving a user
    header.

21. `libtrn/opt.cpp`, `set_header_list`: promote `str` to
    `std::string_view`.  Build one mutable `std::string` for comma
    tokenization and pass tokens to `set_header`.

22. `libtrn/opt.cpp`, `quote_string`: promote `val` to
    `std::string_view`.  Build the static return buffer from the view in
    both quoted and unquoted cases so returned text stays
    null-terminated.

## Defer

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
- `libtrn/datasrc.cpp`, `SourceFile::append`: `bp` is mutable line
  storage and the key length is an interior slice used before the line is
  compacted and stored.
- `libtrn/rt-page.cpp`, `display_group`: `len` is used for display
  padding while `group` still flows to C string description and output
  helpers.
- `libtrn/rt-util.cpp`, `compress_from`: `size` is a display width, not
  the length of `from`.
- Struct fields in `Article`, `Subject`, `UniversalItem`, score files,
  and termcap storage: those are ownership model changes, not local
  function cleanups.
