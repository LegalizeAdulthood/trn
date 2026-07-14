<!-- Copyright (c) 2026, Richard Thomson -->

# INI Settings Refactor Plan

## Goal

Replace the destructive INI token stream with immutable document traversal.
`IniDocument` owns the original text and does not rewrite, lower-case, or
append sentinel records to it.  Sections and settings are views into that
text.

Processing INI contents should use range-for loops:

```cpp
for (const IniSection section : document)
{
    for (const IniSetting setting : section)
    {
    }
}
```

## Final Shape

### `IniDocument`

`IniDocument` owns the complete INI text as a `std::string`.

- Construction accepts `std::string` contents and optional source name.
- File reading stays at call sites through `file_contents`.
- The owned text is not modified after construction.
- `begin()` and `end()` enumerate `IniSection` values.
- Iterators scan the owned text and return section views.
- Section views remain valid only while the owning document is alive.
- Section views are invalidated when the owning document is moved.

`IniDocument` is not a writer.  It does not need mutable settings, dynamic
section insertion, deletion, formatting preservation, or write-back.

### `IniSection`

`IniSection` is a view into an `IniDocument`.

- `name()` returns a `std::string_view`.
- `condition()` returns a `std::string_view`.
- `has_condition()` returns whether `condition()` is non-empty.
- `begin()` and `end()` enumerate `IniSetting` values in the section body.

Conditions are section-level guards.  They are used by options,
data-source, and access group parsing.  Settings do not have conditions.

### `IniSetting`

`IniSetting` is a view into an `IniSection`.

- `name()` returns a `std::string_view` into the document text.
- `value()` returns a normalized `std::string`.

`value()` owns the normalization result because comments, quotes, escapes,
line continuations, and trailing whitespace can be removed or transformed.
The normalized value must not be a view into temporary storage.

Setting names keep the document text unchanged.  Case-insensitive lookup is
the responsibility of `IniSchema`, not `IniDocument`.

### `IniSectionValues`

`IniSectionValues` stores schema-matched values for one parsed section.

- Values must become owned `std::string` objects.
- `c_str()` can continue to return a pointer to the owned string.
- `value()` can return an optional string view into owned storage.

Borrowed value views are no longer valid once `IniSetting::value()` returns
an owned normalized string.

## Refactoring Rules

- Add tests for uncovered current behavior before refactoring it.
- Run newly added tests before the refactor slice.
- Preserve section conditions.
- Preserve current value normalization behavior.
- Use user documentation as the first guide for file syntax.
- If documented behavior is sketchy or unclear, ask for the intended
  behavior instead of preserving incidental tokenizer side effects by
  default.
- Prefer range-for loops over explicit `next_section` style iteration.
- Do not add mutable document or setting APIs without a real caller.
- Do not introduce new raw C string ownership.
- Do not add static non-const parser state.

## Current Gaps

- `IniDocument::prepare` rewrites the document into a NUL-delimited token
  stream.
- `IniDocument::Section` exposes mutable `char *` fields.
- `parse_ini_section` consumes a tokenized `char *` section body.
- `IniSectionValues` borrows value views from the tokenized document.
- `check_ini_cond` takes mutable `char *` condition text.
- Callers use `next_section` instead of range-for loops.

## Implementation Slices

4. Add range-for section iteration to `IniDocument`.

   Implement `begin()` and `end()` using a scanner over the owned text.
   Keep the old `next_section` path only as a temporary compatibility path
   until callers are migrated.

5. Change `IniSectionValues` to own strings.

   Store `std::string` values internally.  Keep the existing lookup API.
   Domain code can still ask for views or C strings while callers migrate.

6. Add a range-based `parse_ini_section` overload.

   Accept `const IniSection &`, iterate settings with range-for, look up
   each setting name through `IniSchema`, and store `setting.value()` in
   `IniSectionValues`.

7. Convert `read_data_sources`.

   Use range-for over `IniDocument`.  Check `section.condition()`, skip
   group sections, parse data-source settings with the range-based
   `parse_ini_section`, and remove local use of legacy section fields.

8. Convert `opt_file`.

   Use range-for over `IniDocument`.  Check section conditions, dispatch by
   `section.name()`, parse options with the range-based
   `parse_ini_section`, and remove local use of legacy section fields.

9. Convert `rcstuff_init_data`.

   Use range-for over `IniDocument`.  Check section conditions, select
   group sections by `section.name()`, parse group settings with the
   range-based `parse_ini_section`, and remove local use of legacy section
   fields.

10. Convert `ngstuff_init`.

    Use range-for over `IniDocument`.  Parse the first section with the
    range-based `parse_ini_section` and remove local use of legacy section
    fields.

11. Change `check_ini_cond`.

    Accept `std::string_view` condition text.  Use local owned storage only
    where interpolation still requires a NUL-terminated C string.

12. Remove the legacy token stream.

    Delete `IniDocument::Section`, `next_section`, `prepare`,
    `find_next_section`, `skip_section_body`, and the `char *`
    `parse_ini_section` overload once no callers remain.
