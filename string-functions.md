<!-- This software is copyrighted as detailed in the LICENSE file. -->

# String Function Audit

## Scope

This audit covers project-owned C and C++ source under `C:\code\trn\trn`,
excluding the vendored `vcpkg` tree.  The scan looked for calls where a
string literal reaches a parameter typed as a modifiable C string,
usually `char *`.

The primary finder was Clang with `-Wwritable-strings` for C++ files and
`-Wwrite-strings` for `support/unipatch.c`.  The flagged call sites were
then checked by reading the callee bodies.  The K&R C support file did
not add any literal-to-`char *` argument cases.

Some additional literal-to-`char *` diagnostics are not function calls:
data tables, globals, locals, and functions returning `char *` literals.
Those are listed separately at the end.

## Summary

- The direct function-call issues are concentrated in ten entries.
- Most are old read-only APIs whose parameter should be `const char *`.
- Three calls are only safe because the literal is an empty or simple
  sentinel.
- One external API, POSIX `putenv`, is a storage sink when built with a
  POSIX prototype.

## Read-only APIs Typed as Mutable

### `interp` and `do_interp`

Signatures:

```cpp
char *do_interp(char *dest, int dest_size, char *pattern,
                const char *stoppers, const char *cmd);
char *interp(char *dest, int dest_size, char *pattern);
```

`pattern` is scanned and advanced, but the input buffer is not written.
The return value is a position inside the input pattern, which is why the
old signature returns `char *`.

Direct literal calls:

- `libtrn/artsrch.cpp:207`
- `libtrn/artsrch.cpp:213`
- `libtrn/decode.cpp:547`
- `libtrn/opt.cpp:204`
- `libtrn/opt.cpp:1789`
- `libtrn/respond.cpp:521`
- `libtrn/util.cpp:181`

Source-only or configuration-dependent literal calls:

- `libtrn/cache.cpp:584`, inside `PENDING` and `ARTSEARCH`
- `libtrn/decode.cpp:549`, the non-`MSDOS` branch

Many more calls can receive literal defaults through `get_val`, for
example the response header defaults in `libtrn/respond.cpp`.

Recommendation: make the pattern path const-correct.  The clean shape is
`const char *do_interp(..., const char *pattern, ...)` and
`const char *interp(..., const char *pattern)`.  Callers that need to
keep mutating their original command buffer can keep a separate mutable
pointer.

### `get_val`

Signature:

```cpp
char *get_val(const char *nam, char *def);
```

`def` is never modified.  `get_val` returns either the environment value
or the default pointer.  Literal defaults therefore flow through a
mutable return type.

Literal/default call sites include:

- `libtrn/mime.cpp:99`
- `libtrn/ng.cpp:1860`
- `libtrn/respond.cpp:179`
- `libtrn/respond.cpp:222`
- `libtrn/respond.cpp:283`
- `libtrn/respond.cpp:321`
- `libtrn/respond.cpp:342`
- `libtrn/respond.cpp:366`
- `libtrn/respond.cpp:683`
- `libtrn/respond.cpp:706`
- `libtrn/respond.cpp:756`
- `libtrn/respond.cpp:779`
- `libtrn/respond.cpp:866`
- `libtrn/respond.cpp:883`
- `libtrn/respond.cpp:936`
- `libtrn/respond.cpp:1007`
- `libtrn/respond.cpp:1038`
- `libtrn/respond.cpp:1086`
- `libtrn/respond.cpp:1098`
- `libtrn/rt-page.cpp:456`

Recommendation: prefer the existing `get_val_const` where callers only
read the value.  If mutable environment values must still be supported,
split the API into const and mutable forms.

### `dump_header`

Signature:

```cpp
static void dump_header(char *where);
```

This debug-only helper only prints `where`.

Literal call site:

- `libtrn/head.cpp:244`

Recommendation: change `where` to `const char *`.

### `univ_add_text_file`

Signature:

```cpp
void univ_add_text_file(const char *desc, char *name);
```

`name` is inspected and copied through `file_exp` and `save_str`.  It is
not modified.

Literal call site:

- `libtrn/univ.cpp:723`

Recommendation: change `name` to `const char *`.

## Copy-before-edit APIs

### `set_macro`

Signature:

```cpp
void set_macro(char *seq, char *def);
```

The literal call sites are:

- `libtrn/opt.cpp:416`
- `libtrn/terminal.cpp:564`

`set_macro` reads `seq` and copies it into a local buffer before editing
alternate escape forms.  It forwards `def` to `mac_line` with
`tbsize == 0`; in that mode `mac_line` treats the line as an already
parsed definition.  These literal calls do not write the literals.

The caveat is that `mac_line` normally mutates its `line` argument when
it reads macro-file lines.  `set_macro` is relying on a narrower mode
than the type expresses.

Recommendation: make `set_macro` take `const char *` for both arguments,
then split or wrap the `mac_line` mode that accepts an already parsed
definition.

### `tree_puts`

Signature:

```cpp
ArticleLine tree_puts(char *orig_line, ArticleLine header_line,
                      int is_subject);
```

Literal call site:

- `libtrn/rt-wumpus.cpp:617`

The body intends to copy the line before display edits.  For normal text
that is true.  However, when hiding is enabled it calls
`decode_header(line, orig_line, len)`.  With `USE_UTF_HACK` enabled,
`decode_header` temporarily writes into encoded-word input.

The current literal is `"+"`, so it cannot take the encoded-word branch.
The signature is still accurately mutable for other inputs unless
`decode_header` is made source-const.

Recommendation: either make `decode_header` stop modifying its input or
copy before calling it.  Then `tree_puts` can take `const char *`.

## Sentinel or Ownership APIs

### `do_newsgroup`

Signature:

```cpp
DoNewsgroupResult do_newsgroup(char *start_command);
```

Literal call sites:

- `libtrn/rt-select.cpp:408`
- `libtrn/rt-select.cpp:897`
- `libtrn/rt-select.cpp:998`
- `libtrn/univ.cpp:1172`

Non-empty commands are pushed and then freed.  The empty literal is safe
only because `empty(start_command)` takes a separate path and the
function skips `std::free(start_command)`.

Recommendation: replace the empty-string sentinel with `nullptr`, an
enum, or a small command object that makes ownership explicit.

### `parse_ini_section`

Signature:

```cpp
char *parse_ini_section(char *cp, IniWords words[]);
```

Literal call site:

- `libtrn/rt-select.cpp:750`

This is a real in-place parser.  It lowercases option names and stores
pointers into the parsed buffer.  The empty literal is safe only because
the first check is `if (!*cp) return nullptr;`.

Recommendation: do not pass a literal to this parser.  Add a separate
reset/default path, or pass `nullptr` and handle it explicitly.

### `Article::set_subj_line`

Signature:

```cpp
void Article::set_subj_line(char *subj, int size);
```

Literal call site:

- `libtrn/head.cpp:348`

The function mostly reads `subj` and copies into a new subject buffer.
It also calls `decode_header(new_subj + 4, subj_start, size)`.
With `USE_UTF_HACK` enabled, `decode_header` can temporarily write into
encoded-word input.  The literal `"<NONE>"` is not encoded, so this call
does not currently write to the literal.

Recommendation: pass a mutable local sentinel, or make `decode_header`
source-const so `set_subj_line` can become const-correct.

## External Storage Sink

### `putenv`

On POSIX, `putenv` is declared as taking `char *` and may keep the
pointer as the environment entry.  `util_final` passes string literals:

- `libtrn/util.cpp:91`
- `libtrn/util.cpp:92`
- `libtrn/util.cpp:93`
- `libtrn/util.cpp:94`
- `libtrn/util.cpp:95`
- `libtrn/util.cpp:96`

On the current Windows headers this does not trigger the same const
diagnostic, but the calls are still portability hazards.

Recommendation: clear these names through `export_var`, `_putenv_s` on
Windows, or a small cross-platform wrapper that owns mutable storage.

## Related Non-call Conversions

The compiler scan also found literal conversions that are outside the
requested function-argument scope:

- `libtrn/opt.cpp:60-162` initializes option help fields typed as
  `char *`.
- `libtrn/terminal.cpp:289-307` initializes terminal capability globals
  typed as `char *`.
- `libtrn/cache.cpp:312` and `libtrn/cache.cpp:324` return `""` from a
  function returning `char *`.
- `libtrn/util.cpp:609` and `libtrn/util.cpp:613` return literal text
  from a function returning `char *`.
- `libtrn/search.cpp` and `libtrn/scmd.cpp` assign diagnostic literals
  to local `char *` variables.

These should be handled in the same migration, but they are not cases of
a string literal being passed to a modifiable function parameter.

## Suggested Order

1. Convert the interpolation cursor chain from leaf helper to public
   wrappers: `skip_interp`, `do_interp`, `interp`, and `interp_search`.
2. Convert the remaining pure read-only `get_val` callers after the
   interpolation callers can pass const pattern text through the chain.
3. Convert `set_macro`, which owns its stored text and has no return
   alias.
4. Remove literal sentinels from `do_newsgroup` and
   `parse_ini_section`.
5. Decide whether `decode_header` can stop mutating its source.  That
   unlocks const cleanup in `Article::set_subj_line` and `tree_puts`.
6. Replace the `putenv` literals with an owning environment wrapper.

## Implementation Slices

Each slice below changes one function and its direct callers only.  If a
helper also needs a signature change, it has its own slice.

### Slice 1: `skip_interp`

Change `skip_interp(char *pattern, const char *stoppers)` to take a
const pattern and return a const cursor.  Update only its callers in
`intrp.cpp`.

Return-alias note: the returned cursor points into `pattern`.  Avoid
wrapping temporary `std::string` objects around the call.

Validation: run focused interpolation tests, then build.

### Slice 2: `do_interp`

Change `do_interp` so `pattern` is const and the returned cursor is
const.  Update only direct callers of `do_interp`; use
`std::string_view` internally where it simplifies scanning.

Return-alias note: the return value points into `pattern`.  If a caller
passes `std::string`, that string must outlive the returned cursor.

Validation: run focused interpolation tests, then build.

### Slice 3: `interp`

Change `interp(char *dest, int dest_size, char *pattern)` to take a
const pattern and return a const cursor.  Update its direct callers.

Return-alias note: this wrapper returns the `do_interp` cursor into
`pattern`; it must not return a pointer into temporary owned text.

Validation: run focused interpolation tests, then build.

### Slice 4: `interp_search`

Change `interp_search` to take a const pattern and return a const
cursor.  Update its direct callers.

Return-alias note: this wrapper returns a cursor into its pattern
argument.  Preserve the caller-owned lifetime.

Validation: run focused interpolation tests, then build.

### Slice 5: `get_val`

Change `get_val(const char *nam, char *def)` and its callers so literal
defaults use a const result.  Prefer `get_val_const` at call sites that
only read the selected value; copy into `std::string` or a writable
buffer before mutation.

Return-alias note: the returned pointer aliases either the environment
buffer or `def`.  Do not return a mutable pointer when `def` may be a
literal.

Validation: build with `cmake --build --preset rt-default`.

### Slice 6: `set_macro`

Change `set_macro(char *seq, char *def)` to take
`std::string_view seq` and `std::string_view def`, and update its direct
callers.  Copy into local storage only when generating alternate escape
forms or when storing definitions.

Return-alias note: no input pointer is returned.  Key-map storage must
own copied text, not a view into a caller buffer.

Validation: build with `cmake --build --preset rt-default`.

### Slice 7: `do_newsgroup`

Change `do_newsgroup(char *start_command)` and its direct callers to
remove the `""` sentinel.  Use an explicit no-command state and owned
`std::string` for commands that must be pushed.

Return-alias note: no input pointer is returned, but ownership is
consumed today.  Preserve no-command, empty-command, and command-text as
separate states.

Validation: build with `cmake --build --preset rt-default`.

### Slice 8: `parse_ini_section`

Change `parse_ini_section(char *cp, IniWords words[])` and its direct
callers so `""` is not passed.  Keep real parser input mutable, because
the parser lowercases keys and stores pointers into that buffer.

Return-alias note: the returned pointer aliases `cp`, and `IniWords`
stores pointers into `cp`.  If a caller starts from `std::string`, the
string must outlive all stored pointers.

Validation: build with `cmake --build --preset rt-default`.

### Slice 9: `decode_header`

Change `decode_header(char *to, char *from, int size)` to take a const
source.  Replace temporary source edits with bounded views or local
`std::string` copies, and update direct callers.

Return-alias note: no source pointer is returned.  Views used inside the
function must not outlive the caller-owned source.

Validation: run focused header tests if present, then build.

### Slice 10: `Article::set_subj_line`

After `decode_header` is source-const, change
`Article::set_subj_line(char *subj, int size)` and its direct callers to
take a const source or `std::string_view`.

Return-alias note: no input pointer is returned.  Subject storage remains
owned by the article/subject structures.

Validation: build with `cmake --build --preset rt-default`.

### Slice 11: `tree_puts`

After `decode_header` is source-const, change
`tree_puts(char *orig_line, ArticleLine header_line, int is_subject)` and
its direct callers to take a const source or `std::string_view`.

Return-alias note: no input pointer is returned.  Display work should
continue to happen on local copied buffers.

Validation: build with `cmake --build --preset rt-default`.

### Slice 12: `util_final`

Change `util_final` so it does not pass literals to `putenv`.  Use a
small owned environment-clearing helper inside this function or call a
platform API that does not retain caller storage.

Return-alias note: this is storage aliasing, not return aliasing.  Any
buffer passed to POSIX `putenv` must remain valid after the call.

Validation: build with `cmake --build --preset rt-default`.

### Follow-up: Non-call Literal Conversions

The related table/global/return conversions are outside the "literal
passed to function" scope.  Track those separately, still one function
or data object per slice.
