<!-- Copyright (c) 2026, Richard Thomson -->

# Universal Item Refactor Plan

## Goal

Modernize `UniversalItem` now that selector state is no longer encoded in
`UniversalItemType`.  Keep each change small, reviewable, and behavior
preserving.  Remove each slice from this plan when it is complete.

## Current Shape

`UniversalItem` still stores payloads in the `UniversalData` union and owns
most payload strings through raw `char *` fields.  Items are stored as an
intrusive global doubly linked list rooted at `g_first_univ` and
`g_last_univ`.

`UniversalItemType` now describes the payload kind only.
`UniversalItemState` tracks normal, deselected, and deleted selector state.

## Target Shape

`UniversalItem` should own modern C++ payload storage.  Payload
alternatives should be type-safe, string ownership should be explicit, and
normal traversal should use a standard container instead of list links
embedded in the item.

Selector flags remain flags.  They are independent bookkeeping, not a
replacement for item state.

## Refactoring Rules

- Add tests before refactoring behavior that is not already covered.
- Prefer explicit types instead of `auto` unless the type is repeated.
- Do not add wrappers unless the same slice converts callers to use them.
- Do not introduce hidden global state or function-local static state.
- Preserve behavior before simplifying ownership or traversal.
- Use empty strings instead of optional values when empty has no meaning.
- Avoid changing formatting outside touched lines.

## Implementation Slices

### 15. Add And Use Help Key Payload Accessors

- Target: `UN_HELP_KEY` payload use.
- Change: add typed accessors for the help location payload and replace
  direct `m_data.i` access sites in the same slice.
- Data flow: help location flows into help mode display.
- Risk: low; behavior is localized to help-key entries.
- Verification: focused universal tests and normal workflow.

### 17. Convert News Group Payload Strings

- Target: `UniversalNewsgroup`.
- Change: replace `char *ng` with `std::string ng`.
- Data flow: group names are consumed by matching, lookup, display, and
  paging.
- Risk: medium; do not let local string storage escape.
- Verification: focused universal tests and normal workflow.

### 18. Convert Virtual Group Payload Strings

- Target: `UniversalVirtualGroup`.
- Change: replace `char *ng` with `std::string ng`.
- Data flow: virtual group names are consumed by matching and virtual pass
  expansion.
- Risk: medium; virtual pass must not store pointers into temporary
  strings.
- Verification: focused universal tests and normal workflow.

### 19. Convert Article Payload Strings

- Target: `UniversalVirtualData`.
- Change: replace `char *` fields with `std::string` fields.
- Data flow: article newsgroup, message id, author, and subject feed
  display, read-state checks, and virtual article expansion.
- Risk: high; several fields have old null-versus-empty behavior.
- Verification: focused universal tests and normal workflow.

### 20. Convert Config File Payload Strings

- Target: `UniversalConfigFileData`.
- Change: replace `char *` fields with `std::string` fields.
- Data flow: title, file name, and label feed recursive universal file
  loading.
- Risk: medium; empty label and missing label semantics must be preserved.
- Verification: focused universal tests and normal workflow.

### 21. Convert Group Mask Payload Strings

- Target: `UniversalGroupMaskData`.
- Change: replace `char *` fields with `std::string` fields.
- Data flow: mask title and mask list feed group mask expansion.
- Risk: medium; empty masks must keep current behavior.
- Verification: focused universal tests and normal workflow.

### 22. Convert Text File Payload String

- Target: `UniversalTextFile`.
- Change: replace `char *fname` with `std::string fname`.
- Data flow: text file name flows into file expansion and pager invocation.
- Risk: medium; file name lifetime and expansion behavior are user visible.
- Verification: focused universal tests and normal workflow.

### 23. Convert Debug Payload String

- Target: debug string payload.
- Change: replace the union debug `char *` with an owned `std::string` in
  its eventual payload alternative.
- Data flow: debug string flows only to display and cleanup.
- Risk: low; behavior is isolated.
- Verification: focused universal tests and normal workflow.

### 24. Convert Universal Item Description

- Target: `UniversalItem::m_desc`.
- Change: replace `char *m_desc` with `std::string m_desc`.
- Data flow: descriptions are used by display, article descriptions, and
  several universal item constructors.
- Risk: high; many call sites distinguish missing description from text.
- Verification: focused universal tests and normal workflow.

### 25. Convert Universal Data To Variant

- Target: `UniversalData`.
- Change: replace the union with `std::variant` alternatives.
- Data flow: payload accessors become the only read and write path.
- Risk: high; the temporary invariant is that `m_type` and the active
  variant alternative agree.
- Verification: focused universal tests and normal workflow.

### 26. Derive Type From Variant

- Target: `UniversalItem::m_type`.
- Change: replace direct type storage with a `type()` helper or variant
  predicate.
- Data flow: type checks drive cleanup, paging, display, and virtual pass
  expansion.
- Risk: high; all type switches must keep the same behavior.
- Verification: focused universal tests and normal workflow.

### 27. Convert Universal Iteration Helpers

- Target: traversal in `univ.cpp`.
- Change: add range-style helpers over the current list and convert one
  traversal cluster in `univ.cpp`.
- Data flow: add, close, mask, sort, and virtual pass code all traverse the
  global item collection.
- Risk: medium; traversal order must stay unchanged.
- Verification: focused universal tests and normal workflow.

### 28. Convert Selector Iteration Helpers

- Target: traversal in `rt-page.cpp`.
- Change: convert universal selector traversal clusters to the range
  helpers.
- Data flow: paging, display, selection counts, and navigation consume the
  item collection.
- Risk: high; selector navigation is user visible.
- Verification: focused universal tests and normal workflow.

### 29. Replace Selector Pointer Positions

- Target: `g_sel_page_univ` and `g_sel_next_univ`.
- Change: replace global item pointers with stable indices or handles.
- Data flow: selector paging stores positions across traversal and display.
- Risk: high; invalid handles can break navigation after sorting.
- Verification: focused universal tests and normal workflow.

### 30. Replace Intrusive Item List Storage

- Target: `g_first_univ`, `g_last_univ`, `m_next`, and `m_prev`.
- Change: store universal items in `std::vector<UniversalItem>`.
- Data flow: all universal item creation, traversal, sorting, display, and
  paging use the same collection.
- Risk: high; pointer stability, sorting, and selector position tracking
  must already be solved.
- Verification: focused universal tests and normal workflow.

### 31. Simplify Universal Item Cleanup

- Target: manual payload cleanup.
- Change: remove cleanup paths made obsolete by strings and variant
  payloads.
- Data flow: `univ_close`, deleted virtual groups, and normal teardown all
  rely on automatic storage cleanup.
- Risk: medium; avoid changing lifetime of still-active selected items.
- Verification: focused universal tests and normal workflow.

### 32. Remove Transitional Helpers

- Target: temporary payload/type compatibility helpers.
- Change: delete assertions, bridges, and compatibility helpers no longer
  needed after variant and vector storage are complete.
- Data flow: no runtime data flow should remain through transitional APIs.
- Risk: low; this should be mechanical after prior slices.
- Verification: focused universal tests and normal workflow.
