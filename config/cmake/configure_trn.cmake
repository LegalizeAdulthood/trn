include(CheckCXXSourceCompiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CMakePushCheckState)

function(configure_flavor_header name flavor)
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${name}.${flavor}.h.in"
        "include/config/${name}.h"
        COPYONLY)
endfunction()

function(set_config_cache_default name value type doc)
    set(last_default_name "TRN_LAST_DEFAULT_${name}")
    set(update_default OFF)

    if(DEFINED CACHE{${name}})
        if(DEFINED CACHE{${last_default_name}} AND "${${name}}" STREQUAL "${${last_default_name}}")
            set(update_default ON)
        else()
            foreach(legacy_default IN LISTS ARGN)
                if("${${name}}" STREQUAL "${legacy_default}")
                    set(update_default ON)
                endif()
            endforeach()
        endif()
    endif()

    if(update_default)
        set(${name} "${value}" CACHE ${type} "${doc}" FORCE)
    else()
        set(${name} "${value}" CACHE ${type} "${doc}")
    endif()
    set(${last_default_name} "${value}" CACHE INTERNAL "Last default value for ${name}.")
endfunction()

function(configure_trn_tool_path_default tool_path variable missing_value)
    if(tool_path)
        set("${variable}" "${tool_path}" PARENT_SCOPE)
    else()
        set("${variable}" "${missing_value}" PARENT_SCOPE)
    endif()
endfunction()

function(configure_trn_verification_command_default tool_path arguments variable)
    if(tool_path)
        set("${variable}" "${tool_path} ${arguments}" PARENT_SCOPE)
    else()
        set("${variable}" "" PARENT_SCOPE)
    endif()
endfunction()

function(validate_posting_host_match_segments value)
    if(NOT "${value}" MATCHES "^(0|[1-9][0-9]*)$")
        message(FATAL_ERROR
            "Option POSTING_HOST_MATCH_SEGMENTS ('${value}') must be a non-negative decimal integer.")
    endif()
endfunction()

function(validate_unsubscribed_char value)
    string(LENGTH "${value}" value_length)
    if(NOT (value_length EQUAL 1))
        message(FATAL_ERROR "Option UNSUBSCRIBED_CHAR ('${value}') must be set to a single character.")
    endif()
endfunction()

function(validate_name_type value)
    if(NOT "${value}" MATCHES "^(bsd|usg|other)$")
        message(FATAL_ERROR "Option NAME_TYPE ('${value}') must be bsd, usg, or other.")
    endif()
endfunction()

function(configure_name_parsing_policy name_type)
    validate_name_type("${name_type}")
    unset(PASS_NAMES CACHE)
    unset(BERKELEY_NAMES CACHE)
    if("${name_type}" STREQUAL "bsd")
        set(PASS_NAMES ON PARENT_SCOPE)
        set(BERKELEY_NAMES ON PARENT_SCOPE)
    elseif("${name_type}" STREQUAL "usg")
        set(PASS_NAMES ON PARENT_SCOPE)
        set(BERKELEY_NAMES OFF PARENT_SCOPE)
    else()
        set(PASS_NAMES OFF PARENT_SCOPE)
        set(BERKELEY_NAMES OFF PARENT_SCOPE)
    endif()
endfunction()

function(configure_name_parsing_default has_getpwent probed_name_type)
    if(NOT has_getpwent)
        set(DEFAULT_NAME_TYPE "other" PARENT_SCOPE)
    elseif("${probed_name_type}" MATCHES "^(bsd|usg|other)$")
        set(DEFAULT_NAME_TYPE "${probed_name_type}" PARENT_SCOPE)
    else()
        set(DEFAULT_NAME_TYPE "other" PARENT_SCOPE)
    endif()
endfunction()

function(probe_name_type_default has_getpwent)
    if(NOT has_getpwent OR CMAKE_CROSSCOMPILING)
        set(PROBED_NAME_TYPE "other" PARENT_SCOPE)
        return()
    endif()

    set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/name_type_probe")
    set(probe_source "${probe_dir}/name_type_probe.c")
    file(MAKE_DIRECTORY "${probe_dir}")
    file(WRITE "${probe_source}" [=[
#include <pwd.h>
#include <stdio.h>
#include <string.h>

static int has_name_text(const char *gecos)
{
    if (gecos == 0)
    {
        return 0;
    }
    for (; *gecos != '\0'; ++gecos)
    {
        if (*gecos != ' ' && *gecos != '\t' && *gecos != ','
            && *gecos != ':' && *gecos != ';')
        {
            return 1;
        }
    }
    return 0;
}

static int looks_usg(const char *gecos)
{
    const char *hyphen = strchr(gecos, '-');
    const char *paren = strchr(gecos, '(');

    return hyphen != 0 && paren != 0 && hyphen < paren;
}

int main(void)
{
    struct passwd *pwd;

    while ((pwd = getpwent()) != 0)
    {
        if (!has_name_text(pwd->pw_gecos))
        {
            continue;
        }
        if (looks_usg(pwd->pw_gecos))
        {
            puts("usg");
        }
        else
        {
            puts("bsd");
        }
        endpwent();
        return 0;
    }
    endpwent();
    puts("other");
    return 0;
}
]=])
    try_run(NAME_TYPE_PROBE_RUN_RESULT NAME_TYPE_PROBE_COMPILE_RESULT
        "${probe_dir}"
        "${probe_source}"
        RUN_OUTPUT_VARIABLE NAME_TYPE_PROBE_OUTPUT)
    if(NAME_TYPE_PROBE_COMPILE_RESULT AND NAME_TYPE_PROBE_RUN_RESULT EQUAL 0)
        string(STRIP "${NAME_TYPE_PROBE_OUTPUT}" PROBED_NAME_TYPE)
    else()
        set(PROBED_NAME_TYPE "other")
    endif()
    configure_name_parsing_default("${has_getpwent}" "${PROBED_NAME_TYPE}")
    set(PROBED_NAME_TYPE "${DEFAULT_NAME_TYPE}" PARENT_SCOPE)
endfunction()

function(validate_name_parsing_support name_type has_getpwent)
    configure_name_parsing_policy("${name_type}")
    if(PASS_NAMES AND NOT has_getpwent)
        message(FATAL_ERROR "NAME_TYPE=${name_type} requires getpwent support.")
    endif()
    set(PASS_NAMES "${PASS_NAMES}" PARENT_SCOPE)
    set(BERKELEY_NAMES "${BERKELEY_NAMES}" PARENT_SCOPE)
endfunction()

function(validate_distribution_code name value)
    if("${value}" STREQUAL "")
        message(FATAL_ERROR
            "Option ${name} must be non-empty; use 'none' when no distribution code is available.")
    endif()
endfunction()

function(configure_save_path_defaults make_save_directories)
    if(make_save_directories)
        set(DEFAULT_SAVEDIR "%p/%c" PARENT_SCOPE)
        set(DEFAULT_SAVENAME "%a" PARENT_SCOPE)
    else()
        set(DEFAULT_SAVEDIR "%p" PARENT_SCOPE)
        set(DEFAULT_SAVENAME "%^C" PARENT_SCOPE)
    endif()
endfunction()

function(configure_termlib_default probed_has_termlib)
    if(probed_has_termlib)
        set(DEFAULT_HAS_TERMLIB ON PARENT_SCOPE)
    else()
        set(DEFAULT_HAS_TERMLIB OFF PARENT_SCOPE)
    endif()
endfunction()

function(configure_termlib_cache_default)
    set_config_cache_default(HAS_TERMLIB "${DEFAULT_HAS_TERMLIB}" BOOL
        "Indicates that termlib-style routines are available")
endfunction()

function(probe_termlib_default)
    if(WIN32)
        configure_termlib_default(ON)
        set(DEFAULT_HAS_TERMLIB "${DEFAULT_HAS_TERMLIB}" PARENT_SCOPE)
        return()
    endif()

    if(TARGET curses)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_LIBRARIES curses)
        check_cxx_source_compiles([=[
extern "C" int tputs(const char *, int, int (*)(int));
int putchr(int ch)
{
    return ch;
}
int main()
{
    return tputs("", 1, putchr);
}
]=] TRN_HAS_TERMLIB_TPUTS)
        cmake_pop_check_state()
    else()
        set(TRN_HAS_TERMLIB_TPUTS OFF)
    endif()

    configure_termlib_default("${TRN_HAS_TERMLIB_TPUTS}")
    set(DEFAULT_HAS_TERMLIB "${DEFAULT_HAS_TERMLIB}" PARENT_SCOPE)
endfunction()

function(read_default_nntp_server_file output)
    if(DEFINED TRN_NNTP_SERVER_FILE_CANDIDATES)
        set(candidates ${TRN_NNTP_SERVER_FILE_CANDIDATES})
    else()
        set(candidates "/etc/nntpserver" "/usr/local/etc/nntpserver")
    endif()

    foreach(candidate IN LISTS candidates)
        if(EXISTS "${candidate}" AND NOT IS_DIRECTORY "${candidate}")
            file(STRINGS "${candidate}" lines REGEX "^[ \t]*[^# \t].*")
            if(lines)
                list(GET lines 0 server_name)
                string(STRIP "${server_name}" server_name)
                if(NOT "${server_name}" STREQUAL "")
                    set(${output} "${server_name}" PARENT_SCOPE)
                    return()
                endif()
            endif()
        endif()
    endforeach()

    set(${output} "" PARENT_SCOPE)
endfunction()

function(probe_default_nntp_server)
    if(NOT TRN_NEWS_SOURCE_IGNORE_ENV
        AND DEFINED ENV{NNTPSERVER}
        AND NOT "$ENV{NNTPSERVER}" STREQUAL "")
        set(PROBED_SERVER_NAME "$ENV{NNTPSERVER}" PARENT_SCOPE)
        return()
    endif()

    read_default_nntp_server_file(server_name)
    set(PROBED_SERVER_NAME "${server_name}" PARENT_SCOPE)
endfunction()

function(probe_local_news_spool)
    if(DEFINED TRN_NEWS_LIB_CANDIDATES)
        set(news_lib_candidates ${TRN_NEWS_LIB_CANDIDATES})
    else()
        set(news_lib_candidates "/usr/lib/news" "/var/lib/news")
    endif()

    if(DEFINED TRN_NEWS_SPOOL_CANDIDATES)
        set(news_spool_candidates ${TRN_NEWS_SPOOL_CANDIDATES})
    else()
        set(news_spool_candidates "/usr/spool/news" "/var/spool/news")
    endif()

    foreach(news_lib IN LISTS news_lib_candidates)
        if(EXISTS "${news_lib}/active" AND NOT IS_DIRECTORY "${news_lib}/active")
            foreach(news_spool IN LISTS news_spool_candidates)
                if(IS_DIRECTORY "${news_spool}")
                    set(PROBED_HAS_LOCAL_SPOOL ON PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endif()
    endforeach()

    set(PROBED_HAS_LOCAL_SPOOL OFF PARENT_SCOPE)
endfunction()

function(configure_news_source_cache_defaults detected_server_name detected_has_local_spool)
    if(NOT "${detected_server_name}" STREQUAL "")
        set(DEFAULT_SERVER_NAME "${detected_server_name}" PARENT_SCOPE)
    elseif(detected_has_local_spool)
        set(DEFAULT_SERVER_NAME "local" PARENT_SCOPE)
    else()
        set(DEFAULT_SERVER_NAME "news.gmane.io" PARENT_SCOPE)
    endif()
    set(DEFAULT_HAS_LOCAL_SPOOL "${detected_has_local_spool}" PARENT_SCOPE)
endfunction()

function(probe_news_source_defaults)
    probe_default_nntp_server()
    probe_local_news_spool()
    configure_news_source_cache_defaults("${PROBED_SERVER_NAME}" "${PROBED_HAS_LOCAL_SPOOL}")
    set(DEFAULT_SERVER_NAME "${DEFAULT_SERVER_NAME}" PARENT_SCOPE)
    set(DEFAULT_HAS_LOCAL_SPOOL "${DEFAULT_HAS_LOCAL_SPOOL}" PARENT_SCOPE)
endfunction()

function(configure_news_source_defaults)
    if("${SERVER_NAME}" STREQUAL "local" AND NOT HAS_LOCAL_SPOOL)
        message(FATAL_ERROR "Option SERVER_NAME is 'local', so HAS_LOCAL_SPOOL must be ON.")
    endif()

    if("${SERVER_NAME}" STREQUAL "local")
        set(DEFAULT_ACTIVE "%x/active" PARENT_SCOPE)
        set(DEFAULT_GROUP_DESC "%x/newsgroups" PARENT_SCOPE)
        set(DEFAULT_SUBSCRIPTIONS "%x/subscriptions" PARENT_SCOPE)
        set(DEFAULT_NEWS_SPOOL "/usr/spool/news" PARENT_SCOPE)
    else()
        set(DEFAULT_ACTIVE "remote" PARENT_SCOPE)
        set(DEFAULT_GROUP_DESC "none" PARENT_SCOPE)
        set(DEFAULT_SUBSCRIPTIONS "" PARENT_SCOPE)
        set(DEFAULT_NEWS_SPOOL "none" PARENT_SCOPE)
    endif()
endfunction()

function(configure_news_overview_dir_default)
    if("${SERVER_NAME}" STREQUAL "local")
        set(DEFAULT_OVERVIEW_DIR "${NEWS_SPOOL}" PARENT_SCOPE)
    else()
        set(DEFAULT_OVERVIEW_DIR "remote" PARENT_SCOPE)
    endif()
endfunction()

function(configure_news_overview_format_default)
    if("${OVERVIEW_DIR}" STREQUAL ""
        OR "${OVERVIEW_DIR}" STREQUAL "none"
        OR "${OVERVIEW_DIR}" STREQUAL "remote")
        set(DEFAULT_OVERVIEW_FMT "none" PARENT_SCOPE)
    else()
        set(DEFAULT_OVERVIEW_FMT "%x/overview.fmt" PARENT_SCOPE)
    endif()
endfunction()

function(configure_trn)
    #
    # Perform inspections of the system and configure accordingly.
    #
    check_include_file(fcntl.h      I_FCNTL)
    check_include_file(io.h         I_IO)
    check_include_file(pwd.h        I_PWD)
    check_include_file(sgtty.h      I_SGTTY)
    check_include_file(strings.h    I_STRINGS)
    check_include_file(sys/filio.h  I_SYS_FILIO)
    check_include_file(sys/ioctl.h  I_SYS_IOCTL)
    check_include_file(sys/stat.h   I_SYS_STAT)
    check_include_file(sys/utime.h  I_SYS_UTIME)
    check_include_file(sys/wait.h   I_SYS_WAIT)
    check_include_file(termcap.h    I_TERMCAP)
    check_include_file(termios.h    I_TERMIOS)
    check_include_file(unistd.h     I_UNISTD)
    check_include_file(utime.h      I_UTIME)
    check_include_file(vfork.h      I_VFORK)

    if(I_UNISTD)
        check_symbol_exists(rdchk "unistd.h" HAS_RDCHK)
    endif()
    check_symbol_exists(sigblock "signal.h" HAS_SIGBLOCK)
    check_symbol_exists(sighold "signal.h" HAS_SIGHOLD)

    # Determine method used for file descriptor I/O
    if(I_FCNTL)
        check_symbol_exists(open "fcntl.h" HAS_FCNTL_OPEN)
        if(HAS_FCNTL_OPEN)
            set(FDIO_FLAVOR "fcntl")
        endif()
    endif()
    if(NOT FDIO_FLAVOR AND I_IO)
        check_symbol_exists(open "io.h" HAS_IO_OPEN)
        if(HAS_IO_OPEN)
            set(FDIO_FLAVOR "io")
        endif()
    endif()
    if(NOT FDIO_FLAVOR)
        message(FATAL_ERROR "Couldn't find open() in <fcntl.h> or <io.h>; file descriptor I/O not found.")
    endif()

    # Pipe open/close functions
    check_symbol_exists(_popen "stdio.h" HAS_UNDERSCORE_POPEN)
    if(HAS_UNDERSCORE_POPEN)
        set(PIPE_IO_FLAVOR "underscore")
    endif()
    if(NOT PIPE_IO_FLAVOR)
        check_symbol_exists(popen "stdio.h" HAS_STANDARD_POPEN)
        if(HAS_STANDARD_POPEN)
            set(PIPE_IO_FLAVOR "standard")
        endif()
    endif()
    if(NOT PIPE_IO_FLAVOR)
        message(FATAL_ERROR "Couldn't determine functions for pipe open/close.")
    endif()

    # Environment variable functions
    check_symbol_exists(_putenv_s "stdlib.h" HAS_UNDERSCORE_PUTENV_S)
    if(HAS_UNDERSCORE_PUTENV_S)
        set(ENV_FLAVOR "putenv_s")
    endif()
    if(NOT ENV_FLAVOR)
        check_symbol_exists(setenv "stdlib.h" HAS_SETENV)
        check_symbol_exists(unsetenv "stdlib.h" HAS_UNSETENV)
        if(HAS_SETENV AND HAS_UNSETENV)
            set(ENV_FLAVOR "setenv")
        endif()
    endif()
    if(NOT ENV_FLAVOR)
        message(FATAL_ERROR "Couldn't determine functions for environment variables.")
    endif()

    # String case-insensitive compare functions
    if(I_STRINGS)
        check_symbol_exists(strcasecmp "strings.h" HAS_STRCASECMP)
        if(HAS_STRCASECMP)
            set(STRING_CASE_COMPARE_FLAVOR "strings")
        endif()
    endif()
    if(NOT STRING_CASE_COMPARE_FLAVOR)
        set(STRING_CASE_COMPARE_FLAVOR "manual")
    endif()

    if(I_PWD)
        check_symbol_exists(getpwent "pwd.h" HAS_GETPWENT)
    endif()
    if(I_UNISTD)
        check_symbol_exists(getdomainname "unistd.h" HAS_GETDOMAINNAME)
        check_symbol_exists(gethostname "unistd.h" HAS_GETHOSTNAME)
        check_symbol_exists(vfork "unistd.h" HAS_VFORK)
        check_symbol_exists(sleep "unistd.h" HAS_SLEEP)
        check_symbol_exists(chown "unistd.h" HAS_CHOWN)
        check_symbol_exists(kill "sys/types.h;signal.h;unistd.h" HAS_KILL)
    endif()
    if(NOT HAS_SLEEP)
        check_symbol_exists(_sleep "stdlib.h" HAS_UNDERSCORE_SLEEP)
    endif()
    if(I_SYS_STAT)
        check_symbol_exists(chmod "sys/stat.h" HAS_CHMOD)
    endif()
    if(NOT HAS_GETHOSTNAME)
        set(CMAKE_REQUIRED_LIBRARIES "ws2_32.lib")
        check_symbol_exists(gethostname "WinSock2.h" HAS_GETHOSTNAME)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif()
    if(NOT HAS_GETHOSTNAME)
        check_symbol_exists(uname "sys/utsname.h" HAS_UNAME)
    endif()
    if(NOT HAS_GETDOMAINNAME)
        check_symbol_exists(res_init "arpa/nameser.h;netinet/in.h;resolv.h" HAS_RES_INIT)
    endif()
    if(I_SYS_WAIT)
        check_symbol_exists(WEXITSTATUS "sys/wait.h" USE_WIFSTAT)
    endif()
    if(WIN32)
        check_cxx_source_compiles([=[
#include <conio.h>
int main()
{
    return _kbhit() + _getch();
}
]=] TRN_HAS_CONIO_TERMINAL_INPUT)
        check_cxx_source_compiles([=[
#include <process.h>
int main()
{
    return static_cast<int>(_spawnl(_P_WAIT, "cmd", "cmd", "/c", "exit 0", nullptr));
}
]=] TRN_HAS_SPAWN_SHELL)
    else()
        set(TRN_HAS_CONIO_TERMINAL_INPUT OFF)
        set(TRN_HAS_SPAWN_SHELL OFF)
    endif()
    if(WIN32)
        find_program(EDITOR_PATH "notepad" NO_CACHE)
    else()
        find_program(EDITOR_PATH "vi" NO_CACHE)
    endif()
    configure_trn_tool_path_default("${EDITOR_PATH}" DEFAULT_EDITOR_PATH "")
    set_config_cache_default(DEFAULT_EDITOR "${DEFAULT_EDITOR_PATH}" STRING
        "The full pathname of the default editor.")
    set(DEFAULT_MAIL_FILE "/usr/spool/mail/%L")
    set(DEFAULT_MAIL_FILE "${DEFAULT_MAIL_FILE}" PARENT_SCOPE)
    set(MAIL_FILE "${DEFAULT_MAIL_FILE}" CACHE STRING "The interpretable name of the mail spool file for the current user.")
    set(MBOX_CHAR "F" CACHE STRING "The character which will match the beginning of a mailbox file.")
    string(LENGTH "${MBOX_CHAR}" MBOX_CHAR_LEN)
    if(NOT (MBOX_CHAR_LEN EQUAL 1))
        message(FATAL_ERROR "Option MBOX_CHAR ('${MBOX_CHAR}') must be set to a single character.")
    endif()
    set(DEFAULT_UNSUBSCRIBED_CHAR "!")
    set(DEFAULT_UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}" PARENT_SCOPE)
    set(UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}" CACHE STRING
        "The character which marks unsubscribed newsgroups in .newsrc.")
    validate_unsubscribed_char("${UNSUBSCRIBED_CHAR}")
    set(DEFAULT_PASSWORD_FILE "/etc/passwd")
    set(DEFAULT_PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}" PARENT_SCOPE)
    set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}" CACHE STRING
        "The password file used for login-directory lookup when getpwent is unavailable.")
    set(DEFAULT_LOGIN_DIR_FIELD "6")
    set(DEFAULT_LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}" PARENT_SCOPE)
    set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}" CACHE STRING
        "The one-based field containing login directories in PASSWORD_FILE.")
    set(DEFAULT_FULLNAMEFILE "%./.fullname")
    set(DEFAULT_FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}" PARENT_SCOPE)
    set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}" CACHE STRING
        "The file used for full-name lookup when PASS_NAMES is disabled.")
    set(DEFAULT_TRNDIR "%./.trn")
    set(DEFAULT_TRNDIR "${DEFAULT_TRNDIR}" PARENT_SCOPE)
    set(TRNDIR "${DEFAULT_TRNDIR}" CACHE STRING "The user's trn initialization directory (~ and % expansion OK).")
    set(DEFAULT_TRNMACRO "%+/macros")
    set(DEFAULT_TRNMACRO "${DEFAULT_TRNMACRO}" PARENT_SCOPE)
    set(TRNMACRO "${DEFAULT_TRNMACRO}" CACHE STRING "The macro file for trn mode (% expansion OK).")
    set(DEFAULT_RNMACRO "%./.rnmac")
    set(DEFAULT_RNMACRO "${DEFAULT_RNMACRO}" PARENT_SCOPE)
    set(RNMACRO "${DEFAULT_RNMACRO}" CACHE STRING "The macro file for rn mode (% expansion OK).")
    set(DEFAULT_RCNAME "%./.newsrc")
    set(DEFAULT_RCNAME "${DEFAULT_RCNAME}" PARENT_SCOPE)
    set(RCNAME "${DEFAULT_RCNAME}" CACHE STRING "The newsrc file path (% and ~ expansion OK).")
    set(DEFAULT_LOCKNAME "%./.rnlock")
    set(DEFAULT_LOCKNAME "${DEFAULT_LOCKNAME}" PARENT_SCOPE)
    set(LOCKNAME "${DEFAULT_LOCKNAME}" CACHE STRING "The newsrc lock file path (% and ~ expansion OK).")
    set(DEFAULT_LASTNAME "%./.rnlast")
    set(DEFAULT_LASTNAME "${DEFAULT_LASTNAME}" PARENT_SCOPE)
    set(LASTNAME "${DEFAULT_LASTNAME}" CACHE STRING "The last-run state file path (% and ~ expansion OK).")
    set(DEFAULT_SIGNATURE_FILE "%./.signature")
    set(DEFAULT_SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}" PARENT_SCOPE)
    set(SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}" CACHE STRING "The signature file path (% and ~ expansion OK).")
    set(DEFAULT_NNTP_AUTH_FILE "%./.nntpauth")
    set(DEFAULT_NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}" PARENT_SCOPE)
    set(NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}" CACHE STRING "The NNTP authentication file path (% and ~ expansion OK).")
    set(INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE STRING
        "The name of the install prefix for this package, defaults to CMAKE_INSTALL_PREFIX.")
    if(WIN32)
        set(DEFAULT_PREF_SHELL "cmd")
    elseif($ENV{SHELL})
        set(DEFAULT_PREF_SHELL "$ENV{SHELL}")
    else()
        set(DEFAULT_PREF_SHELL "bash")
    endif()
    find_program(DEFAULT_PREF_SHELL_PATH "${DEFAULT_PREF_SHELL}")
    if(NOT DEFAULT_PREF_SHELL_PATH)
        message(FATAL_ERROR "Couldn't locate preferred shell '${DEFAULT_PREF_SHELL}'")
    endif()
    set(PREF_SHELL "${DEFAULT_PREF_SHELL_PATH}" CACHE STRING "The full path of the preferred user shell on this system.")
    if(WIN32)
        set(DEFAULT_SH "${DEFAULT_PREF_SHELL_PATH}")
        set(DEFAULT_PIPE_SAVER "type %A | %b")
        set(DEFAULT_SHAR_SAVER "echo SHARSAVER is unsupported on Windows")
        set(DEFAULT_CUSTOM_SAVER "type %A | %e")
    else()
        set(DEFAULT_SH "/bin/sh")
        set(DEFAULT_PIPE_SAVER "%(%B=^0$?<%A:tail +%Bc %A |) %b")
        set(DEFAULT_SHAR_SAVER "tail +%Bc %A | /bin/sh")
        set(DEFAULT_CUSTOM_SAVER "tail +%Bc %A | %e")
    endif()
    set(DEFAULT_SH "${DEFAULT_SH}" PARENT_SCOPE)
    set(DEFAULT_PIPE_SAVER "${DEFAULT_PIPE_SAVER}" PARENT_SCOPE)
    set(DEFAULT_SHAR_SAVER "${DEFAULT_SHAR_SAVER}" PARENT_SCOPE)
    set(DEFAULT_CUSTOM_SAVER "${DEFAULT_CUSTOM_SAVER}" PARENT_SCOPE)
    set(SH "${DEFAULT_SH}" CACHE STRING "The path to the fast shell for command execution.")
    set(PIPE_SAVER "${DEFAULT_PIPE_SAVER}" CACHE STRING "The shell command used for save-to-pipe.")
    set(SHAR_SAVER "${DEFAULT_SHAR_SAVER}" CACHE STRING "The shell command used to extract shell archives.")
    set(CUSTOM_SAVER "${DEFAULT_CUSTOM_SAVER}" CACHE STRING "The shell command used for custom extraction.")
    find_program(DEFAULT_RIPEM_COMMAND "ripem" NO_CACHE)
    configure_trn_verification_command_default(
        "${DEFAULT_RIPEM_COMMAND}" "-d -Y fgs -i %A" DEFAULT_VERIFY_RIPEM)
    find_program(DEFAULT_PGP_COMMAND "pgp" NO_CACHE)
    configure_trn_verification_command_default("${DEFAULT_PGP_COMMAND}" "+batchmode -m %A" DEFAULT_VERIFY_PGP)
    set(DEFAULT_VERIFY_RIPEM "${DEFAULT_VERIFY_RIPEM}" PARENT_SCOPE)
    set(DEFAULT_VERIFY_PGP "${DEFAULT_VERIFY_PGP}" PARENT_SCOPE)
    set_config_cache_default(VERIFY_RIPEM "${DEFAULT_VERIFY_RIPEM}" STRING
        "The shell command used to verify RIPEM signatures; empty disables the default command.")
    set_config_cache_default(VERIFY_PGP "${DEFAULT_VERIFY_PGP}" STRING
        "The shell command used to verify PGP signatures; empty disables the default command.")
    string(CONCAT DEFAULT_MAIL_HEADER
        [=[To: %t\n]=]
        [=[Subject: %(%i=^$?:Re: %S\n]=]
        [=[X-Newsgroups: %n\n]=]
        [=[In-Reply-To: %i)\n]=]
        [=[%(%{FROM}=^$?:From: %{FROM}\n)]=]
        [=[%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)]=]
        [=[%(%[references]=^$?:References: %[references]\n)]=]
        [=[Organization: %o\n]=]
        [=[Cc: \n]=]
        [=[Bcc: \n]=]
        [=[\n]=])
    string(CONCAT DEFAULT_NEWS_HEADER
        [=[%(%[followup-to]=^$?:%(%[followup-to]=^%n$?:X-ORIGINAL-NEWSGROUPS: %n\n))]=]
        [=[Newsgroups: %(%F=^$?%C:%F)\n]=]
        [=[Subject: %(%S=^$?%\"\n\nSubject: \":Re: %S)\n]=]
        [=[Summary: \n]=]
        [=[Expires: \n]=]
        [=[%(%R=^$?:References: %R\n)]=]
        [=[Sender: \n]=]
        [=[Followup-To: \n]=]
        [=[%(%{FROM}=^$?:From: %{FROM}\n)]=]
        [=[%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)]=]
        [=[Distribution: %(%i=^$?%\"Distribution: \":%D)\n]=]
        [=[Organization: %o\n]=]
        [=[Keywords: %[keywords]\n]=]
        [=[Cc: %(%F=poster?%t:%(%F!=@?:%F))\n]=]
        [=[\n]=])
    string(CONCAT DEFAULT_CANCEL_HEADER
        [=[Newsgroups: %n\n]=]
        [=[Subject: cancel\n]=]
        [=[%(%{FROM}=^$?:From: %{FROM}\n)]=]
        [=[Control: cancel %i\n]=]
        [=[Distribution: %D\n]=]
        [=[\n]=]
        [=[%i was cancelled from within trn.\n]=])
    string(CONCAT DEFAULT_SUPERSEDE_HEADER
        [=[Newsgroups: %n\n]=]
        [=[Subject: %[subject]\n]=]
        [=[%(%{FROM}=^$?:From: %{FROM}\n)]=]
        [=[Summary: %[summary]\n]=]
        [=[Expires: %[expires]\n]=]
        [=[References: %[references]\n]=]
        [=[From: %[from]\n]=]
        [=[Reply-To: %[reply-to]\n]=]
        [=[Supersedes: %i\n]=]
        [=[Sender: %[sender]\n]=]
        [=[Followup-To: %[followup-to]\n]=]
        [=[Distribution: %D\n]=]
        [=[Organization: %o\n]=]
        [=[Keywords: %[keywords]\n]=]
        [=[\n]=])
    set(DEFAULT_MAIL_HEADER "${DEFAULT_MAIL_HEADER}" PARENT_SCOPE)
    set(DEFAULT_NEWS_HEADER "${DEFAULT_NEWS_HEADER}" PARENT_SCOPE)
    set(DEFAULT_CANCEL_HEADER "${DEFAULT_CANCEL_HEADER}" PARENT_SCOPE)
    set(DEFAULT_SUPERSEDE_HEADER "${DEFAULT_SUPERSEDE_HEADER}" PARENT_SCOPE)
    set(MAIL_HEADER "${DEFAULT_MAIL_HEADER}" CACHE STRING "The header template for mail replies.")
    set(NEWS_HEADER "${DEFAULT_NEWS_HEADER}" CACHE STRING "The header template for followup articles.")
    set(CANCEL_HEADER "${DEFAULT_CANCEL_HEADER}" CACHE STRING "The header template for article cancellations.")
    set(SUPERSEDE_HEADER "${DEFAULT_SUPERSEDE_HEADER}" CACHE STRING "The header template for superseding articles.")
    set(ROOT_UID "0" CACHE STRING "On POSIX systems, the uid of root, normally 0.")
    probe_news_source_defaults()
    set(DEFAULT_SERVER_NAME "${DEFAULT_SERVER_NAME}" PARENT_SCOPE)
    set_config_cache_default(SERVER_NAME "${DEFAULT_SERVER_NAME}" STRING
        "The default server name or a file to open to read the server name." "news.gmane.io")
    set(DEFAULT_HAS_LOCAL_SPOOL "${DEFAULT_HAS_LOCAL_SPOOL}" PARENT_SCOPE)
    set_config_cache_default(HAS_LOCAL_SPOOL "${DEFAULT_HAS_LOCAL_SPOOL}" BOOL
        "Indicates that there's a local spool directory configured into trn." OFF)
    configure_news_source_defaults()
    set(DEFAULT_ACTIVE "${DEFAULT_ACTIVE}" PARENT_SCOPE)
    set_config_cache_default(ACTIVE "${DEFAULT_ACTIVE}" STRING
        "The name of the active file for the news system. (~ interpolation OK)"
        "%x/active" "remote")
    set(ACTIVE_TIMES "none" CACHE STRING "The name of the active.times file for the news system.")
    set(PIPE_HOST_CMD "" CACHE STRING "A command to feed to the popen() routine to derive the host name.")
    probe_termlib_default()
    set(DEFAULT_HAS_TERMLIB "${DEFAULT_HAS_TERMLIB}" PARENT_SCOPE)
    configure_termlib_cache_default()
    option(IGNORE_ORG "Indicates that the ORGANIZATION environment variable does not contain an organization name." OFF)
    set(MIMECAP "/etc/mailcap" CACHE STRING
        "The pathname of the mimecap file, which controls what programs get run when handling mime articles.")
    if(WIN32)
        configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/path_list.semicolon.cpp" path_list.cpp COPYONLY)
        set(DEFAULT_MIMECAP "%./.mimecap;~/.mailcap;${MIMECAP}")
        string(REPLACE "\\" "/" _ProgramData "$ENV{ProgramData}")
        set(DEFAULT_NEWS_LIB "${_ProgramData}/trn/news")
    else()
        configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/path_list.colon.cpp" path_list.cpp COPYONLY)
        set(DEFAULT_MIMECAP "%./.mimecap:~/.mailcap:${MIMECAP}")
        set(DEFAULT_NEWS_LIB "/usr/lib/news")
    endif()
    set(DEFAULT_MIMECAP "${DEFAULT_MIMECAP}" PARENT_SCOPE)
    set(DEFAULT_NEWS_LIB "${DEFAULT_NEWS_LIB}" PARENT_SCOPE)
    set(NEWS_LIB "${DEFAULT_NEWS_LIB}" CACHE FILEPATH "The path of the directory serving as the news library (~ expansion OK).")
    set(EXTRA_INEWS "" CACHE FILEPATH "The full path of an inews to use for local postings, or nothing.")
    set(DEFAULT_GROUP_DESC "${DEFAULT_GROUP_DESC}" PARENT_SCOPE)
    set_config_cache_default(GROUP_DESC "${DEFAULT_GROUP_DESC}" FILEPATH
        "The full path of the \"newsgroups\" file for group descriptions, or nothing."
        "%x/newsgroups" "none")
    set(DEFAULT_SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}" PARENT_SCOPE)
    set_config_cache_default(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}" FILEPATH
        "The full path of the list of default subscriptions, or nothing (~ expansion OK)."
        "%x/subscriptions")
    set(DEFAULT_NEWS_SPOOL "${DEFAULT_NEWS_SPOOL}" PARENT_SCOPE)
    set_config_cache_default(NEWS_SPOOL "${DEFAULT_NEWS_SPOOL}" STRING
        "The directory name where news articles are spooled (~, % expansion OK.)."
        "%X/spool" "/usr/spool/news" "none")
    set(DEFAULT_SELECTION_CHARS "abdefgijlorstuvwxyz1234567890BCFGIKVW")
    set(DEFAULT_SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}" PARENT_SCOPE)
    set(SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}" CACHE STRING
        "The default characters to use in the selection menu.")
    set(DEFAULT_LOCALTIME_FMT "%a %b %d %X %Z %Y")
    set(DEFAULT_LOCALTIME_FMT "${DEFAULT_LOCALTIME_FMT}" PARENT_SCOPE)
    set(LOCALTIME_FMT "${DEFAULT_LOCALTIME_FMT}" CACHE STRING
        "The strftime format used for local-time Date headers.")
    set(DEFAULT_MAIL_CALL ON)
    set(DEFAULT_MAIL_CALL "${DEFAULT_MAIL_CALL}" PARENT_SCOPE)
    option(MAIL_CALL "If ON, check periodically for mail." ${DEFAULT_MAIL_CALL})
    set(DEFAULT_NO_FIREWORKS ON)
    set(DEFAULT_NO_FIREWORKS "${DEFAULT_NO_FIREWORKS}" PARENT_SCOPE)
    option(NO_FIREWORKS "If ON, avoid whole-screen flashes on some terminals." ${DEFAULT_NO_FIREWORKS})
    set(DEFAULT_TILDE_NAME ON)
    set(DEFAULT_TILDE_NAME "${DEFAULT_TILDE_NAME}" PARENT_SCOPE)
    option(TILDE_NAME "If ON, allow tilde-login expansion." ${DEFAULT_TILDE_NAME})
    set(DEFAULT_VALIDATE_XREF_SITE OFF)
    set(DEFAULT_VALIDATE_XREF_SITE "${DEFAULT_VALIDATE_XREF_SITE}" PARENT_SCOPE)
    option(VALIDATE_XREF_SITE "If ON, validate xref sites before processing xrefs." ${DEFAULT_VALIDATE_XREF_SITE})
    set(DEFAULT_USE_FTP ON)
    set(DEFAULT_USE_FTP "${DEFAULT_USE_FTP}" PARENT_SCOPE)
    option(USE_FTP "If ON, use ftpgrab for ftp URLs." ${DEFAULT_USE_FTP})
    set(ORG_NAME "%X/organization" CACHE STRING
        "Either the organization name or the full pathname of a file containing the organization name.")
    set(POSTING_HOSTNAME "%X/posting-hostname" CACHE STRING
        "This symbol contains the posting host's name or a file from which to read its name.")
    set(DEFAULT_POSTING_HOST_MATCH_SEGMENTS "2")
    set(DEFAULT_POSTING_HOST_MATCH_SEGMENTS "${DEFAULT_POSTING_HOST_MATCH_SEGMENTS}" PARENT_SCOPE)
    set(POSTING_HOST_MATCH_SEGMENTS "${DEFAULT_POSTING_HOST_MATCH_SEGMENTS}" CACHE STRING
        "The count of dot-separated hostname words to compare for local articles; 0 means all.")
    validate_posting_host_match_segments("${POSTING_HOST_MATCH_SEGMENTS}")
    option(TRN_MARK_XREFS_UNREAD
        "If ON, unmark xrefed articles in other newsgroups when articles are unmarked." ON)
    set(MCHASE ${TRN_MARK_XREFS_UNREAD})
    set(PRIVATE_LIB "%x/trn" CACHE STRING "The path to private executables; ~, %x and %l expansion allowed.")
    set(DEFAULT_GLOBAL_INIT "%X/INIT")
    set(DEFAULT_GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}" PARENT_SCOPE)
    set(GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}" CACHE STRING "The system-wide RNINIT switches file path.")
    set(DEFAULT_DEFACCESS "%X/access.def")
    set(DEFAULT_DEFACCESS "${DEFAULT_DEFACCESS}" PARENT_SCOPE)
    set(DEFACCESS "${DEFAULT_DEFACCESS}" CACHE STRING "The default trn access list path.")
    set(DEFAULT_TRNACCESS "%+/access")
    set(DEFAULT_TRNACCESS "${DEFAULT_TRNACCESS}" PARENT_SCOPE)
    set(TRNACCESS "${DEFAULT_TRNACCESS}" CACHE STRING "The trn access list path.")
    set(DEFAULT_NEWSNEWSNAME "%X/newsnews")
    set(DEFAULT_NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}" PARENT_SCOPE)
    set(NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}" CACHE STRING "The motd-like file path for trn.")
    set(DEFAULT_OV_FILE_NAME "/.overview")
    set(DEFAULT_OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}" PARENT_SCOPE)
    set(OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}" CACHE STRING "The overview file name appended to a group directory.")
    set(DEFAULT_VARYNAME "%Y/rnvary.%$")
    set(DEFAULT_VARYNAME "${DEFAULT_VARYNAME}" PARENT_SCOPE)
    set(VARYNAME "${DEFAULT_VARYNAME}" CACHE STRING "The virtual array file name template.")
    set(DEFAULT_HEADNAME "%./.rnhead.%$")
    set(DEFAULT_HEADNAME "${DEFAULT_HEADNAME}" PARENT_SCOPE)
    set(HEADNAME "${DEFAULT_HEADNAME}" CACHE STRING "The header file name template for followup posting.")
    option(TRN_MAKE_SAVE_DIRECTORIES
        "If ON, default saves use per-newsgroup directories and article-number filenames." OFF)
    configure_save_path_defaults("${TRN_MAKE_SAVE_DIRECTORIES}")
    set(DEFAULT_SAVEDIR "${DEFAULT_SAVEDIR}" PARENT_SCOPE)
    set(DEFAULT_SAVENAME "${DEFAULT_SAVENAME}" PARENT_SCOPE)
    set(SAVEDIR "${DEFAULT_SAVEDIR}" CACHE STRING "The directory template for saving articles.")
    set(SAVENAME "${DEFAULT_SAVENAME}" CACHE STRING "The file name template for saving articles.")
    set(DEFAULT_KILL_GLOBAL "%p/KILL")
    set(DEFAULT_KILL_LOCAL "%p/%c/KILL")
    set(DEFAULT_KILL_THREADS "%+/Kill/Threads")
    set(DEFAULT_KILL_GLOBAL "${DEFAULT_KILL_GLOBAL}" PARENT_SCOPE)
    set(DEFAULT_KILL_LOCAL "${DEFAULT_KILL_LOCAL}" PARENT_SCOPE)
    set(DEFAULT_KILL_THREADS "${DEFAULT_KILL_THREADS}" PARENT_SCOPE)
    set(KILL_GLOBAL "${DEFAULT_KILL_GLOBAL}" CACHE STRING "The global kill-file path template.")
    set(KILL_LOCAL "${DEFAULT_KILL_LOCAL}" CACHE STRING "The local kill-file path template.")
    set(KILL_THREADS "${DEFAULT_KILL_THREADS}" CACHE STRING "The thread kill-file path template.")
    configure_news_overview_dir_default()
    set(DEFAULT_OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}" PARENT_SCOPE)
    set_config_cache_default(OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}" STRING
        "The directory containing overview files."
        "remote" "/usr/spool/news")
    configure_news_overview_format_default()
    set(DEFAULT_OVERVIEW_FMT "${DEFAULT_OVERVIEW_FMT}" PARENT_SCOPE)
    set_config_cache_default(OVERVIEW_FMT "${DEFAULT_OVERVIEW_FMT}" STRING
        "The name of the overview.fmt file."
        "none" "%x/overview.fmt")
    option(THREAD_INIT "If ON, indicates we act like trn no matter what our name is." OFF)
    option(SELECT_INIT "If ON, indicates we default to the selector for group entry." OFF)
    if(NOT CMAKE_INSTALL_BINDIR)
        set(CMAKE_INSTALL_BINDIR "bin")
    endif()
    set_config_cache_default(BIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}" STRING
        "Path of the bin directory where the package will be installed."
        "${CMAKE_INSTALL_PREFIX}/bin")
    set_config_cache_default(USE_INEWS "${BIN_DIR}/inews" STRING
        "Full path to the inews to use for posting." "%x/inews")
    set(DEFAULT_CALL_INEWS "inews -h <%h")
    set(DEFAULT_CALL_INEWS "${DEFAULT_CALL_INEWS}" PARENT_SCOPE)
    set(CALL_INEWS "${DEFAULT_CALL_INEWS}" CACHE STRING "The command used to cancel articles through inews.")
    set(DEFAULT_NEWS_POSTER "Pnews -h %h")
    set(DEFAULT_NEWS_POSTER "${DEFAULT_NEWS_POSTER}" PARENT_SCOPE)
    set(NEWS_POSTER "${DEFAULT_NEWS_POSTER}" CACHE STRING "The command used to edit and submit followup articles.")
    set(DEFAULT_MAIL_POSTER "Rnmail -h %h")
    set(DEFAULT_MAIL_POSTER "${DEFAULT_MAIL_POSTER}" PARENT_SCOPE)
    set(MAIL_POSTER "${DEFAULT_MAIL_POSTER}" CACHE STRING "The command used to edit and deliver mail replies.")
    set(DEFAULT_YOU_SAID "In article %i you write:")
    set(DEFAULT_ATTRIBUTION "In article %i,%?%)f <%>f> wrote:")
    set(DEFAULT_FORWARD_POSTER "${BIN_DIR}/Rnmail -h %h")
    string(CONCAT DEFAULT_FORWARD_HEADER
        [=[To: %\"\n]=]
        [=[\n]=]
        [=[To: \"\n]=]
        [=[Subject: %(%i=^$?:%[subject] (fwd\\)\n]=]
        [=[%(%{FROM}=^$?:From: %{FROM}\n)]=]
        [=[%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\n)]=]
        [=[X-Newsgroups: %n\n]=]
        [=[In-Reply-To: %i)\n]=]
        [=[%(%[references]=^$?:References: %[references]\n)]=]
        [=[Organization: %o\n]=]
        [=[Mime-Version: 1.0\n]=]
        [=[Content-Type: multipart/mixed; boundary=\"=%$%^#=--\"\n]=]
        [=[Cc: \n]=]
        [=[Bcc: \n]=]
        [=[\n]=])
    set(DEFAULT_FORWARD_MSG "------- start of forwarded message -------")
    set(DEFAULT_FORWARD_MSG_END "------- end of forwarded message -------")
    set(DEFAULT_YOU_SAID "${DEFAULT_YOU_SAID}" PARENT_SCOPE)
    set(DEFAULT_ATTRIBUTION "${DEFAULT_ATTRIBUTION}" PARENT_SCOPE)
    set(DEFAULT_FORWARD_POSTER "${DEFAULT_FORWARD_POSTER}" PARENT_SCOPE)
    set(DEFAULT_FORWARD_HEADER "${DEFAULT_FORWARD_HEADER}" PARENT_SCOPE)
    set(DEFAULT_FORWARD_MSG "${DEFAULT_FORWARD_MSG}" PARENT_SCOPE)
    set(DEFAULT_FORWARD_MSG_END "${DEFAULT_FORWARD_MSG_END}" PARENT_SCOPE)
    set(YOU_SAID "${DEFAULT_YOU_SAID}" CACHE STRING "The reply-by-mail quoted-article introduction line.")
    set(ATTRIBUTION "${DEFAULT_ATTRIBUTION}" CACHE STRING "The followup quoted-article attribution line.")
    set(FORWARD_POSTER "${DEFAULT_FORWARD_POSTER}" CACHE STRING "The command used to edit and deliver forwarded articles.")
    set(FORWARD_HEADER "${DEFAULT_FORWARD_HEADER}" CACHE STRING "The header template for forwarded articles.")
    set(FORWARD_MSG "${DEFAULT_FORWARD_MSG}" CACHE STRING "The line marking the start of a forwarded article.")
    set(FORWARD_MSG_END "${DEFAULT_FORWARD_MSG_END}" CACHE STRING "The line marking the end of a forwarded article.")
    find_program(DEFAULT_PAGER_PATH "more" NO_CACHE)
    configure_trn_tool_path_default("${DEFAULT_PAGER_PATH}" DEFAULT_PAGER "")
    set(DEFAULT_PAGER "${DEFAULT_PAGER}" PARENT_SCOPE)
    set_config_cache_default(PAGER "${DEFAULT_PAGER}" STRING
        "The path of the preferred pager on the system; empty disables the default command.")
    set(DEFAULT_REPLYTO_POSTER_CHECKING OFF)
    set(DEFAULT_REPLYTO_POSTER_CHECKING "${DEFAULT_REPLYTO_POSTER_CHECKING}" PARENT_SCOPE)
    option(REPLYTO_POSTER_CHECKING
        "If ON, makes -p also check Reply-To (slow)" ${DEFAULT_REPLYTO_POSTER_CHECKING})
    probe_name_type_default("${HAS_GETPWENT}")
    configure_name_parsing_default("${HAS_GETPWENT}" "${PROBED_NAME_TYPE}")
    set(DEFAULT_NAME_TYPE "${DEFAULT_NAME_TYPE}" PARENT_SCOPE)
    set_config_cache_default(NAME_TYPE "${DEFAULT_NAME_TYPE}" STRING
        "The way full names are stored in the password file (bsd, usg or other).")
    validate_name_parsing_support("${NAME_TYPE}" "${HAS_GETPWENT}")
    set(DEFAULT_LOCAL_DIST "none")
    set(DEFAULT_ORGANIZATION_DIST "none")
    set(DEFAULT_CITY_DIST "none")
    set(DEFAULT_STATE_DIST "none")
    set(DEFAULT_MULTISTATE_DIST "none")
    set(DEFAULT_COUNTRY_DIST "none")
    set(DEFAULT_CONTINENT_DIST "none")
    set(DEFAULT_LOCAL_DIST "${DEFAULT_LOCAL_DIST}" PARENT_SCOPE)
    set(DEFAULT_ORGANIZATION_DIST "${DEFAULT_ORGANIZATION_DIST}" PARENT_SCOPE)
    set(DEFAULT_CITY_DIST "${DEFAULT_CITY_DIST}" PARENT_SCOPE)
    set(DEFAULT_STATE_DIST "${DEFAULT_STATE_DIST}" PARENT_SCOPE)
    set(DEFAULT_MULTISTATE_DIST "${DEFAULT_MULTISTATE_DIST}" PARENT_SCOPE)
    set(DEFAULT_COUNTRY_DIST "${DEFAULT_COUNTRY_DIST}" PARENT_SCOPE)
    set(DEFAULT_CONTINENT_DIST "${DEFAULT_CONTINENT_DIST}" PARENT_SCOPE)
    set(LOCAL_DIST "${DEFAULT_LOCAL_DIST}" CACHE STRING "Distribution code for your local organization or none.")
    set(ORGANIZATION_DIST "${DEFAULT_ORGANIZATION_DIST}" CACHE STRING "Distribution code for your organization or none.")
    set(CITY_DIST "${DEFAULT_CITY_DIST}" CACHE STRING "Distribution code for your city or none.")
    set(STATE_DIST "${DEFAULT_STATE_DIST}" CACHE STRING "Distribution code for your state/province or none.")
    set(MULTISTATE_DIST "${DEFAULT_MULTISTATE_DIST}" CACHE STRING "Distribution code for your multi-state region or none.")
    set(COUNTRY_DIST "${DEFAULT_COUNTRY_DIST}" CACHE STRING "Distribution code for your country or none.")
    set(CONTINENT_DIST "${DEFAULT_CONTINENT_DIST}" CACHE STRING "Distribution code for your continent or none.")
    validate_distribution_code("LOCAL_DIST" "${LOCAL_DIST}")
    validate_distribution_code("ORGANIZATION_DIST" "${ORGANIZATION_DIST}")
    validate_distribution_code("CITY_DIST" "${CITY_DIST}")
    validate_distribution_code("STATE_DIST" "${STATE_DIST}")
    validate_distribution_code("MULTISTATE_DIST" "${MULTISTATE_DIST}")
    validate_distribution_code("COUNTRY_DIST" "${COUNTRY_DIST}")
    validate_distribution_code("CONTINENT_DIST" "${CONTINENT_DIST}")
    if(IGNORE_ORG)
        set(POSTING_ORG_NAME "\${NEWSORG-${ORG_NAME}}" PARENT_SCOPE)
    else()
        set(POSTING_ORG_NAME "\${NEWSORG-\${ORGANIZATION-${ORG_NAME}}}" PARENT_SCOPE)
    endif()

    set(USE_BUILTIN_TERMINAL_CAPABILITIES OFF)
    set(USE_CONIO_TERMINAL_INPUT OFF)
    set(USE_SPAWN_SHELL OFF)
    set(USE_UNDERSCORE_SLEEP OFF)
    set(USE_WINDOWS_ENVIRONMENT_NAMES OFF)
    set(USE_WINDOWS_FILENAME_RULES OFF)
    set(CHECK_NEWSRC_LOCK_PROCESS OFF)
    set(SET_ARTICLE_TEMP_FILE_PERMISSIONS OFF)
    set(PRESERVE_NEWSRC_FILE_PERMISSIONS OFF)
    if(WIN32)
        set(USE_BUILTIN_TERMINAL_CAPABILITIES ON)
        set(USE_WINDOWS_ENVIRONMENT_NAMES ON)
        set(USE_WINDOWS_FILENAME_RULES ON)
    endif()
    if(TRN_HAS_CONIO_TERMINAL_INPUT)
        set(USE_CONIO_TERMINAL_INPUT ON)
    endif()
    if(TRN_HAS_SPAWN_SHELL)
        set(USE_SPAWN_SHELL ON)
    endif()
    if(HAS_UNDERSCORE_SLEEP)
        set(USE_UNDERSCORE_SLEEP ON)
    endif()
    if(HAS_KILL)
        set(CHECK_NEWSRC_LOCK_PROCESS ON)
    endif()
    if(HAS_CHMOD)
        set(SET_ARTICLE_TEMP_FILE_PERMISSIONS ON)
    endif()
    if(HAS_CHMOD AND HAS_CHOWN)
        set(PRESERVE_NEWSRC_FILE_PERMISSIONS ON)
    endif()
    set(USE_BUILTIN_TERMINAL_CAPABILITIES "${USE_BUILTIN_TERMINAL_CAPABILITIES}" PARENT_SCOPE)
    set(USE_CONIO_TERMINAL_INPUT "${USE_CONIO_TERMINAL_INPUT}" PARENT_SCOPE)
    set(USE_SPAWN_SHELL "${USE_SPAWN_SHELL}" PARENT_SCOPE)
    set(USE_UNDERSCORE_SLEEP "${USE_UNDERSCORE_SLEEP}" PARENT_SCOPE)
    set(USE_WINDOWS_ENVIRONMENT_NAMES "${USE_WINDOWS_ENVIRONMENT_NAMES}" PARENT_SCOPE)
    set(USE_WINDOWS_FILENAME_RULES "${USE_WINDOWS_FILENAME_RULES}" PARENT_SCOPE)
    set(CHECK_NEWSRC_LOCK_PROCESS "${CHECK_NEWSRC_LOCK_PROCESS}" PARENT_SCOPE)
    set(SET_ARTICLE_TEMP_FILE_PERMISSIONS "${SET_ARTICLE_TEMP_FILE_PERMISSIONS}" PARENT_SCOPE)
    set(PRESERVE_NEWSRC_FILE_PERMISSIONS "${PRESERVE_NEWSRC_FILE_PERMISSIONS}" PARENT_SCOPE)
    if(NOT WIN32)
        set(HAS_NEWS_ADMIN ON)
        set(NEWS_ADMIN "root")
    endif()
    set(NORM_SIG ON)
    # TODO: Does any modern platform return non-void from a signal handler?
    # This seems like a pre-POSIX coping strategy.
    set(SIGNAL_T "void")
    option(DEBUG "Compile with debug logging enabled." ON)

    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/config.h.in" include/config/config.h)
    set(STRING_CASE_COMPARE_SOURCES_strings "")
    set(STRING_CASE_COMPARE_SOURCES_manual "string_case_compare.cpp")
    set(STRING_CASE_COMPARE_SOURCES ${STRING_CASE_COMPARE_SOURCES_${STRING_CASE_COMPARE_FLAVOR}} PARENT_SCOPE)
    configure_flavor_header("env" "${ENV_FLAVOR}")
    configure_flavor_header("fdio" "${FDIO_FLAVOR}")
    configure_flavor_header("pipe_io" "${PIPE_IO_FLAVOR}")
    configure_flavor_header("string_case_compare" "${STRING_CASE_COMPARE_FLAVOR}")
endfunction()
