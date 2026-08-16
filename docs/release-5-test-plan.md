# Release 5.0 Test Plan Matrix

This is the tracked inventory for issue #33.  It records the release
checklist item, the current status, and the concrete verification path.

Status key:

- Automated: covered by an existing unit, integration, CMake, or package
  test.
- Manual: requires an explicit release smoke test.
- Deferred: tracked here but intentionally left for a later release-5
  slice.
- Obsolete: no longer supported behavior; no release gate remains.

## Shared Evidence

- Built help: `libtrn/help.cpp` and `HelpFiles/top`.
- External help: `HelpFiles/config/options`,
  `HelpFiles/config/macros`, `HelpFiles/samples/access`, and
  `HelpFiles/samples/trnrc`.
- Man pages: `man/trn.1`, `man/Pnews.1`, and `man/Rnmail.1`.
- Option surfaces: `libtrn/OptionCatalog.cpp`, `libtrn/sw.cpp`,
  `tests/test_OptionCatalog.cpp`, `tests/test_OptionApplier.cpp`,
  `tests/test_opt.cpp`, and `tests/test_sw.cpp`.
- Access surfaces: `libtrn/DataSourceConfig.cpp`,
  `config/data/access_def.in`, `HelpFiles/samples/access`,
  `tests/test_DataSourceConfig.cpp`, `tests/test_datasrc.cpp`, and
  `tests/cmake/test_access_file.in`.

## Configuration Files

- `access`: Manual plus automated parser coverage.  Verify with
  `tests/test_DataSourceConfig.cpp` for schema and value parsing,
  `tests/test_datasrc.cpp` for source selection and auth behavior, and a
  manual comparison of `config/data/access_def.in`,
  `HelpFiles/samples/access`, `tests/cmake/test_access_file.in`, and
  `man/trn.1`.
- Global KILL file: Automated plus manual.  Verify default and configured
  global paths in `tests/test_kfile.cpp`, regex behavior in
  `tests/test_search.cpp`, and one manual startup check with `KILLGLOBAL`.
- Local KILL file: Automated plus manual.  Verify local path creation and
  command application in `tests/test_kfile.cpp`, then run one manual
  newsgroup check with `KILLLOCAL`.

## Environment Variables

The generated-config rows use `config/tests/test_generated_config.cpp`.
Runtime override rows use `tests/test_sw.cpp` for `-E` parsing and the
named runtime test for behavior.

- `ATTRIBUTION`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, reply/followup text in
  `tests/test_interp.cpp`, and one manual reply check.
- `AUTOSUBSCRIBE`: Automated.  Verify matching behavior in
  `tests/test_autosub.cpp`.
- `AUTOUNSUBSCRIBE`: Automated.  Verify matching behavior in
  `tests/test_autosub.cpp`.
- `CANCEL`: Manual plus automated.  Verify cancel command interpolation in
  `tests/test_interp.cpp` and run one manual cancel-flow smoke test.
- `CANCELHEADER`: Automated plus manual.  Verify configured header text in
  `config/tests/test_generated_config.cpp` and cancel output in
  `tests/test_interp.cpp`.
- `DOTDIR`: Automated.  Verify startup and expansion behavior in
  `tests/test_init.cpp` and `tests/test_interp.cpp`.
- `EDITOR`: Manual plus CMake default coverage.  Verify default tool
  probing in `config/tests/cmake/test_tool_path_defaults.cmake`, then run
  one manual editor-launch smoke test.
- `EXSAVER`: Manual.  Verify the documented extraction command behavior
  with an `e` extract smoke test and compare `man/trn.1` with
  `config/data/environment.in`.
- `FIRSTLINE`: Automated plus manual.  Verify display interpolation in
  `tests/test_interp.cpp` and run one manual article display check.
- `FORWARDHEADER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp` and forward output in
  `tests/test_interp.cpp`.
- `FORWARDPOSTER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp` and run one manual forward
  smoke test.
- `HIDELINE`: Automated plus manual.  Verify runtime use in
  `tests/test_interp.cpp` and run one manual hidden-header display check.
- `HOME`: Automated.  Verify startup path selection in
  `tests/test_init.cpp`.
- `KILLGLOBAL`: Automated plus manual.  Verify global KILL path handling in
  `tests/test_kfile.cpp` and generated defaults in
  `config/tests/test_generated_config.cpp`.
- `KILLLOCAL`: Automated plus manual.  Verify local KILL path handling in
  `tests/test_kfile.cpp` and generated defaults in
  `config/tests/test_generated_config.cpp`.
- `LOGDIR`: Automated.  Verify fallback home-directory selection in
  `tests/test_init.cpp`.
- `LOGNAME`: Automated.  Verify login-name selection in
  `tests/test_init.cpp`.
- `LOCALTIMEFMT`: Automated.  Verify article date formatting in
  `tests/test_interp.cpp`.
- `MAILCALL`: Automated plus manual.  Verify pager-prompt interpolation in
  `tests/test_interp.cpp` and generated defaults in
  `config/tests/test_generated_config.cpp`.
- `MAILFILE`: Automated plus manual.  Verify configured defaults in
  `config/tests/test_generated_config.cpp` and run one manual mail-call
  smoke test.
- `MAILHEADER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, mail reply output in
  `tests/test_interp.cpp`, and `man/Rnmail.1`.
- `MAILPOSTER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, mail reply behavior in
  `tests/test_interp.cpp`, and `man/Rnmail.1`.
- `MBOXSAVER`: Automated plus manual.  Verify mailbox save behavior in
  `tests/test_interp.cpp` and run one manual mailbox-save smoke test.
- `MODSTRING`: Manual plus runtime coverage.  Verify moderated-group text
  through `tests/test_datasrc.cpp` data-source setup and one manual group
  display check.
- `NAME`: Automated.  Verify real-name override behavior in
  `tests/test_interp.cpp` and name fallback behavior in
  `tests/test_init.cpp`.
- `NEWSHEADER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, followup output in
  `tests/test_interp.cpp`, and `man/Pnews.1`.
- `NEWSORG`: Automated.  Verify organization lookup in
  `tests/test_interp.cpp`.
- `NEWSPOSTER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, followup behavior in
  `tests/test_interp.cpp`, and `man/Pnews.1`.
- `NNTPSERVER`: Automated plus deferred fixture work.  Verify current
  source selection in `tests/test_datasrc.cpp`; deterministic protocol
  fixture coverage is slice 24.
- `NOPOSTRING`: Manual plus runtime coverage.  Verify no-posting group text
  through `tests/test_datasrc.cpp` data-source setup and one manual group
  display check.
- `NORMSAVER`: Automated plus manual.  Verify normal save behavior in
  `tests/test_interp.cpp` and run one manual normal-save smoke test.
- `ORGANIZATION`: Automated.  Verify organization lookup in
  `tests/test_interp.cpp`.
- `PAGESTOP`: Manual.  Verify pager stop behavior with one manual article
  paging smoke test and compare `man/trn.1` with
  `config/data/environment.in`.
- `PIPESAVER`: Automated plus manual.  Verify pipe-save interpolation in
  `tests/test_interp.cpp` and generated defaults in
  `config/tests/test_generated_config.cpp`.
- `REPLYTO`: Automated plus manual.  Verify reply header behavior in
  `tests/test_interp.cpp` and run one manual reply smoke test.
- `RNINIT`: Automated plus manual.  Verify switch parsing in
  `tests/test_sw.cpp`, startup selection in `libtrn/opt.cpp`, and one
  manual rn-mode startup check.
- `RNMACRO`: Automated plus manual.  Verify generated default paths in
  `config/tests/test_generated_config.cpp`, macro loading in
  `tests/test_terminal.cpp`, and one manual macro-file check.
- `SAVEDIR`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, save path behavior in
  `tests/test_opt.cpp` and `tests/test_interp.cpp`, and one manual save.
- `SAVENAME`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp`, save-name behavior in
  `tests/test_opt.cpp`, and one manual save.
- `SELECTCHARS`: Automated plus manual.  Verify selector commands in
  `tests/test_rt-select.cpp` and run one manual selector selection check.
- `SHELL`: Manual plus CMake default coverage.  Verify shell-start defaults
  in `config/tests/cmake/test_tool_path_defaults.cmake` and one manual
  shell escape.
- `SUBJLINE`: Automated.  Verify subject-line override behavior in
  `tests/test_subject.cpp`.
- `SUPERSEDEHEADER`: Automated plus manual.  Verify generated defaults in
  `config/tests/test_generated_config.cpp` and supersede output in
  `tests/test_interp.cpp`.
- `TERM`: Manual plus terminal tests.  Verify terminal behavior in
  `tests/test_terminal.cpp` and run one manual terminal smoke test.
- `TERMCAP`: Manual plus CMake probe coverage.  Verify termlib probing in
  `config/tests/cmake/test_termlib_default.cmake` and run one manual
  terminal smoke test.
- `TRNINIT`: Automated plus manual.  Verify switch parsing in
  `tests/test_sw.cpp`, startup selection in `libtrn/opt.cpp`, and one
  manual trn-mode startup check.
- `TRNMACRO`: Automated plus manual.  Verify generated default paths in
  `config/tests/test_generated_config.cpp`, macro loading in
  `tests/test_terminal.cpp`, and one manual macro-file check.
- `UNSHAR`: Manual.  Verify shell-archive extraction with one manual `e`
  command smoke test and compare `HelpFiles/hints` with `man/trn.1`.
- `USER`: Automated.  Verify login-name selection in
  `tests/test_init.cpp`.
- `VISUAL`: Manual plus CMake default coverage.  Verify editor probing in
  `config/tests/cmake/test_tool_path_defaults.cmake` and one manual
  editor-launch smoke test.
- `XTERMMOUSE`: Automated plus manual.  Verify environment behavior in
  `tests/test_terminal.cpp` and run one manual xterm mouse check.
- `YOUSAID`: Automated plus manual.  Verify reply attribution in
  `tests/test_interp.cpp` and one manual reply smoke test.

## User Interface

- Newsgroup selection: Automated plus manual.  Verify startup and group
  navigation in `tests/test_trn.cpp`, add-group scanning in
  `tests/test_addng.cpp`, and one manual source-selection smoke test.
- Thread selection: Automated plus manual.  Verify selector command input
  in `tests/test_rt-select.cpp`, selector files in `tests/test_univ.cpp`,
  and one manual threaded-group smoke test.
- Article reading: Automated plus manual.  Verify pager commands in
  `tests/test_art.cpp`, article display interpolation in
  `tests/test_interp.cpp`, and one manual read-through smoke test.
- Paging: Automated plus manual.  Verify pager command parsing in
  `tests/test_art.cpp`, terminal input in `tests/test_terminal.cpp`, and
  one manual long-article paging smoke test.
- Charset conversion: Deferred.  Current header decoding coverage lives in
  `tests/test_cache.cpp` and MIME decoding in `tests/test_mime.cpp`; the
  release-5 charset pipeline work is tracked by slices 31 through 35.
- `e` shell-archive extraction: Manual.  Verify with an article containing
  a shell archive, `UNSHAR`, `HelpFiles/hints`, and the extract sections in
  `man/trn.1`.
- `e` uuencoded binary extraction: Automated plus manual.  Verify prescan
  and decoding in `tests/test_uudecode.cpp`, piece handling in
  `tests/test_decode.cpp`, and one manual multipart extract.
- `e` yencoded binary extraction: Deferred.  yEnc implementation and tests
  are tracked by slice 36.
- Posting: Automated plus manual.  Verify cancel, supersede, reply,
  forward, and followup output in `tests/test_interp.cpp`, generated helper
  defaults in `config/tests/test_generated_config.cpp`, and one manual
  `Pnews` smoke test.
- Command-line options: Automated plus manual.  Verify switch parsing in
  `tests/test_sw.cpp`, option application in `tests/test_opt.cpp`,
  `tests/test_OptionCatalog.cpp`, and `tests/test_OptionApplier.cpp`, then
  run one manual command-line startup check.
- Macros: Automated plus manual.  Verify macro definition and expansion in
  `tests/test_ngstuff.cpp` and `tests/test_terminal.cpp`, with
  documentation comparison against `HelpFiles/config/macros`.
- Percent expansion: Automated plus manual.  Verify expansion coverage in
  `tests/test_interp.cpp`, escape handling in `tests/test_ngstuff.cpp`, and
  one manual command/reply expansion check.

## Documentation Drift

- Access-file documentation drift: Manual.  Compare
  `libtrn/DataSourceConfig.cpp`, `config/data/access_def.in`,
  `HelpFiles/samples/access`, `tests/cmake/test_access_file.in`, and
  `man/trn.1`; missing access-schema fields remain tracked by later
  release-5 access slices.
- Option documentation drift: Manual.  Compare `libtrn/OptionCatalog.cpp`,
  `libtrn/sw.cpp`, `HelpFiles/config/options`, `HelpFiles/config/macros`,
  and `man/trn.1`; parser behavior is covered by `tests/test_sw.cpp` and
  option behavior by `tests/test_opt.cpp`.
- Missing scoring help: Deferred.  Existing scoring behavior is covered by
  `tests/test_score-easy.cpp`, `tests/test_scorefile.cpp`,
  `tests/test_scoresave.cpp`, and `tests/test_scmd.cpp`, but the help text
  is not a complete current guide to the scoring option surface.
- Stale selector help: Obsolete as a release gate.  The old
  `HelpFiles/changelog` placeholder is historical text; current selector
  behavior should be verified against `libtrn/OptionCatalog.cpp`,
  `tests/test_rt-select.cpp`, `tests/test_univ.cpp`, `HelpFiles/top`,
  `HelpFiles/config/options`, and `man/trn.1`.
