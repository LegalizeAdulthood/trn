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
`get_a_line`, `fetch_cache`, `file_exp`, and `push_string`.

The useful local targets fall into four groups:

- Read-only labels selected from string literals, then printed.
- Bounded tokens cut out of a larger C string.
- Temporary strings that must be null-terminated before calling legacy
  regex, hash, file, or shell helpers.
- Pointers returned by legacy helpers that are immediately copied into
  `std::string` globals or fields.

Use `std::string_view` only while no callee needs ownership or a
guaranteed terminator.  When a sliced token flows to a C API, build a
local `std::string` and pass `c_str()`.  For `printf` style output, use
`%.*s` with the view length unless the view is known to cover a whole
literal.

## Refactoring Slices

Each slice changes one function.  Add local includes as needed.

1. `libtrn/autosub.cpp`, `match_list`

   Use a `std::string_view` cursor for the remaining pattern list and
   slices split at commas.  Keep the existing local `std::string`
   materialization before `newsgroup_comp`, because that callee needs a
   null-terminated pattern.

2. `libtrn/scorefile.cpp`, `sf_append`

    Use views for `scoretext` and the one-character shortcut check.
    Use `std::string` for filename assembly before calling `file_exp`,
    `make_dir`, and `std::fopen`.  This touches several legacy file and
    scoring helpers, so keep it after the lower-level pattern work.

3. `libtrn/url.cpp`, `parse_url`

    Parse `url` with string views for scheme, host, optional port, and
    path.  Copy into the existing global C buffers only at the end of
    each validated field.  Port parsing can use `std::from_chars` or a
    temporary owned string before `std::atoi`.

4. `libtrn/univ.cpp`, `univ_use_file`

    Use a string view for the input filename and an owned string for the
    effective open name.  Keep `url_get`, `file_exp`, and `std::fopen`
    at the boundary.  This depends on the URL parser slice if URL-backed
    universal files are handled in the same pass.

## Defer

- `libtrn/terminal.cpp`, `print_lines`: the cursor is logically const,
  but it flows through `put_char_adv(char **)`.  Make `put_char_adv`
  const-friendly first.
- `util/util2.cpp`, `file_exp`: it returns a static C buffer and feeds
  `do_interp`.  Convert only with a broader filename-expansion slice.
- `libtrn/util.cpp`, INI parsing helpers: they update caller `char **`
  cursors and write into caller buffers.
- Struct fields in `Article`, `Subject`, `UniversalItem`, score files,
  and termcap storage: those are ownership model changes, not local
  function cleanups.
