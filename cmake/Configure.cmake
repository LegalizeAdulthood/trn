#
# Perform inspections of the system and configure accordingly.
#
function(configure)
    set(DIRENTRYTYPE "struct dirent")
    set(EMULATE_NDIR ON)
    set(HAS_FTIME ON)
    set(HAS_GETCWD ON)
    set(HAS_GETHOSTNAME ON)
    set(HAS_MKDIR ON)
    set(HAS_RENAME ON)
    set(HAS_TERMLIB ON)
    set(I_TIME ON)
    set(MBOX_CHAR "F")
    set(MSDOS ON)
    set(NEWS_ADMIN "root")
    set(NORMSIG ON)
    set(ROOT_ID "0")
    set(SCAN ON)
    set(SCORE ON)
    set(SERVER_NAME "news.gmane.io")
    set(SIGNAL_T "void")
    set(SUPPORT_NNTP ON)
    set(SUPPORT_XTHREAD ON)
    set(TRN_INIT OFF)
    set(TRN_SELECT ON)
    set(USE_GENAUTH ON)
    configure_file(config.h.in config.h)
endfunction()
