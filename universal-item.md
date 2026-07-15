<!-- Copyright (c) 2026, Richard Thomson -->

# Universal Item Refactor Plan

## Goal

Modernize `UniversalItem` now that selector state is no longer encoded in
`UniversalItemType`.  Keep each change small, reviewable, and behavior
preserving.  Remove each slice from this plan when it is complete.

## Current Shape

`UniversalItem` stores payloads in the `UniversalData` variant and derives
its item type from the active payload alternative.  Items are stored in
`g_univ_items`, and traversal uses `univ_items()` and index helpers.

`UniversalItemType` now describes the payload kind only.
`UniversalItemState` tracks normal and deselected selector state.

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

### 32. Remove Transitional Helpers

- Target: temporary payload/type compatibility helpers.
- Change: delete assertions, bridges, and compatibility helpers no longer
  needed after variant and vector storage are complete.
- Data flow: no runtime data flow should remain through transitional APIs.
- Risk: low; this should be mechanical after prior slices.
- Verification: focused universal tests and normal workflow.
