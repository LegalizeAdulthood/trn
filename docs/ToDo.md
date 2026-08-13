# Things To Do

* Look for fixed-size array limits that can be eliminated
* Replace all POSIX filesystem manipulation with std::filesystem
* Validate that all conditional compilation blocks correspond to a CMake
  option
* Eliminate the push/pop macro garbage in the universal selector
* Inline Macro FILE_REF
* Eliminate all MSDOS conditional branches
* Review default setting of MAIL_FILE and entire "checking mail" mechanism
  on Windows
* Review default setting of NEWS_LIB on Windows
* Review default settings of active-file paths on Windows: ACTIVE and
  ACTIVE_TIMES
* Review default settings of news-source paths on Windows: GROUP_DESC,
  SUBSCRIPTIONS, NEWS_SPOOL, OVERVIEW_DIR, OVERVIEW_FMT, EXTRA_INEWS,
  ORG_NAME, POSTING_HOSTNAME, PRIVATE_LIB, and Thread Dir
* Review default settings of account and user-state paths on Windows:
  PASSWORD_FILE, FULLNAMEFILE, TRNDIR, TRNMACRO, RNMACRO, RCNAME,
  LOCKNAME, LASTNAME, SIGNATURE_FILE, and NNTP_AUTH_FILE
* Review default settings of data, article, save, and kill-file paths on
  Windows: GLOBAL_INIT, DEFACCESS, TRNACCESS, NEWSNEWSNAME, MIMECAP,
  OV_FILE_NAME, VARYNAME, HEADNAME, SAVEDIR, SAVENAME, KILL_GLOBAL,
  KILL_LOCAL, and KILL_THREADS
* Review default settings of Unix shell and helper commands on Windows:
  DEFAULT_EDITOR, PREF_SHELL, SH, PIPE_SAVER, SHAR_SAVER, CUSTOM_SAVER,
  VERIFY_RIPEM, VERIFY_PGP, USE_INEWS, CALL_INEWS, NEWS_POSTER,
  MAIL_POSTER, FORWARD_POSTER, BIN_DIR, PAGER, SHELL_START,
  DIFF_PROGRAM, ED_PROGRAM, ISPELL_PROGRAM, ISPELL_OPTIONS, and
  SPELL_PROGRAM
* Review default settings of Unix name and time policies on Windows:
  PASS_NAMES, BERKELEY_NAMES, NAME_TYPE, and LOCALTIME_FMT

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
