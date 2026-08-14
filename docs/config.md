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

- `DEFAULT_EDITOR`: handled.  CMake probes the default editor and emits
  the configured value.
- `MAIL_FILE`: handled.  CMake owns the historical mail-file default and
  emits the configured value directly.
- `MBOX_CHAR`: handled.  CMake validates that the value is one
  character.
- `UNSUBSCRIBED_CHAR`: handled.  CMake owns the newsrc unsubscribe marker
  default and validates that it is one character.
- `PASS_NAMES`: handled.  CMake derives this generated detail from the
  `NAME_TYPE` policy before emitting `config.h`.
- `BERKELEY_NAMES`: handled.  CMake derives this generated detail from
  the `NAME_TYPE` policy before emitting `config.h`.
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
- `ACTIVE`: handled.  CMake derives the default from the probed or
  overridden news-source mode and emits the configured value directly.
- `ACTIVE_TIMES`: handled.  CMake owns `ACTIVE_TIMES`, defaults it to
  `none`, and `config.h.in` emits the cache value directly.
- `PIPE_HOST_CMD`: handled.  It remains an optional hostname fallback.
- `HAS_TERMLIB`: handled.  CMake probes termlib-style support from the
  configured terminal path, keeps the cache variable as an explicit
  override, and still links `libtrn` through the `curses` target.
- `IGNORE_ORG`: handled.  It still drives posting organization fallback.
- `SERVER_NAME`: handled.  CMake probes default NNTP server sources and
  keeps the cache variable as an explicit override.
- `HAS_LOCAL_SPOOL`: handled.  CMake probes local active-file and spool
  paths, keeps the cache variable as an explicit override, and validates
  that `SERVER_NAME=local` has local-spool support enabled.
- `MIMECAP`: handled.
- `NEWS_LIB`: handled.  CMake owns the retained platform default and
  emits the configured value directly.
- `EXTRA_INEWS`: handled.  It is still consumed by local posting.
- `GROUP_DESC`: handled.  CMake derives the default from the probed or
  overridden news-source mode and emits the configured value directly.
- `SUBSCRIPTIONS`: handled.  CMake derives the default from the probed
  or overridden news-source mode and emits the configured value directly.
- `NEWS_SPOOL`: handled.  CMake derives the default from the probed or
  overridden news-source mode and emits the configured value directly.
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
- `MAIL_CALL`, `NO_FIREWORKS`, `TILDE_NAME`, `VALIDATE_XREF_SITE`, and
  `USE_FTP`: handled.  CMake owns the retained feature-policy defaults
  and emits configured definitions directly.
- `PIPE_SAVER`: handled.  CMake owns the historical Unix command and a
  Windows-safe command default, and emits the configured value directly.
- `SHAR_SAVER`: handled.  CMake owns the historical Unix command and an
  unsupported Windows command default, and emits the configured value
  directly.
- `CUSTOM_SAVER`: handled.  CMake owns the historical Unix command and a
  Windows-safe command default, and emits the configured value directly.
- `VERIFY_RIPEM`: handled.  CMake emits a verifier command using the
  found `ripem` path; otherwise the default command is disabled.
- `VERIFY_PGP`: handled.  CMake emits a verifier command using the found
  `pgp` path; otherwise the default command is disabled.
- `OVERVIEW_DIR`: handled.  CMake derives the default from the probed or
  overridden news-source mode and emits the configured value directly.
- `OVERVIEW_FMT`: handled.  CMake derives the overview format-file
  default from `OVERVIEW_DIR` and emits the configured value directly.
- `THREAD_INIT`: handled.
- `SELECT_INIT`: handled.
- `USE_INEWS` and `BIN_DIR`: handled as CMake cache variables for
  generated helpers; remaining helper template migration is tracked
  below.
- `CALL_INEWS`, `NEWS_POSTER`, and `MAIL_POSTER`: handled.  CMake owns
  the retained posting helper defaults and emits configured commands
  directly.
- `PAGER`: handled.  CMake probes the pager used by generated help and
  helper scripts; the cache value remains an explicit override.
- `REPLYTO_POSTER_CHECKING`: handled.
- `FULLNAMEFILE`: handled.  CMake owns the historical full-name file
  default and emits the configured value directly.
- `NAME_TYPE`: handled.  CMake probes the default name-parsing policy,
  keeps it overrideable, derives the generated details, and emits the
  generated `Pnews` default.
- `LOCAL_DIST`, `ORGANIZATION_DIST`, `CITY_DIST`, `STATE_DIST`,
  `MULTISTATE_DIST`, `COUNTRY_DIST`, and `CONTINENT_DIST`: handled for
  generated `Pnews` distribution defaults.
- `DEBUG`: handled.
- `SHELL_START`, `DIFF_PROGRAM`, `ED_PROGRAM`, `ISPELL_PROGRAM`,
  `ISPELL_OPTIONS`, and `SPELL_PROGRAM`: handled.  CMake probes helper
  tool defaults for generated helper files.  Missing optional tools are
  disabled with empty values, except `ISPELL_PROGRAM` uses `none`, and
  cache variables remain explicit overrides.
- `MAIL_HEADER`, `NEWS_HEADER`, `CANCEL_HEADER`, and
  `SUPERSEDE_HEADER`: handled.  CMake owns the retained posting header
  templates and emits them directly.
- `YOU_SAID`, `ATTRIBUTION`, `FORWARD_POSTER`, `FORWARD_HEADER`,
  `FORWARD_MSG`, and `FORWARD_MSG_END`: handled.  CMake owns the
  retained reply and forward defaults and emits them directly.

### Source Symbols Without CMake Owners

These symbols are consumed outside `config/include/config`, but no CMake
cache variable or generated `config.h.in` owner exists for them.  If this
section gains entries, add a concrete release-plan slice to either add a
retained owner or explicitly retire each one.

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

The CMake setup exposes separate cache override pieces:

- `SERVER_NAME`
- `HAS_LOCAL_SPOOL`
- `ACTIVE`
- `NEWS_SPOOL`
- `GROUP_DESC`
- `SUBSCRIPTIONS`
- `OVERVIEW_DIR`
- `OVERVIEW_FMT`

The CMake setup keeps those pieces separate, but derives their defaults
from probed system features before applying user overrides:

- `NNTPSERVER` or a standard server-name file selects an NNTP default
  source.
- A local active file plus a local spool directory enables local-spool
  support.
- If local-spool support is found without an NNTP source, the default
  source is local.
- If both are found, NNTP remains the default source while retaining
  local-spool support.
- If neither is found, the release fallback is `news.gmane.io` with no
  local-spool support.

Explicit `SERVER_NAME` and `HAS_LOCAL_SPOOL` cache values override the
probed defaults.

`SERVER_NAME=local` with `HAS_LOCAL_SPOOL=OFF` fails configure.

### Thread Directory

Legacy configured a thread directory for `.thread`/XTHREAD database files.
That support has been retired: remote access uses XOVER overview data and
builds threads on the client, and local spool thread databases are no
longer supported.  `Thread Dir` is not a supported access-file setting.

### `Rnmail`

`config.h.in` emits `MAIL_POSTER` as `Rnmail -h %h`, assuming `Rnmail`
is available on the command path.  CMake does not generate `Rnmail`.

The only `Rnmail` generator currently present is the legacy
`config/legacy/Rnmail.SH`, which is listed as an interface source, not
configured into a runnable script.

Result: porting `Rnmail.SH` to CMake remains a later migration item.

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

### Posting Host Match Segments

Legacy `hostbits` accepted an integer count: all host parts, two domain
parts, three domain parts, and so on.  CMake retains that behavior as
`POSTING_HOST_MATCH_SEGMENTS`.

The configured count controls the posting-host suffix used when checking
whether an article belongs to the current login.

### Name Parsing Defaults

Legacy `Configure` asked whether `/etc/passwd` stored full names in
Berkeley/V7 format first.  If that answer was no, it asked whether
`/etc/passwd` stored full names in USG format.  Those answers selected
`nametype=bsd`, `nametype=usg`, or `nametype=other`, then generated
`PASS_NAMES`, `BERKELEY_NAMES`, and `d_usgnames` from that policy.

Current CMake exposes `NAME_TYPE` as an overrideable cache variable,
probes the default from local passwd GECOS data when `getpwent()` is
available, and derives `PASS_NAMES` and `BERKELEY_NAMES` for the
generated config.

## Changed Semantics

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

Current CMake retains only the tool paths used by generated files and
runtime defaults:

- `DIFF_PROGRAM`
- `ED_PROGRAM`
- `ISPELL_PROGRAM`
- `ISPELL_OPTIONS`
- `SPELL_PROGRAM`
- `PAGER`
- `DEFAULT_EDITOR`
- `PREF_SHELL`

Those retained defaults are probed with CMake.  Missing optional tools
default to empty values, except `ISPELL_PROGRAM` uses `none`, and cache
variables remain explicit overrides.

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
