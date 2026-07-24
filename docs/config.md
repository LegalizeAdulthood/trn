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
