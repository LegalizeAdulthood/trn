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

## Refactoring Slices

Each slice centers on one function.  Add local includes and update the
matching declaration as needed.  The list is ordered from simpler local
helpers toward callers that can pass string views through once lower
helpers accept them.

### Ubuntu `-Wwrite-strings` Slices

These slices are prepended to remove the current Ubuntu build warnings.
Prefer `std::string_view` or `std::string`.  Use `const char *` only
where a null sentinel or legacy C API makes a view a poor fit.

### Local Modernization Slices

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

- The `IniWords` / `vals` mechanism, including `data_source_init`,
  `prep_ini_words`, `ini_values`, `set_options`, and `g_options_ini`.
  The storage and ownership model needs an overhaul; do not patch it
  with local string buffers or one-function string-literal slices.
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
