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
- `libtrn/autosub.cpp`, `match_list`: already copies comma-delimited
  patterns into `std::string` before passing them to the regex compiler.

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

The Ubuntu build log for run `29094736532` adds a current
`-Wwrite-strings` cleanup track.  It reports 141 unique warning sites.
The generated `config.h` sites map back to `data_source_init` and
`nntplist.cpp::main`; fix the assigning source, not the generated file.

## Refactoring Slices

Each slice centers on one function.  Add local includes and update the
matching declaration as needed.  The list is ordered from simpler local
helpers toward callers that can pass string views through once lower
helpers accept them.

### Ubuntu `-Wwrite-strings` Slices

These slices are prepended to remove the current Ubuntu build warnings.
Prefer `std::string_view` or `std::string`.  Use `const char *` only
where a null sentinel or legacy C API makes a view a poor fit.

1. `libtrn/datasrc.cpp`, `data_source_init`

   Use owned local `std::string` defaults for `ACTIVE`, `NEWS_SPOOL`,
   `OVERVIEW_DIR`, `OVERVIEW_FMT`, `ACTIVE_TIMES`, and `GROUP_DESC`.
   Store their `data()` pointers in the temporary `vals` array only
   until `new_data_source` copies or expands them.

2. `libtrn/util.cpp`, `prep_ini_words`

   Split `IniWords` help text from parsed value storage.  Make help text
   a `std::string_view` or `const char *` field, and store the allocated
   value vector in a separate field instead of overloading `help_str`.
   This fixes the `g_options_ini` literal table in `opt.cpp`.

3. `libtrn/rt-util.cpp`, `set_spin`

    Promote the static spinner alphabet to `std::string_view`.  The bar
    spinner literal is indexed only by the spinner helpers, so no
    terminator is required.

4. `libtrn/rt-util.cpp`, `perform_status_init`

    After the spinner alphabet is a view, set the progress spinner to
    `"v>^<"` as a view.  Existing index reads in `perform_status` stay
    unchanged.

5. `libtrn/terminal.cpp`, `arrow_macros`

    Make the static arrow macro tables arrays of `std::string_view`.
    `set_macro` already accepts views, so the table entries no longer
    need writable pointer types.

6. `libtrn/terminal.cpp`, `xmouse_check`

    Make `s_mouse_bar_btns` read-only storage and update local scanning
    cursors in `xmouse_check`, `draw_mouse_bar`, and `check_mouse_bar`
    to `const char *` or views.  The mouse bar text is read, not edited.

7. `libtrn/terminal.cpp`, `term_set`

    Promote read-only termcap capability globals such as `g_tc_BC`,
    `g_tc_UC`, `g_tc_VB`, and `g_tc_CR` to const-qualified pointers.
    Keep any synthesized capability in owned storage before assigning
    the pointer.

8. `libtrn/terminal.cpp`, `line_col_calcs`

    After `s_tc_CL` is read-only, assign the non-CRT fallback `"\n\n"`
    without a writable conversion.  The clear-screen string is only read
    by `tputs`.

9. `libtrn/kfile.cpp`, `kill_file_init`

    Split the mutable delimiter pointer from the command-letter text.
    Use a read-only `std::string_view` for the comma fallback and pass
    only its first character to the thread-command lookup.

10. `libtrn/kfile.cpp`, `do_kill_file`

    Apply the same split to the kill-file command cursor.  Keep the
    buffer split mutable, but represent the default `"T,"` command text
    as a view.

11. `libtrn/kfile.cpp`, `edit_kill_file`

    Apply the command-cursor split to the edit path.  The message-id
    line remains mutable; the comma fallback becomes read-only text.

12. `libtrn/scorefile.cpp`, `sf_get_extra_header`

    Return a `std::string_view` for the extra header text.  The only
    caller copies the result into its own lowercase buffer, so the empty
    result can be an empty view instead of a writable literal.

13. `libtrn/search.cpp`, `CompiledRegex::compile`

    Make compile diagnostics read-only.  The null return remains the
    success sentinel, so `const char *` is the smallest safe signature
    change; callers that store the diagnostic should become read-only.

14. `libtrn/util.cpp`, `secs_to_text`

    Promote the result to read-only text.  The dynamic case still uses
    `g_buf`, but the `"never"` and `"missing"` results are literals.
    Update direct callers to stop storing the result in writable locals.

15. `libtrn/cache.cpp`, `fetch_cache`

    Promote the return path to read-only cached text, or add a view
    helper if mutable callers remain.  The two empty-string returns are
    read-only "no header text" results, not buffers to edit.

16. `libtrn/head.cpp`, `prefetch_lines`

    After `fetch_cache` is read-only, split the local `s` variable into
    a read-only source and an owned copy path.  Preserve the existing
    `copy` behavior for callers that request owned storage.

17. `libtrn/intrp.cpp`, `do_interp`

    Split the large substitution variable `s` into read-only source
    text and mutable scratch cursors.  Literal substitutions such as
    `" "` and `"noname"` should be views; only paths that call
    `decode_header`, `strchr` for mutation, or `strcpy` need mutable
    buffers.

### Local Modernization Slices

1. `libtrn/ngsrch.cpp`, `newsgroup_comp`

   Promote `pattern` to `std::string_view` in the implementation and
   `libtrn/include/trn/ngsrch.h`.  Keep the translated regex pattern as
   a local `std::string` and pass `c_str()` only to
   `CompiledRegex::compile`.  Do this before changing callers that split
   or own pattern text.

2. `libtrn/autosub.cpp`, `match_list`

   Promote `pat_list` to `std::string_view`.  The function already uses
   a view cursor and comma-delimited pattern views.  After
   `newsgroup_comp` accepts a view, pass each `pattern_view` directly
   and remove the per-pattern `std::string`.

3. `libtrn/url.cpp`, `parse_url`

   Promote `url` to `std::string_view` in the implementation and
   `libtrn/include/trn/url.h`.  Keep parsing as views.  Use short owned
   diagnostic strings for `printf` output, then copy validated fields
   into the legacy URL buffers.

4. `libtrn/url.cpp`, `url_get`

   Promote only the `url` parameter to `std::string_view`; keep
   `outfile` as `const char *` because it flows to fetch/file helpers.
   This slice depends on the `parse_url` slice and lets callers pass URL
   slices without allocating.

5. `libtrn/univ.cpp`, `univ_use_file`

   Promote `fname` to `std::string_view`.  Keep the effective open name
   as an owned `std::string` for `file_exp` and `std::fopen`.  After
   `url_get` accepts a view, pass `file_name.substr(4)` directly for
   URL-backed universal files and remove the temporary URL string.

6. `libtrn/edit_dist.cpp`, `edit_distn`

   Promote `from/from_len` and `to/to_len` to two
   `std::string_view` parameters in the implementation and
   `libtrn/include/trn/edit_dist.h`.  The dynamic-programming body only
   indexes read-only text ranges.  Treat null legacy inputs as empty
   views at call sites.

7. `libtrn/cache.cpp`, `decode_header`

   Promote only the input pair `from/size` to `std::string_view` in the
   implementation and `libtrn/include/trn/cache.h`.  Keep `to` as a
   mutable output buffer.  This is a good bottom slice for subject
   parsing because encoded-word handling already creates owned
   `std::string` values for bounded substrings.

8. `libtrn/cache.cpp`, `Article::set_subj_line`

   Promote the subject input from `const char *` plus `int size` to
   `std::string_view` in the method and `libtrn/include/trn/Article.h`.
   Keep the local owned `std::string` because the body needs a
   null-terminated, mutable buffer for `subject_has_re` and
   `decode_header`.  Update the handful of callers that currently pass
   `strlen` or a known buffer length.  This is last because it changes a
   class method contract and several parsing call sites.

9. `libtrn/nntp.cpp`, `nntp_list`

   Promote the `arg/len` pair to `std::string_view` in the
   implementation and `libtrn/include/trn/nntp.h`.  Keep `type` as a
   C string for now because it flows to legacy case comparison and
   formatting.  This is a bottom network helper used by active-file and
   overview loading.

10. `libtrn/datasrc.cpp`, `DataSource::find_active_group`

   Promote the `nam/len` pair to `std::string_view` in the method and
   `libtrn/include/trn/datasrc.h`.  Keep `outbuf` mutable.  After
   `nntp_list` accepts a view, pass the group name through without
   rebuilding pointer/length pairs; keep hash calls at their current
   boundary until the hash API is promoted.

11. `libtrn/hash.cpp`, `hash`

   Promote the private `key/keylen` pair to `std::string_view`.  This is
   the bottom of the hash-key chain and has no callers outside
   `hash.cpp`.

12. `libtrn/hash.cpp`, `hash_find`

   Promote the private `key/keylen` pair to `std::string_view`.  Pass
   `key.data()` and `key.size()` only to the existing compare callback.
   This prepares the public hash helpers without changing callback
   ownership yet.

13. `libtrn/hash.cpp`, `hash_fetch`

   Promote the public key pair to `std::string_view` in the
   implementation and `libtrn/include/trn/hash.h`.  This has many
   callers, but most already hold a pointer plus known length.

14. `libtrn/hash.cpp`, `hash_store`

   Promote the public key pair to `std::string_view` in the
   implementation and `libtrn/include/trn/hash.h`.  Keep storing
   `he_key_len` as an integer until the hash entry layout changes.

15. `libtrn/hash.cpp`, `hash_delete`

   Promote the public key pair to `std::string_view` in the
   implementation and `libtrn/include/trn/hash.h`.  Do this after
   `hash_find` and after the fetch/store call sites are compiling.

## Defer

- `libtrn/terminal.cpp`, `print_lines`: the cursor is logically const,
  but it flows through `put_char_adv(char **)`.  Make `put_char_adv`
  const-friendly first.
- `util/util2.cpp`, `file_exp`: it returns a static C buffer and feeds
  `do_interp`.  Convert only with a broader filename-expansion slice.
- `libtrn/util.cpp`, INI parsing helpers: they update caller `char **`
  cursors and write into caller buffers.
- `config/string_case_compare.cpp`, length-limited overloads: `len` is
  a comparison limit, not a guaranteed extent for both inputs.  Add
  separate string-view overloads rather than blindly wrapping
  `const char *` plus `len`.
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
- `libtrn/hash.h`, `HashCompareFunc` and comparator callbacks: these can
  become `std::string_view`, but only as a coordinated hash-callback API
  slice after the public hash helpers are promoted.
- Struct fields in `Article`, `Subject`, `UniversalItem`, score files,
  and termcap storage: those are ownership model changes, not local
  function cleanups.
