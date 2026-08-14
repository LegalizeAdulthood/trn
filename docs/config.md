<!-- Copyright (c) 2026, Richard Thomson -->

# Configuration Audit

This note compares the old metaconfig scripts under `config/legacy`
with the current CMake-based `config` library and generated data files.

The current system keeps most runtime policy choices as CMake cache
variables in `config/cmake/configure_trn.cmake`, emits C/C++ symbols
through `config/cmake/config.h.in`, and generates shell/data files from
`config/data/*.in`.

## Configure Option Inventory

Status labels:

- `handled`: CMake owns the setting or an intentional feature check.
- `needed`: live source or generated files still need CMake migration.
- `removed`: old Configure behavior is intentionally not a CMake option.
- `obsolete`: old portability logic is displaced by modern build logic.

Legacy source compared: `config/legacy/Configure`, especially the
`config.sh` output block and the prompts that feed install paths, news
source policy, posting policy, name policy, tool paths, and feature
probes.

### Current CMake Owners

These legacy policy choices already have CMake cache owners.  Some still
need semantic fixes, listed here rather than hidden in prose:

- `DEFAULT_EDITOR`: handled.  CMake finds the default editor and emits
  the configured value.
- `MAIL_FILE`: handled.  CMake owns the historical mail-file default and
  emits the configured value directly.
- `MBOX_CHAR`: handled.  CMake validates that the value is one
  character.
- `UNSUBSCRIBED_CHAR`: handled.  CMake owns the newsrc unsubscribe marker
  default and validates that it is one character.
- `PASS_NAMES`: needed.  CMake owns the option, but current defaults do
  not match the old Linux hint.
- `BERKELEY_NAMES`: needed.  CMake owns the option, but it does not
  imply `PASS_NAMES`.
- `PASSWORD_FILE`: handled.  CMake owns the historical password-file
  default and emits the configured value directly.
- `LOGIN_DIR_FIELD`: handled.  CMake owns the historical login-directory
  field number for password-file fallback lookup.
- `TRNDIR`: handled.  CMake owns the historical long-filename default
  and emits the configured value directly.
- `TRNMACRO`: handled.  CMake owns the historical macro-file default and
  emits the configured value directly.
- `RNMACRO`: handled.  CMake owns the historical long-filename default
  and emits the configured value directly.
- `RCNAME`: handled.  CMake owns the historical long-filename default and
  emits the configured value directly.
- `LOCKNAME`: handled.  CMake owns the historical long-filename default
  and emits the configured value directly.
- `LASTNAME`: handled.  CMake owns the historical long-filename default
  and emits the configured value directly.
- `SIGNATURE_FILE`: handled.  CMake owns the historical long-filename
  default and emits the configured value directly.
- `NNTP_AUTH_FILE`: handled.  CMake owns the historical auth-file default
  and emits the configured value directly.
- `INSTALL_PREFIX`: handled.  It follows `CMAKE_INSTALL_PREFIX`.
- `PREF_SHELL`: handled for Unix.  Windows-safe shell policy is owned by
  later Windows helper work.
- `SH`: handled.  CMake owns the shell used for command execution and
  emits the configured value directly.
- `ROOT_UID`: handled for POSIX policy.
- `ACTIVE`: handled.  CMake owns the historical active-file default and
  emits the configured value directly.
- `ACTIVE_TIMES`: handled.  CMake owns `ACTIVE_TIMES`, defaults it to
  `none`, and `config.h.in` emits the cache value directly.
- `PIPE_HOST_CMD`: handled.  It remains an optional hostname fallback.
- `HAS_TERMLIB`: handled.  It is now an explicit cache boolean while the
  target links through curses.
- `IGNORE_ORG`: handled.  It still drives posting organization fallback.
- `SERVER_NAME`: handled.  It replaces the old NNTP server prompt.
- `HAS_LOCAL_SPOOL`: handled.  Coupled local/NNTP defaults still need
  review with the news-source mode work below.
- `MIMECAP`: handled.
- `NEWS_LIB`: handled.  CMake owns the retained platform default and
  emits the configured value directly.
- `EXTRA_INEWS`: handled.  It is still consumed by local posting.
- `GROUP_DESC`: needed.  CMake owns the value, but the empty default
  bypasses the `common.h` fallback.
- `SUBSCRIPTIONS`: needed.  CMake owns the value, but the empty default
  bypasses the intended source fallback.
- `NEWS_SPOOL`: handled as the default local spool path.
- `SELECTION_CHARS`: handled.  CMake owns the historical selection-menu
  character default and emits the configured value directly.
- `ORG_NAME`: handled.
- `POSTING_HOSTNAME`: handled.
- `POSTING_HOST_MATCH_SEGMENTS`: handled.  CMake owns the legacy
  `hostbits` count under a name that reflects local-posting ownership
  matching.
- `TRN_MARK_XREFS_UNREAD`: handled.  It is the CMake-facing owner for
  generated `MCHASE`.
- `PRIVATE_LIB`: handled.
- `GLOBAL_INIT`: handled.  CMake owns the historical system-wide init
  file default and emits the configured value directly.
- `DEFACCESS`: handled.  CMake owns the historical default access-list
  path and emits the configured value directly.
- `TRNACCESS`: handled.  CMake owns the historical user access-list path
  and emits the configured value directly.
- `NEWSNEWSNAME`: handled.  CMake owns the historical newsnews path and
  emits the configured value directly.
- `OV_FILE_NAME`: handled.  CMake owns the historical long-filename
  overview basename and emits the configured value directly.
- `VARYNAME`: handled.  CMake owns the historical virtual-array file
  template and emits the configured value directly.
- `HEADNAME`: handled.  CMake owns the historical long-filename header
  file template and emits the configured value directly.
- `MKDIRS`: handled.  CMake expresses the old default save-layout
  selection as `TRN_MAKE_SAVE_DIRECTORIES`, without emitting a source
  macro.
- `SAVEDIR`: handled.  CMake owns the historical save-directory defaults
  and emits the configured value directly.
- `SAVENAME`: handled.  CMake owns the historical save-file defaults and
  emits the configured value directly.
- `KILL_GLOBAL`, `KILL_LOCAL`, and `KILL_THREADS`: handled.  CMake owns
  the historical kill-file path defaults and emits the configured values
  directly.
- `LOCALTIME_FMT`: handled.  CMake owns the historical local-time format
  default and emits the configured value directly.
- `PIPE_SAVER`: handled.  CMake owns the historical Unix command and a
  Windows-safe command default, and emits the configured value directly.
- `SHAR_SAVER`: handled.  CMake owns the historical Unix command and an
  unsupported Windows command default, and emits the configured value
  directly.
- `CUSTOM_SAVER`: handled.  CMake owns the historical Unix command and a
  Windows-safe command default, and emits the configured value directly.
- `VERIFY_RIPEM`: handled.  CMake emits the historical verifier command
  when `ripem` is found; otherwise the default command is disabled.
- `VERIFY_PGP`: handled.  CMake emits the historical verifier command
  when `pgp` is found; otherwise the default command is disabled.
- `OVERVIEW_DIR`: needed.  CMake owns the value, but the empty default
  bypasses source fallback behavior.
- `OVERVIEW_FMT`: needed.  CMake owns the value, but the empty default
  bypasses source fallback behavior.
- `THREAD_INIT`: handled.
- `SELECT_INIT`: handled.
- `USE_INEWS`: needed.  CMake emits `CALL_INEWS`, but posting helper
  generation and install layout are not complete.
- `BIN_DIR`: needed.  CMake emits poster defaults through it, but
  generated helpers and install layout are not complete.
- `PAGER`: handled for generated help and runtime defaults.
- `REPLYTO_POSTER_CHECKING`: handled.
- `FULLNAMEFILE`: handled.  CMake owns the historical full-name file
  default and emits the configured value directly.
- `NAME_TYPE`: needed.  It feeds generated `Pnews`, whose template is not
  fully migrated.
- `LOCAL_DIST`, `ORGANIZATION_DIST`, `CITY_DIST`, `STATE_DIST`,
  `MULTISTATE_DIST`, `COUNTRY_DIST`, and `CONTINENT_DIST`: needed.
  They feed generated `Pnews`, whose template is not fully migrated.
- `DEBUG`: handled.
- `SHELL_START`, `DIFF_PROGRAM`, `ED_PROGRAM`, `ISPELL_PROGRAM`,
  `ISPELL_OPTIONS`, and `SPELL_PROGRAM`: needed.  CMake owns them for
  generated helper files, but the helper templates still contain
  metaconfig residue.
- `MAIL_HEADER`, `NEWS_HEADER`, `CANCEL_HEADER`, and
  `SUPERSEDE_HEADER`: handled.  CMake owns the retained posting header
  templates and emits them directly.
- `YOU_SAID`, `ATTRIBUTION`, `FORWARD_POSTER`, `FORWARD_HEADER`,
  `FORWARD_MSG`, and `FORWARD_MSG_END`: handled.  CMake owns the
  retained reply and forward defaults and emits them directly.

### Source Symbols Without CMake Owners

These symbols are consumed outside `config/include/config`, but no CMake
cache variable or generated `config.h.in` owner exists for them.  Slice 5
must either add a retained owner or explicitly retire each one.

- Feature policy: `MAIL_CALL`, `NO_FIREWORKS`, `TILDE_NAME`,
  `VALIDATE_XREF_SITE`, and `USE_FTP`.

### Retired Configure Areas

These legacy Configure variables are intentionally not CMake cache
options unless a live source consumer is found:

- `handled`: compiler, linker, build type, library search, executable
  suffix, and object suffix settings are CMake target or generator
  policy.
- `handled`: `bison`, `yacc`, and parser generation are CMake target
  logic.
- `handled`: file descriptor I/O, pipe I/O, environment, string compare,
  hostname, domain, vfork, and include-file checks are CMake feature
  checks.
- `obsolete`: `nm` extraction, C library path prompts, bundled
  `strftime`, old `ndir` emulation, and Tk linkage are not 5.0
  configure options.
- `removed`: `config.sh`, `Policy.sh`, `.SH` extraction, generated
  Makefile policy, AFS install splitting, prototype generation, and
  typedef regeneration are legacy build mechanics.
- `removed`: `LIMITED_FILENAMES` variants for user-state and article
  paths; CMake retains the historical long-filename defaults.
- `removed`: old scan/score memory-saving toggles and pre-ANSI compiler
  probes stay retired unless current source grows a concrete dependency.

## Real Gaps

### Remote Threading Methods

Current news-server support favors overview and header-based threading
data. `OVER` overview here means capability discovery, `OVER`, and
`LIST OVERVIEW.FMT`. `HDR` refs means retrieving `References` with `HDR`
or old `XHDR`.

| Rank | Method | Current server fit |
| --- | --- | --- |
| 1 | `OVER` overview | Best standard path |
| 2 | `XOVER` overview | Common compatibility path |
| 3 | `HDR` refs | Standard sparse-overview fallback |
| 4 | `XROVER` refs | Legacy shortcut |
| 5 | `HEAD` | Available but expensive |

Current trn support for those same ranked methods is:

| Rank | Method | Current trn support |
| --- | --- | --- |
| 1 | `OVER` overview | Partial: fmt yes; caps/`OVER` no |
| 2 | `XOVER` overview | Yes |
| 3 | `HDR` refs | Partial: `XHDR` yes; `HDR` no |
| 4 | `XROVER` refs | No |
| 5 | `HEAD` | Yes |

Result: remaining remote-threading work is modernizing the overview path
to prefer `CAPABILITIES` and `OVER` before falling back to `XOVER`.

### News Source Mode

Legacy `Configure` had one coordinated prompt for news source support:
`local`, `nntp`, or `both`.

The CMake setup exposes separate pieces:

- `SERVER_NAME`
- `HAS_LOCAL_SPOOL`
- `ACTIVE`
- `NEWS_SPOOL`
- `GROUP_DESC`
- `SUBSCRIPTIONS`
- `OVERVIEW_DIR`
- `OVERVIEW_FMT`

This covers most data, but it does not reproduce the old coupled
defaults, resets, or validation. Changing one side no longer clears or
recomputes the dependent values.

### Thread Directory

Legacy configured a thread directory. Current code parses a `Thread Dir`
field in `DataSourceConfig`, but there is no CMake cache variable, no
sample entry in `access_def.in`, and no consumer found that applies it to
the default data source.

Result: thread-directory configuration is only partially migrated.

### `Rnmail`

`config.h.in` emits `MAIL_POSTER` as `@BIN_DIR@/Rnmail -h %h`, but
CMake does not generate `Rnmail`.

The only `Rnmail` generator currently present is the legacy
`config/legacy/Rnmail.SH`, which is listed as an interface source, not
configured into a runnable script.

Result: the compiled default mail poster can point at a missing file.

### `Pnews` Template Migration

`config/data/Pnews.in` is only partly converted from the old `.SH`
template form.

Observed leftovers:

- It still starts with `$spitshell >Pnews <<!GROK!THIS!`.
- `inews=${@USE_INEWS@-inews}` is malformed for CMake substitution.
- It still leaves old shell variables such as `nidump` and `ypmatch`.

Result: generated `Pnews` may contain metaconfig residue or broken shell
syntax.

### `mbox.saver`

`config/data/mbox.saver.in` still uses `$sed`, but CMake does not define
or substitute that variable.

Result: generated `mbox.saver` can call an empty command at the `From`
line escaping step.

### Empty Cache Defaults Override Fallbacks

Several CMake cache variables default to empty strings and are then
unconditionally emitted to `config.h`.

Notable cases:

- `GROUP_DESC`
- `SUBSCRIPTIONS`
- `OVERVIEW_DIR`
- `OVERVIEW_FMT`

The old source-side fallback defaults in `config/include/config/common.h`
only apply when a symbol is not defined. Emitting an empty definition
bypasses those fallbacks.

### Posting Host Match Segments

Legacy `hostbits` accepted an integer count: all host parts, two domain
parts, three domain parts, and so on.  CMake retains that behavior as
`POSTING_HOST_MATCH_SEGMENTS`.

The configured count controls the posting-host suffix used when checking
whether an article belongs to the current login.

### Name Parsing Defaults

The old Linux hint selected Berkeley-style passwd full-name parsing.

Current defaults leave both `PASS_NAMES` and `BERKELEY_NAMES` off. Also,
`BERKELEY_NAMES` does not imply `PASS_NAMES`, so selecting Berkeley format
alone has no effect on the C++ name lookup path.

Result: `%N` and posting-name defaults can differ from the legacy Linux
configuration.

## Changed Semantics

### Termlib

Legacy `Configure` tried to detect termlib routines and libraries.

Current CMake exposes `HAS_TERMLIB` as a cache boolean defaulting to on,
while the build links `libtrn` to the `curses` target. This is probably
intentional, but it is not the same probe model.

### Tool Paths

Legacy located many tools directly:

- `awk`
- `cat`
- `diff`
- `ed`
- `egrep`
- `grep`
- `ispell`
- `mail`
- `more`
- `pgp`
- `sed`
- `spell`
- `vi`

Current CMake only exposes a small subset for generated scripts:

- `DIFF_PROGRAM`
- `ED_PROGRAM`
- `ISPELL_PROGRAM`
- `ISPELL_OPTIONS`
- `SPELL_PROGRAM`
- `PAGER`
- `DEFAULT_EDITOR`
- `PREF_SHELL`

This is mostly reasonable, but templates that still refer to old tool
variables need either fixed literals, CMake variables, or removal.

## Mostly Replaced by CMake or Modern Code

These old configure areas appear intentionally retired or displaced:

- compiler and optimizer prompts
- linker flags and library search prompts
- `nm`-based symbol extraction
- C library path selection
- old `ndir` emulation
- bundled `strftime.c`
- Tk linkage
- scan/score size toggles
- Makefile generation and install path split for AFS
- `config.sh`, `Policy.sh`, and `.SH` extraction mechanics

They should not be treated as missing config library options unless a
current source file still consumes the old symbol or behavior.

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
- `file_ref`: absolute-path classifier. Its implementation varies for
  MSDOS, but the API is a trn path helper, not a platform setting.
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
- `TILDE_NAME`: tilde-login expansion; currently defined but no current
  consumer found.
- `MCHASE`: generated xref-unmarking option used by article and newsrc
  logic.
- `M_CHASE`: stale common-header spelling. It is only undefined in
  `common.h`; current consumers use `MCHASE`.
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
- News-source defaults: `SERVER_NAME`, `HAS_LOCAL_SPOOL`, `NEWS_LIB`,
  `PRIVATE_LIB`, `ACTIVE`, `ACTIVE_TIMES`, `GROUP_DESC`,
  `SUBSCRIPTIONS`, `NEWS_SPOOL`, `OVERVIEW_DIR`, `OVERVIEW_FMT`, and
  `EXTRA_INEWS`.
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
  `CONTINENT_DIST`, and `PNEWS_ORG_NAME`.

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
