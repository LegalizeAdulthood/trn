# Things To Do

* Look for fixed-size array limits that can be eliminated
* Replace all POSIX filesystem manipulation with std::filesystem
* Validate that all conditional compilation blocks correspond to a CMake
  option
* Eliminate the push/pop macro garbage in the universal selector
* Inline Macro FILE_REF
* Eliminate all MSDOS conditional branches

## AddGroup

Restructure add-group storage so `AddGroup` objects live in a
`std::vector<AddGroup>` table addressed by stable indices rather than
linked heap nodes.

* Introduce an `AddGroupIndex` sentinel type for selector cursors,
  page items, and traversal state.
* Replace `g_first_add_group`, `g_last_add_group`, `g_sel_page_gp`,
  `g_sel_next_gp`, and `Selection::gp` with index-based state.
* Keep sorted traversal separate from object identity, for example with
  a vector of indices sorted by the active selector order.
* Replace `m_next` and `m_prev` traversal with index/range helpers.
* Change `new_nntp_groups` and `new_local_groups` to collect candidates
  without the temporary home-grown hash table.
* Use standard containers for de-duplication, such as an
  `std::unordered_map<std::string, AddGroupIndex>` or
  `std::unordered_set<std::string>`, so duplicate candidates do not
  leak discarded `AddGroup` nodes.
* After collection, append unique candidates to the `AddGroup` table and
  update the sorted-index vector instead of walking a temporary hash
  table into a linked list.
