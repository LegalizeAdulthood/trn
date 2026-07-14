<!-- Copyright (c) 2026, Richard Thomson -->

# Universal Selector Plan

## Goal

Separate universal item state from universal item type while preserving
current selector behavior. Add focused tests first so the refactor checks
the behavior provided by the old type sentinels.

## Plan

### 5. Refactor universal item state in one slice

- Add:

  ```cpp
  enum UniversalItemState
  {
      UIS_NORMAL,
      UIS_DESELECTED,
      UIS_DELETED
  };
  ```

- Add `UniversalItemState m_state` to `UniversalItem`.
- Remove `UN_GROUP_DESEL`, `UN_VGROUP_DESEL`, and `UN_DELETED`.
- Replace type mutations with state mutations.
- Keep `UniversalItemFlags` as bit flags for selector bookkeeping.
- Avoid early payload free on deleted virtual groups; let normal cleanup
  own the payload.

### 6. Verify

- Run the new focused universal tests before the refactor.
- Run them again after the refactor.
- Run the normal workflow.
