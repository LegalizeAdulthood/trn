# Things To Do

* Look for fixed-size array limits that can be eliminated
* Replace all POSIX filesystem manipulation with std::filesystem
* Validate that all conditional compilation blocks correspond to a CMake
  option
* Eliminate the push/pop macro garbage in the universal selector
* Eliminate all MSDOS conditional branches
* Review default setting of MAIL_FILE and entire "checking mail" mechanism
  on Windows
* Review default setting of NEWS_LIB on Windows
* Review default settings of active-file paths on Windows: ACTIVE and
  ACTIVE_TIMES
* Review default settings of news-source paths on Windows: GROUP_DESC,
  SUBSCRIPTIONS, NEWS_SPOOL, OVERVIEW_DIR, OVERVIEW_FMT, EXTRA_INEWS,
  ORG_NAME, POSTING_HOSTNAME, and PRIVATE_LIB
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
  PASS_NAMES, BERKELEY_NAMES, NAME_TYPE, LOGIN_DIR_FIELD, and
  LOCALTIME_FMT
* Review name lookup defaults on modern Linux, BSD, and Windows,
  including PASS_NAMES, BERKELEY_NAMES, NAME_TYPE, and matching generated
  posting helper behavior
