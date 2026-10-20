<!-- Copyright (c) 2026, Richard Thomson -->

# Installing trn 5.0

`INSTALL.md` is the canonical install document for trn 5.0.  The
checked-in CMake presets are the supported build entry points.

## Supported Platforms

The 5.0 target platform set is Windows, Linux, FreeBSD, NetBSD, OpenBSD,
and macOS.  Current CI covers Windows, Linux, FreeBSD, NetBSD, and
OpenBSD.  macOS support is a separate 5.0 release gate.

## Required Tools

Install these first:

- Git, with submodule support.
- CMake 3.23 or newer.
- A C++17 compiler and a CMake-supported build tool.
- Bison on Unix-like systems.  On Windows, CMake downloads winflexbison
  during configure.
- Network access for the first vcpkg dependency restore.

On Windows, Visual Studio 2022 with the Desktop development with C++
workload is the expected compiler environment.  On Unix-like systems, use
the platform compiler with Ninja or Make.

## Fresh Checkout

Clone the repository, then initialize submodules:

```console
git clone https://github.com/LegalizeAdulthood/trn.git
cd trn
git submodule update --init --recursive
```

Bootstrap the vcpkg submodule before configuring.

Windows:

```console
.\vcpkg\bootstrap-vcpkg.bat
```

Unix-like systems:

```console
./vcpkg/bootstrap-vcpkg.sh
```

The CMake preset uses `vcpkg/scripts/buildsystems/vcpkg.cmake`, and vcpkg
installs the manifest dependencies from `vcpkg.json` during configure.

## Configure, Build, And Test

The default preset writes its build tree to `../build-default` and builds
the Debug configuration.

Run the steps separately:

```console
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Or run the checked-in workflow preset:

```console
cmake --workflow --preset default
```

## CMake Customization

Set site policy at configure time with `-DNAME=value`:

```console
cmake --preset default -DPRIVATE_LIB=%x/trn
```

These cache values are compiled into `config.h` or generated helper files.
Personal reading preferences should normally stay in `.trn/trnrc`.
Variables not named here are internal derived details, ordinary CMake
toolchain variables, or historical audit entries in `docs/config.md`.

### Install Layout Variables

- `CMAKE_INSTALL_PREFIX`: final runtime prefix. Configure this to the
  path where trn will run. Use `cmake --install --prefix` only for
  staging, because it does not rewrite compiled defaults.
- `CMAKE_INSTALL_BINDIR`: command directory below the prefix.
- `CMAKE_INSTALL_MANDIR`: manual-page root below the prefix.
- `INSTALL_PREFIX`: prefix recorded for `%x` expansion; normally follows
  `CMAKE_INSTALL_PREFIX`.
- `NEWS_LIB`: news library directory used by `%x`.
- `PRIVATE_LIB`: trn private library directory used by `%X`.
- `BIN_DIR`: command directory used by generated posting defaults.

Coverage: `ConfigCMakeTest.installLayoutContainsRuntimeHelpers`,
`GeneratedConfigTest.newsLibDefaultMatchesPlatformDefault`,
`GeneratedConfigTest.newsLibConfiguredPathIsEmitted`, and
`GeneratedConfigTest.postingHelpersUseInstalledHelperPaths`.
Untested: `INSTALL_PREFIX`.

### News Source Variables

CMake probes NNTP and local spool inputs first.  Override these only for a
known site policy:

- `SERVER_NAME` and `HAS_LOCAL_SPOOL`: selected news source policy.
- `ACTIVE`, `ACTIVE_TIMES`, `GROUP_DESC`, `SUBSCRIPTIONS`, and
  `NEWS_SPOOL`: local or remote data-source defaults.
- `OVERVIEW_DIR` and `OVERVIEW_FMT`: overview storage and format-file
  defaults.
- `EXTRA_INEWS`: optional local posting command path.

Coverage: `ConfigCMakeTest.newsSourceProbeDetectsLocalOnly`,
`ConfigCMakeTest.newsSourceProbeDetectsNntpOnly`,
`ConfigCMakeTest.newsSourceProbeDetectsBoth`,
`ConfigCMakeTest.newsSourceProbeUsesReleaseDefaultWithoutSources`,
`ConfigCMakeTest.newsSourceDefaultsUseNntpMode`,
`ConfigCMakeTest.newsSourceEmptyDefaultReplacesLegacyPath`,
`ConfigCMakeTest.newsSourceDefaultsUseLocalMode`,
`ConfigCMakeTest.newsSourceDefaultsUseBothMode`,
`ConfigCMakeTest.newsSourceDefaultsRejectLocalWithoutSpool`,
`GeneratedConfigTest.newsSourceDetectedDefaultsAreEmitted`,
`GeneratedConfigTest.newsSourceNntpModeDefaultsAreEmitted`,
`GeneratedConfigTest.newsSourceLocalModeDefaultsAreEmitted`,
`GeneratedConfigTest.newsSourceBothModeDefaultsAreEmitted`,
`GeneratedConfigTest.activeTimesDefaultIsNone`,
`GeneratedConfigTest.activeTimesConfiguredPathIsEmitted`,
`GeneratedConfigTest.groupDescConfiguredPathIsEmitted`,
`GeneratedConfigTest.subscriptionsConfiguredPathIsEmitted`,
`GeneratedConfigTest.overviewDirConfiguredPathIsEmitted`, and
`GeneratedConfigTest.overviewFmtConfiguredPathIsEmitted`.
Untested: `EXTRA_INEWS`.

### Account And Site Identity

- `NAME_TYPE`: passwd GECOS policy.  If unset, CMake probes the default.
- `PASSWORD_FILE`, `LOGIN_DIR_FIELD`, and `FULLNAMEFILE`: fallback full
  name lookup.
- `ROOT_UID`: uid treated as root by POSIX permission checks.
- `ORG_NAME`, `IGNORE_ORG`, and `POSTING_HOSTNAME`: organization and
  posting-host defaults.
- `POSTING_HOST_MATCH_SEGMENTS`: posting-host suffix segment count.
- `LOCAL_DIST`, `ORGANIZATION_DIST`, `CITY_DIST`, `STATE_DIST`,
  `MULTISTATE_DIST`, `COUNTRY_DIST`, and `CONTINENT_DIST`: Pnews
  distribution codes.
- `PIPE_HOST_CMD`: optional command used to derive the host name.

Coverage: `ConfigCMakeTest.nameTypeAcceptsBsd`,
`ConfigCMakeTest.nameTypeAcceptsUsg`,
`ConfigCMakeTest.nameTypeAcceptsOther`,
`ConfigCMakeTest.nameTypeRejectsUnknown`,
`ConfigCMakeTest.nameTypeBsdDerivesPassNames`,
`ConfigCMakeTest.nameTypeUsgDerivesPassNames`,
`ConfigCMakeTest.nameTypeOtherDerivesFullnameFile`,
`ConfigCMakeTest.nameTypeDefaultUsesBsdProbe`,
`ConfigCMakeTest.nameTypeDefaultUsesUsgProbe`,
`ConfigCMakeTest.nameTypeDefaultUsesOtherProbe`,
`ConfigCMakeTest.nameTypeDefaultUsesOtherWithoutGetpwent`,
`ConfigCMakeTest.nameTypeRejectsBsdWithoutGetpwent`,
`ConfigCMakeTest.postingHostMatchSegmentsAcceptsZero`,
`ConfigCMakeTest.postingHostMatchSegmentsAcceptsLargeCount`,
`ConfigCMakeTest.postingHostMatchSegmentsRejectsText`,
`ConfigCMakeTest.postingHostMatchSegmentsRejectsNegative`,
`ConfigCMakeTest.pnewsDistributionCodeAcceptsNone`,
`ConfigCMakeTest.pnewsDistributionCodeRejectsEmpty`,
`GeneratedConfigTest.accountNameFilesDefaultMatchHistoricalFallback`,
`GeneratedConfigTest.accountNameFilesConfiguredValuesAreEmitted`,
`GeneratedConfigTest.postingHostMatchSegments*`,
`GeneratedConfigTest.pnewsDistributionDefaultsMatchHistoricalFallbacks`,
`GeneratedConfigTest.pnewsDistributionConfiguredValuesAreEmitted`, and
`HostMatchNameTest.*`.
Untested: `ROOT_UID`, `ORG_NAME`, `IGNORE_ORG`, `POSTING_HOSTNAME`, and
`PIPE_HOST_CMD`.

### User State And Data Files

- `TRNDIR`, `TRNMACRO`, `RNMACRO`, `RCNAME`, `LOCKNAME`, `LASTNAME`,
  `SIGNATURE_FILE`, and `NNTP_AUTH_FILE`: user state paths.
- `GLOBAL_INIT`, `DEFACCESS`, `TRNACCESS`, and `NEWSNEWSNAME`: trn data
  file paths.
- `MIMECAP`: system mimecap path appended after user mimecap files.
- `OV_FILE_NAME`, `VARYNAME`, and `HEADNAME`: article-side path
  templates.
- `MAIL_FILE`: mail spool path template.
- `SAVEDIR`, `SAVENAME`, and `TRN_MAKE_SAVE_DIRECTORIES`: save-file
  layout.
- `KILL_GLOBAL`, `KILL_LOCAL`, and `KILL_THREADS`: memorized-command
  paths.

Coverage: `GeneratedConfigTest.userStatePaths*`,
`GeneratedConfigTest.userStatePathsConfiguredValuesAreEmitted`,
`GeneratedConfigTest.dataFilePathsDefaultMatchHistoricalFallbacks`,
`GeneratedConfigTest.dataFilePathsConfiguredValuesAreEmitted`,
`GeneratedConfigTest.articlePathTemplatesDefaultMatchHistoricalFallbacks`,
`GeneratedConfigTest.articlePathTemplatesConfiguredValuesAreEmitted`,
`GeneratedConfigTest.mailFileDefaultMatchesHistoricalFallback`,
`GeneratedConfigTest.mailFileConfiguredPathIsEmitted`,
`GeneratedConfigTest.savePathTemplatesDefaultMatchCMakeSelection`,
`GeneratedConfigTest.savePathTemplatesCanUseCategoryPolicy`,
`GeneratedConfigTest.savePathTemplatesConfiguredValuesAreEmitted`,
`GeneratedConfigTest.killFilePathsDefaultMatchHistoricalFallbacks`,
`GeneratedConfigTest.killFilePathsConfiguredValuesAreEmitted`,
`ConfigCMakeTest.savePathDefaultsUseFlatMode`, and
`ConfigCMakeTest.savePathDefaultsUseCategoryMode`.
Untested: `MIMECAP`.

### Command And Helper Variables

- `DEFAULT_EDITOR`, `PREF_SHELL`, `SH`, and `PAGER`: editor, shell, and
  pager defaults.
- `SHELL_START`, `DIFF_PROGRAM`, `ED_PROGRAM`, `ISPELL_PROGRAM`,
  `ISPELL_OPTIONS`, and `SPELL_PROGRAM`: generated helper tool defaults.
  Windows `Speller.cmd` uses native checker programs; it does not require
  the POSIX differencing or editing tools.
- `PIPE_SAVER`, `SHAR_SAVER`, and `CUSTOM_SAVER`: save and extraction
  commands.
- `VERIFY_RIPEM` and `VERIFY_PGP`: optional verifier commands.

Coverage: `ConfigCMakeTest.toolPathDefaultsUseFoundTools`,
`ConfigCMakeTest.toolPathDefaultsDisableMissingTools`,
`ConfigCMakeTest.toolPathDefaultsKeepOverrides`,
`GeneratedConfigTest.shellCommandDefaultsMatchPlatformFallbacks`,
`GeneratedConfigTest.shellCommandConfiguredValuesAreEmitted`,
`GeneratedConfigTest.verificationCommandDefaultsMatchToolDiscovery`,
`GeneratedConfigTest.verificationCommandDisabledDefaultsAreEmitted`,
`GeneratedConfigTest.verificationCommandConfiguredValuesAreEmitted`,
`GeneratedConfigTest.helperToolDefaultsMatchToolDiscovery`, and
`GeneratedConfigTest.helperToolConfiguredValuesAreEmitted`.
Untested: `PREF_SHELL`.

### Posting And Reply Variables

- `USE_INEWS`, `CALL_INEWS`, `NEWS_POSTER`, and `MAIL_POSTER`: posting
  helper command defaults.  On Windows, `NEWS_POSTER` and `MAIL_POSTER`
  select generated `.cmd` helpers.  `Pnews.cmd` supports NNTP posting;
  `Rnmail.cmd` reports that email delivery is unsupported until its port.
- `MAIL_HEADER`, `NEWS_HEADER`, `CANCEL_HEADER`, and `SUPERSEDE_HEADER`:
  posting header templates.
- `YOU_SAID`, `ATTRIBUTION`, `FORWARD_POSTER`, `FORWARD_HEADER`,
  `FORWARD_MSG`, and `FORWARD_MSG_END`: reply and forward defaults.
- `REPLYTO_POSTER_CHECKING`: optional reply-to poster identity check.

Coverage: `GeneratedConfigTest.postingHelperPaths*`,
`GeneratedConfigTest.postingHelperPathConfiguredValuesAreEmitted`,
`GeneratedConfigTest.postingHeaderTemplates*`,
`GeneratedConfigTest.postingHeaderTemplateConfiguredValuesAreEmitted`,
`GeneratedConfigTest.replyAndForwardTemplates*`,
`GeneratedConfigTest.postingHelpersUseInstalledHelperPaths`, and
`GeneratedConfigTest.featurePolicyConfiguredValuesAreEmitted`.

### UI And Runtime Behavior Variables

- `MBOX_CHAR`: mailbox From-line marker.
- `UNSUBSCRIBED_CHAR`: newsrc unsubscribe marker.
- `SELECTION_CHARS`: selector item characters.
- `LOCALTIME_FMT`: local-time display format.
- `MAIL_CALL`, `NO_FIREWORKS`, `TILDE_NAME`, `VALIDATE_XREF_SITE`, and
  `USE_FTP`: retained runtime feature policy.
- `TRN_MARK_XREFS_UNREAD`: generated `MCHASE` xref-unmarking policy.
- `THREAD_INIT` and `SELECT_INIT`: startup mode defaults.
- `HAS_TERMLIB`: termlib capability override after CMake probes support.
- `DEBUG`: compile debug logging support.

Coverage: `ConfigCMakeTest.unsubscribedCharAcceptsSingleCharacter`,
`ConfigCMakeTest.unsubscribedCharRejectsEmpty`,
`ConfigCMakeTest.unsubscribedCharRejectsMultipleCharacters`,
`ConfigCMakeTest.termlibDefaultUsesFoundProbe`,
`ConfigCMakeTest.termlibDefaultUsesMissingProbe`,
`ConfigCMakeTest.termlibOverrideCanEnableMissingProbe`,
`ConfigCMakeTest.termlibOverrideCanDisableFoundProbe`,
`GeneratedConfigTest.selectionPolicyDefaultMatchesHistoricalFallbacks`,
`GeneratedConfigTest.selectionPolicyConfiguredValuesAreEmitted`,
`GeneratedConfigTest.localTimeFormatDefaultMatchesHistoricalFallback`,
`GeneratedConfigTest.localTimeFormatConfiguredValueIsEmitted`,
`GeneratedConfigTest.featurePolicyDefaultsMatchHistoricalFallbacks`,
`GeneratedConfigTest.featurePolicyConfiguredValuesAreEmitted`,
`GeneratedConfigTest.termlibProbeMatchesPlatformCheck`,
`SelectorPageTest.outputSelectorUsesConfiguredCharacterOrder`, and
`tests/test_bits.cpp` plus `tests/test_rcln.cpp` when `MCHASE` is
enabled.
Untested: `MBOX_CHAR`, `THREAD_INIT`, `SELECT_INIT`, and `DEBUG`.

## Install

Install the configured build tree with:

```console
cmake --install ../build-default --config Debug
```

Add `--prefix <directory>` to stage the install tree somewhere other than
the configured install prefix.

The install tree includes the built commands, generated posting helpers,
generated private helper/data files, `HelpFiles`, and the supported man
pages for `trn`, `Pnews`, and `Rnmail`.

The supported POSIX runtime layout is:

- `${prefix}/${bindir}`: `trn`, `inews`, `nntplist`, `trn-artchk`,
  `Pnews`, and `Rnmail`.
- `${prefix}/${private-lib}`: `mbox.saver`, `norm.saver`, `Speller`,
  `Pnews.header`, `INIT`, `access.def`, `environment`, `newsnews`, and
  `HelpFiles`.
- `${prefix}/${mandir}/man1`: `trn.1`, `Pnews.1`, and `Rnmail.1`.

On Windows, the helper entry points are installed as `Pnews.cmd`,
`Rnmail.cmd`, `mbox.saver.cmd`, `norm.saver.cmd`, and `Speller.cmd`.
`norm.saver.cmd` and `mbox.saver.cmd` invoke their adjacent `.ps1` scripts
using Windows PowerShell 5.1.  The normal saver takes seven arguments;
the mailbox saver adds a complete From envelope as its eighth argument.
It supports the configured `MBOX_CHAR` format, including control-A
separators.  Both preserve article bytes apart from required mailbox
escaping and report argument or I/O failures.  Saves remain internal to
trn unless `NORMSAVER` or `MBOXSAVER` contains an explicit command; see
their entries in `man/trn.1` for examples.

`Speller.cmd` invokes its adjacent `Speller.ps1` with Windows PowerShell
5.1.  Pass one or more draft filenames.  It checks Subject, Keywords,
Summary, their continuations, and unquoted body text; other headers,
quoted lines, and signatures remain protected.  Native checker failures
leave the draft unchanged.  The checker receives a temporary file in the
draft's original byte encoding and must preserve that encoding.

Set `ISPELL_PROGRAM` to an in-place native checker, with arguments in
`ISPELL_OPTIONS`.  Use `ISPELL_PROGRAM=none` and `SPELL_PROGRAM` for a
word-list checker; its output goes to `PAGER`.  Environment settings
override the CMake defaults on both platforms.  On Windows, checker
program values are executable paths, without options or surrounding
quotes; options and `PAGER` accept whitespace-separated arguments with
single or double quotes around an entire argument.  No shell expressions
are evaluated.  `QUOTECHARS` is an anchored regular expression: .NET on
Windows, POSIX basic on Unix.  Common patterns such as `>` and `[>|:]`
work on both.  An unset or empty value checks quoted lines too.

`Pnews.cmd` invokes its adjacent `Pnews.ps1` with PowerShell 5.1.
It incorporates header generation, distribution prompts, prepared bodies,
signatures, editing, validation, spelling, optional PGP, and draft
recovery.  It submits the article to the adjacent C++ `inews.exe -h`
through a byte stream.  Set a qualified `POSTING_HOSTNAME`, or provide
the configured posting-hostname site file, before using native `inews`.
`USE_INEWS` still selects a custom submission command at configure time.

Windows `INEWS`, `ARTCHECK`, `NNTPLIST`, and `SPELLER` environment settings
override the corresponding tool commands.  Editor, pager, and PGP
commands accept a path or quoted arguments; streamed tools must be native
executables or PowerShell scripts.  `NEWSLIB` selects the distribution
library.  `AUTHORCOPY` uses installed `mbox.saver.cmd`, with `POSTSAVER`
as an override.  Failures retain `.article` and `dead.article`; see
`man/Pnews.1` for the command and recovery contracts.

Windows email copies and unapproved moderated posts retain their drafts
with a clear diagnostic until the email backend is implemented.
`Rnmail.cmd` still reports that sending email is unsupported.
Unix packages retain `Pnews` and `Pnews.header`; Windows folds the
scriptlet's behavior into the PowerShell composer.  Shared native tool
support, `trn-tools.ps1`, is installed in the private library.

`strn.1` was a stale historical stub and is retired.  `default_topic` is
a source-distribution sample for sites that want a default scan-mode topic
file; CMake does not install it automatically.

## Package

Build first, then create the default package artifacts:

```console
cpack --preset default
```

On Windows, the default package includes a portable ZIP.  If CPack can
find the NSIS packaging toolchain, the default package command also emits
a Windows installer.  The portable ZIP can be built explicitly with:

```console
cpack --preset windows-zip
```

Packages use the same CMake install rules as `cmake --install`: built
commands, generated helper and data files, `HelpFiles`, and supported man
pages are taken from the install tree.  Package roots include
`ReadMe.md`, `INSTALL.md`, `LICENSE.txt`, and `HACKERSGUIDE.txt`.
Top-level `INIT.txt` is intentionally omitted because the runtime `INIT`
is installed in the private library.  `default_topic` remains a
source-distribution sample and is not packaged.

## Runtime Setup

The old install flow asked installers to choose how trn should read news.
That choice still belongs in runtime configuration:

- News can come from an NNTP server or from a local spool.
- Overview data normally comes from the server or local spool.
- Groups can be read unthreaded when no overview data is available, with
  slower startup.
- Posting helpers and site data are installed by CMake.

After installation, run `trn` and use the online options editor to save
personal defaults in `.trn/trnrc`.  Site defaults belong in the installed
INIT file.
