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

1. `libtrn/rt-select.cpp`, `switch_dmode`

   Replace local display label `const char *s` with `std::string_view`.
   It flows only to `std::sprintf`; use `%.*s`.  No project callee sees
   the storage.

2. `libtrn/charsubst.cpp`, `current_char_subst`

   In the non-UTF branch, replace the mutable static `const char *show`
   with a local `std::string_view` over the selected literal and return
   `show.data()`.  This is a leaf and also removes hidden mutable local
   state.

3. `libtrn/rt-page.cpp`, `display_option`

   Convert `pre`, `item`, `post`, and `val` locals to string views.
   `item` and `val` still point at existing option storage, while
   `post.substr(len)` replaces pointer arithmetic.  The only string data
   flow is the final `std::printf`.

4. `libtrn/mime.cpp`, `mime_types_match`

   Use `std::string_view` for the MIME pattern and `find('/')` instead
   of `std::strchr`.  Keep calls to `string_case_equal` at the boundary,
   using the existing length overload for wildcard prefixes.

5. `libtrn/mime.cpp`, `find_attr`

   Represent `attr` as a string view and use `attr.size()` instead of
   `std::strlen`.  The scanner still returns a pointer into `str`, so
   this is a local-only helper cleanup with no caller impact.

6. `libtrn/color.cpp`, `color_init`

   After null checks, wrap foreground and background capabilities in
   `std::string_view`, compare them as views, and assign into
   `ColorObj::fg` and `ColorObj::bg`.  The legacy
   `tc_color_capability` return pointer does not escape.

7. `libtrn/ng.cpp`, `ask_memorize`

   Replace `mode_string` and `mode_phrase` with `std::string_view`.
   Use `front()` for the author/subject toggle and `%.*s` for output.
   This is still local, but the labels flow through several prompts and
   help strings, so it is less mechanical than the display helpers.

8. `libtrn/ngsrch.cpp`, `newsgroup_comp`

   Replace the fixed `char ng_pattern[128]` and cursor pair with an
   owned `std::string` built by `push_back`.  Pass `c_str()` only to
   `CompiledRegex::compile`.  This is a bottom helper used by newsgroup
   search, autosubscribe, and restriction logic.

9. `libtrn/autosub.cpp`, `match_list`

   Use a `std::string_view` cursor for the remaining pattern list and
   slices split at commas.  Keep the existing local `std::string`
   materialization before `newsgroup_comp`, because that callee needs a
   null-terminated pattern.

10. `libtrn/scorefile.cpp`, `sf_append`

    Use views for `scoretext` and the one-character shortcut check.
    Use `std::string` for filename assembly before calling `file_exp`,
    `make_dir`, and `std::fopen`.  This touches several legacy file and
    scoring helpers, so keep it after the lower-level pattern work.

11. `libtrn/url.cpp`, `parse_url`

    Parse `url` with string views for scheme, host, optional port, and
    path.  Copy into the existing global C buffers only at the end of
    each validated field.  Port parsing can use `std::from_chars` or a
    temporary owned string before `std::atoi`.

12. `libtrn/univ.cpp`, `univ_use_file`

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
