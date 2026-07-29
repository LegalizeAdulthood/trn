<!-- Copyright (c) 2026, Richard Thomson -->

# Configuration Audit

This note compares the old metaconfig scripts under `config/legacy`
with the current CMake-based `config` library and generated data files.

The current system keeps most runtime policy choices as CMake cache
variables in `config/cmake/configure_trn.cmake`, emits C/C++ symbols
through `config/cmake/config.h.in`, and generates shell/data files from
`config/data/*.in`.

## Real Gaps

### Active Times

`configure_trn.cmake` defines the cache variable `ACTIVE_TIMES`, but
`config.h.in` expands `@ACT_TIMES@`.

Result: configured `ACTIVE_TIMES` values do not reach `config.h`.

### Legacy Probes Still Used by Source

These symbols are present in `config.h.in` and still gate source code,
but are not set by `configure_trn.cmake`:

- `HAS_RDCHK`
- `HAS_SIGBLOCK`
- `HAS_SIGHOLD`
- `UNION_WAIT`
- `USE_WIFSTAT`

Relevant consumers include terminal input polling, signal masking, NNTP
signal handling, and wait-status decoding.

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

### `sgtty.h`

CMake probes `sgtty.h` into `I_SGTTY`, and terminal code still checks
`I_SGTTY`, but `config.h.in` does not emit the symbol.

Result: any fallback behavior guarded by `I_SGTTY` is unreachable.

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

- `MAIL_FILE`
- `NEWS_LIB`
- `ACTIVE`
- `GROUP_DESC`
- `SUBSCRIPTIONS`
- `OVERVIEW_DIR`
- `OVERVIEW_FMT`

The old source-side fallback defaults in `config/include/config/common.h`
only apply when a symbol is not defined. Emitting an empty definition
bypasses those fallbacks.

### Host Bits

Legacy `hostbits` accepted an integer count: all host parts, two domain
parts, three domain parts, and so on.

Current CMake exposes `HOST_BITS` as a boolean option. The default still
behaves like a count of two, but users cannot configure larger counts.

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
- Host comparison policy: `HOST_BITS`. It is not a boolean platform
  probe; current code treats values greater than one as a count.

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
