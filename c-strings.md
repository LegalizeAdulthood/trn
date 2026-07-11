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

After the hash key API promotion, the remaining hash key pointer/length
boundary is the compare callback contract.  The comparator callbacks
only compare bounded keys against strings owned by hash payloads, so
they can be staged toward `std::string_view` without changing hash
entry storage.

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

17. `libtrn/addng.cpp`, `add_newsgroup_cmp`

   Locally derive a key view from the callback `key/key_len` pair and
   compare it against `AddGroup::m_name`.  The payload owns the compared
   string, so no view storage escapes the callback.

18. `libtrn/cache.cpp`, `subject_cmp`

   Locally derive a key view from the callback `key/key_len` pair and
   compare it against the stored subject text after the `"Re: "` prefix.
   Keep the callback signature until all hash comparators are prepared.

19. `libtrn/datasrc.cpp`, `source_file_cmp`

   Locally derive a key view from the callback `key/key_len` pair and
   compare it with the cached source-file line slice.  This keeps the
   `ListNode` storage model unchanged.

20. `libtrn/rt-process.cpp`, `msg_id_cmp`

   Locally derive a key view from the callback `key/key_len` pair.  Use
   the view for both kill-file message IDs and `Article::m_msg_id`
   comparisons, without changing message-ID ownership.

21. `libtrn/rcstuff.cpp`, `rcline_cmp`

   Locally derive a key view from the callback `key/keylen` pair and
   compare it against `NewsgroupData::m_rc_line`.  This prepares the
   newsrc hash comparator while preserving newsrc line storage.

22. `libtrn/hash.cpp`, `hash_find`

   After comparator bodies are view-clean, promote `HashCompareFunc` in
   `libtrn/include/trn/hash.h` to take `std::string_view` and pass the
   existing key view from `hash_find`.  This is a coordinated callback
   API slice: the typedef and comparator declarations must change
   together, but the behavioral center is `hash_find`.

23. `libtrn/rcstuff.cpp`, `find_newsgroup`

   Promote `ngnam` to `std::string_view` in the function and
   `libtrn/include/trn/rcstuff.h`.  It now only forwards the key to
   `hash_fetch`, so callers with `std::string` can stop using `c_str()`.

24. `libtrn/addng.cpp`, `add_to_hash`

   Promote `name` to `std::string_view`.  Use the view size for the
   flexible `AddGroup` allocation, copy the name with `std::memcpy`, and
   pass the view to `hash_store`.  The node owns `m_name`, so no local
   view storage escapes.

25. `libtrn/univ.cpp`, `univ_add_group`

   Promote `grpname` to `std::string_view` in the function and
   `libtrn/include/trn/univ.h`.  Hash lookup can use the view directly;
   build a local `std::string` only at legacy `strcmp` and `save_str`
   boundaries.  Universal items must keep owning copied group names.

26. `libtrn/univ.cpp`, `univ_add_virtual_group`

   Promote `grpname` to `std::string_view` in the function and
   `libtrn/include/trn/univ.h`.  Mirror the owned-string boundary from
   `univ_add_group` while keeping virtual-group storage unchanged.

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
- Struct fields in `Article`, `Subject`, `UniversalItem`, score files,
  and termcap storage: those are ownership model changes, not local
  function cleanups.
