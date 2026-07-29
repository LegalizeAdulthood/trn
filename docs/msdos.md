<!-- Copyright (c) 2026, Richard Thomson -->

# MSDOS Audit

## Direction

The `MSDOS` preprocessor macro is a holdover from an actual DOS port.
In the current CMake configuration it is set for `WIN32`, so modern
Windows builds inherit DOS-era behavior.

The next release should eliminate as much of this holdover cruft as
practical.  Platform variation should be expressed through CMake feature
tests and the config library, not through scattered `#ifdef MSDOS`
branches across headers and source files.  Sprinkled conditional
compilation makes behavior hard to audit, test, and evolve.

`MSDOS` should not be used as a shorthand for Windows.  Each current use
should be replaced with the feature or platform policy it actually
means.

## Current Definition

- `config/cmake/configure_trn.cmake` sets `MSDOS` when `WIN32` is true.
- `config/cmake/config.h.in` exposes `MSDOS` in the generated config.
- `config/include/config/config2.h` includes `config/msdos.h` when
  `MSDOS` is defined.
- `config/include/config/msdos.h` defines DOS-specific helpers and
  policies: drive-letter absolute paths, baud constants,
  `RESTORE_ORIGDIR`, `NO_FILELINKS`, `LAX_INEWS`, and `sleep` as
  `_sleep`.
- `config/include/config/common.h` treats `MSDOS` as a reason to enable
  `PENDING`.

The legacy Configure and Makefile templates also contain `MSDOS`
support, but those are separate historical build-system artifacts.

## Environment Behavior

`util/env.cpp` changes login and home-directory discovery under
`MSDOS`.

Normal order for `g_home_dir`:

- `HOME`
- `LOGDIR`
- fallback to `/` in `env_init2`

Additional `MSDOS` order:

- if `g_home_dir` is still empty, use `HOMEDRIVE` plus `HOMEPATH`

Normal login behavior:

- in lax mode, try `USER`, then `LOGNAME`
- outside `MSDOS`, fall back to `getlogin`

Additional `MSDOS` behavior:

- if `g_login_name` is still empty, try `USERNAME`

`HOMEDRIVE` and `HOMEPATH` are used only in this environment fallback.
They are not used elsewhere in production code.  Tests define the names
only when `MSDOS` is set; otherwise test helpers map them to `nullptr`
so non-DOS expectations can ignore them.

## Terminal Behavior

`libtrn/terminal.cpp` and `libtrn/include/trn/terminal.h` contain most
of the `MSDOS` variation.

Current behavior under `MSDOS`:

- include `<conio.h>`
- use fixed ANSI terminal capability strings instead of termcap lookup
- skip termcap storage area setup
- use DOS arrow-key scan sequences instead of termcap `ku`, `kd`, `kl`,
  and `kr`
- use `kbhit` for pending input when no other pending-input feature is
  available
- use `getch` for terminal reads, mapping a zero byte to a control
  prefix for extended keys
- provide local `tputs` and `tgoto` shims
- provide no-op tty mode functions when termios is unavailable

These are several independent platform features: console input,
terminal capability source, cursor addressing, and tty mode control.
They should be split into config-library abstractions.

## Shell Behavior

`libtrn/util.cpp`, `do_shell`, uses `MSDOS` to select subprocess
execution.

Current behavior under `MSDOS`:

- include `<process.h>`
- run commands with `spawnl(P_WAIT, shell, shell, "/c", cmd, nullptr)`

Normal behavior:

- use `vfork`, `execl`, signal handling, and `wait`
- use `-c` when invoking a shell command

This should be a process-launch feature or platform service, not a DOS
macro branch.

## File And Path Behavior

`config/msdos.h` changes absolute-path detection:

- under `MSDOS`, `FILE_REF` accepts `/` and drive-letter paths such as
  `C:...`
- otherwise `FILE_REF` accepts only `/`

This should become an explicit path policy in the config library.

`libtrn/decode.cpp` changes attachment filename handling:

- under `MSDOS`, use an allow-list of filename characters
- otherwise use a Unix-style bad-character list
- under `MSDOS`, reject DOS device names such as `aux`, `con`, `nul`,
  `prn`, `com1` through `com4`, and `lpt1` through `lpt3`
- under `MSDOS`, unpack multipart pieces under `%Y/parts/`
- otherwise unpack under `%Y/m-prts-%L/`

These are filesystem policy decisions.  Reserved names, invalid
characters, and staging-directory layout should be modeled directly.

## Permissions And Locking

`libtrn/nntp.cpp` skips `chmod(0600)` on NNTP temporary article files
under `MSDOS`.

`libtrn/rcstuff.cpp` skips two Unix-specific paths under `MSDOS`:

- stale newsrc lock checking with pid, host, and `kill(pid, 0)`
- preserving `stat`, `chmod`, and `chown` metadata on rewritten newsrc
  files

The DOS lock path still rewrites the lock file, but it does not perform
the non-DOS process and host validation.  These should become capability
checks for process probing and file ownership/mode preservation.

## Replacement Targets

Replace `MSDOS` sites with narrowly named configuration facts:

- `TRN_HAS_CONIO` or a console-input backend for `kbhit` and `getch`
- `TRN_HAS_TERMIOS` or a tty-mode backend for terminal mode changes
- a terminal capability provider for fixed ANSI versus termcap data
- a process-launch service for `spawnl` versus `fork` and `exec`
- a path policy for drive-letter absolute paths
- a filename policy for reserved names and invalid characters
- a file-permission capability for `chmod` and `chown`
- a process-probing capability for stale lock validation
- a decode-staging-directory policy

The cleanup goal is to move decisions to build-time feature tests and
small platform services, then make call sites consume those services
without open-coded preprocessor branches.
