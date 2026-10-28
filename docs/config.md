<!-- Copyright (c) 2026, Richard Thomson -->

# Remaining Configuration Work

## Generated Data Verification

- Keep `access.def` synced with `DataSourceConfig` and `RcGroupConfig`.
  Test that the generated file names every schema field and preserves
  configured active, spool, overview, description, and auth defaults.
- Test that generated `environment` documents the configured poster,
  saver, pager, editor, shell, and spell-checker defaults.
- Test that generated `newsnews` contains the configured project version
  and current project URL.

## Common-Only Config Library Surface

Scope: this audit excludes declarations whose only job is platform
discovery or platform adaptation: `HAS_*`, `I_*`, `Signal_t`, `MSDOS`,
signal and wait glue, vfork glue, fdio headers, pipe wrappers, and the
generated environment implementation choice. It includes symbols that are
configurable but are not platform probes. These are common because
`common.h` is common, not because they describe the host platform.

### Domain Types

`config/include/config/typedef.h` is pure trn domain API:

- `Number`: strong-type template alias. This is why `config` exposes the
  `strong_type` dependency.
- `NewsgroupNum`, `ArticleNum`, `ArticlePosition`, `ArticleLine`, and
  `ActivePosition`: article, newsgroup, and file-position domain types.
- `newsgroup_after`, `newsgroup_before`, `article_after`,
  `article_before`, `line_after`, and `line_before`: domain arithmetic.
- `ArticleUnread`: unread-count type.
- `MemorySize`, `Uchar`, and `stat_t`: legacy convenience aliases.

`config/include/config/config2.h` also defines common helpers:

- `char_int`: command-character type.
- `Ctl`: control-character helper.
- `file_ref`: absolute-path classifier using
  `std::filesystem::path::is_absolute()`.
- `safe_malloc` and `safe_realloc`: debug-allocator aliases under
  `USE_DEBUGGING_MALLOC`.

### Application Limits

These constants live in `common.h`, but they describe trn data structures
or command protocol, not host capabilities:

- `BITS_PER_BYTE`: search bitmap math.
- `LINE_BUF_LEN` and `CMD_BUF_LEN`: line and command-buffer limits.
- `PUSH_SIZE`: terminal pushback buffer size.
- `MAX_FILENAME`: legacy filename limit; no current consumer found.
- `FINISH_CMD`: command terminator used by command readers.
- `VARY_SIZE`: virtual article-position array window size.
- `TC_SIZE`: termcap string storage size.
- `MIN_DIST`: edit-distance cutoff.

### Feature Policy

These are compile-time product choices, not platform probes:

- `MAIL_CALL`: mail polling.
- `NO_FIREWORKS`: conservative terminal repaint behavior.
- `TILDE_NAME`: tilde-login expansion.
- `MCHASE`: generated xref-unmarking option used by article and newsrc
  logic.
- `VALIDATE_XREF_SITE`: optional xref validation.
- `USE_FTP`: URL ftpgrab support.
- `REPLYTO_POSTER_CHECKING`: posting identity check behavior.
- `DEBUG`: trn debug logging support.

### Runtime Defaults

These generated or fallback macros configure trn behavior, paths, scripts,
or message templates. They are runtime policy, not platform discovery:

- Identity and account policy: `HAS_NEWS_ADMIN`, `NEWS_ADMIN`,
  `ORG_NAME`, `POSTING_HOSTNAME`, `IGNORE_ORG`, `PASS_NAMES`,
  `BERKELEY_NAMES`, `PASSWORD_FILE`, `LOGIN_DIR_FIELD`, and `ROOT_UID`.
- Install and tool defaults: `INSTALL_PREFIX`, `DEFAULT_EDITOR`,
  `PREF_SHELL`, `SH`, `BIN_DIR`, and `PAGER`.
- News-source defaults derived from probes or overrides: `SERVER_NAME`,
  `HAS_LOCAL_SPOOL`, `NEWS_LIB`, `PRIVATE_LIB`, `ACTIVE`,
  `ACTIVE_TIMES`, `GROUP_DESC`, `SUBSCRIPTIONS`, `NEWS_SPOOL`,
  `OVERVIEW_DIR`, `OVERVIEW_FMT`, and `EXTRA_INEWS`.
- User-state files: `TRNDIR`, `TRNMACRO`, `RNMACRO`, `FULLNAMEFILE`,
  `RCNAME`, `LOCKNAME`, `LASTNAME`, `SIGNATURE_FILE`, and
  `NNTP_AUTH_FILE`.
- trn data files: `GLOBAL_INIT`, `DEFACCESS`, `TRNACCESS`,
  `NEWSNEWSNAME`, and `MIMECAP`.
- Article and save paths: `OV_FILE_NAME`, `VARYNAME`, `HEADNAME`,
  `MAIL_FILE`, `SAVEDIR`, and `SAVENAME`.
- UI and formatting defaults: `SELECTION_CHARS`, `UNSUBSCRIBED_CHAR`,
  `MBOX_CHAR`, `LOCALTIME_FMT`, `YOU_SAID`, `ATTRIBUTION`,
  `FORWARD_MSG`, and `FORWARD_MSG_END`.
- Posting and responder templates: `CALL_INEWS`, `NEWS_POSTER`,
  `MAIL_POSTER`, `FORWARD_POSTER`, `MAIL_HEADER`, `NEWS_HEADER`,
  `FORWARD_HEADER`, `CANCEL_HEADER`, and `SUPERSEDE_HEADER`.
- Saver and verifier commands: `PIPE_SAVER`, `SHAR_SAVER`,
  `CUSTOM_SAVER`, `VERIFY_RIPEM`, and `VERIFY_PGP`.
- Kill-file defaults: `KILL_GLOBAL`, `KILL_LOCAL`, and `KILL_THREADS`.
- Startup behavior: `THREAD_INIT` and `SELECT_INIT`.
- Host comparison policy: `POSTING_HOST_MATCH_SEGMENTS`. It is not a
  boolean platform probe; values greater than one are a count.

`config/data/CMakeLists.txt` and `configure_trn.cmake` also own generated
script/data policy that is not platform configuration:

- Script shell and tools: `SHELL_START`, `DIFF_PROGRAM`, `ED_PROGRAM`,
  `ISPELL_PROGRAM`, `ISPELL_OPTIONS`, and `SPELL_PROGRAM`.
- Posting script data: `NAME_TYPE`, `LOCAL_DIST`, `ORGANIZATION_DIST`,
  `CITY_DIST`, `STATE_DIST`, `MULTISTATE_DIST`, `COUNTRY_DIST`,
  `CONTINENT_DIST`, and `POSTING_ORG_NAME`.

### Globals And Utilities

These definitions make `config` carry application state and helper code:

- `g_msg`: global status/message text, defined in `common.cpp`.
- `plural`: plural suffix helper.
- `all_bits`: bit-mask helper.
- `TRN_ASSERT` and `report_assertion`: assertion wrapper and reporting
  implementation. This is why `config` links to `fmt`.
- `g_debug` and `DEB_*`: debug state and debug bit masks, defined by
  `common.h` and `debug.cpp` when `DEBUG` is enabled.

### Generic Adapter APIs

Some generated headers are selected by platform probes but expose common
C++ APIs that are now used as normal trn utilities:

- `get_env_var`, `set_env_var`, and `unset_env_var`: environment access
  with `std::string` and `std::string_view` policy.
- `string_case_compare` and `string_case_equal` overloads for
  `std::string_view`: case-insensitive comparison helpers.

The platform-dependent part is only the implementation choice. The API
surface and default-value semantics are common utility code.
