<!-- Copyright (c) 2026, Richard Thomson -->

# Universal Item Refactor Plan

## Goal

Modernize `UniversalItem` now that selector state is no longer encoded in
`UniversalItemType`.  Keep each change small, reviewable, and behavior
preserving.  Remove each slice from this plan when it is complete.

## Current Shape

`UniversalItem` stores payloads in the `UniversalData` variant and derives
its item type from the active payload alternative.  Items are still stored
as an intrusive global doubly linked list rooted at `g_first_univ` and
`g_last_univ`, with new code starting to traverse through `univ_items()`.

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
