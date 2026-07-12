<!-- Copyright (c) 2026, Richard Thomson -->

# IniWords Refactor Plan

## Final Shape

The final design separates immutable INI schema, parsed INI data, option
editing state, and domain-specific application of values.  No class stores
both schema metadata and parse results.

### `IniField`

Immutable field definition.

- Stores the field id used by the caller.
- Stores the field name as text.
- Stores optional help text for UI prompts.
- Stores whether the row is a real assignment key or a display group.

This replaces the overloaded `hash`, `item`, and `help_str` rows in
`IniWords`.

### `IniSchema`

Immutable schema for one logical section type.

- Stores the section name.
- Stores an ordered list of `IniField` entries.
- Owns prepared lookup data for key name to field id.
- Provides display iteration in file order.
- Provides lookup by parsed assignment key.

The schema does not store parse results.

### `IniDocument`

Owns a tokenized INI buffer.

- Reads or receives raw mutable text.
- Performs the current `prep_ini_data` tokenization.
- Iterates sections and exposes section name, condition, and assignment
  pairs.
- Keeps all borrowed assignment strings valid for the document lifetime.

The existing destructive parser can move here first without changing its
text format.

### `IniSectionValues`

Borrowed parse result for one schema and one section.

- Stores field id to borrowed value pointer or string view.
- Resets per parsed section.
- Has no ownership of values.
- Cannot free assignment text.

This replaces the `char **vals` array hidden behind `words[0].help_str`.

### `OptionCatalog`

Application schema for user options.

- Owns the option `IniSchema`.
- Maps `OptionIndex` values to fields explicitly.
- Exposes display groups for the selector.
- Supplies prompt help text.

This removes the positional dependency between `OptionIndex` gaps and
`g_options_ini` rows.

### `OptionDraft`

Mutable option-selector state.

- Stores user edits as owned strings.
- Tracks selected, saved, and default state.
- Applies accepted edits through `OptionApplier`.
- Discards unaccepted edits without touching parsed file buffers.

This replaces using global INI values as both parsed config input and
selector-owned edit storage.

### `OptionApplier`

Domain logic that interprets option strings.

- Converts strings into current global option state.
- Captures default and saved values.
- Renders current state through `option_value`.

This keeps the switch in `set_option` separate from parsing and UI edit
storage.

### `DataSourceConfig`

Typed representation of one access-file data source section.

- Reads fields from `IniSectionValues`.
- Applies defaults from environment and compiled paths.
- Builds `DataSource` objects.

The `Thread Dir` alignment issue is represented explicitly, even if the
field remains unused.

### `RcGroupConfig`

Typed representation of one `Group N` access-file section.

- Reads `ID`, `Newsrc`, and `Add Groups`.
- Builds `Newsrc` entries.

This keeps rcgroup parsing independent of data-source parsing.

## Rules

- No type punning between `char *` and `char **`.
- No schema object owns parsed assignment values.
- No parsed borrowed value is ever freed.
- Owned edit strings live in `OptionDraft` only.
- Domain enums map explicitly to schema ids.
- Section display rows never participate in assignment lookup.
- The parser may remain destructive until `IniDocument` owns that fact.

## Current Hazards To Retire

- `words[0].help_str` aliases a `char **vals` allocation.
- `words[0].hash` is both prepared length and metadata.
- Parsed config values and selector-owned strings share one global array.
- `rcstuff_init_data` frees `vals` directly instead of unpreparing the
  table.
- `s_datasrc_ini` contains `Thread Dir`, but `DataSourceIniIndex` skips
  that slot.
- `OptionIndex` values depend on invisible table rows and section headers.

## Slices

### Slice 11: Add `IniDocument`

Move tokenized buffer ownership into a class.

- Read file text into `IniDocument`.
- Move `prep_ini_data` behavior into the class.
- Expose section iteration without caller-owned cursor plumbing.
- Keep `next_ini_section` as a compatibility wrapper during migration.

Run parser, options-file, and access-file tests.

### Slice 12: Remove `IniWords`

Delete the compatibility layer once callers are migrated.

- Remove `IniWords`, `prep_ini_words`, `unprep_ini_words`,
  `ini_values`, `ini_value`, and old `parse_ini_section` overloads.
- Remove hidden `char **vals` allocation.
- Remove positional metadata stored in row zero.
- Clean up tests that only existed for compatibility.

Run the normal build and test workflow.
