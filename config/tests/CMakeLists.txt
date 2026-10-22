include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_env.cpp
    test_file_ref.cpp
    test_path_list.cpp
    test_string_case_compare.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
target_folder(config-test "Tests")
gtest_discover_tests(config-test PROPERTIES LABELS trn)

add_executable(pnews-test-tool pnews_test_tool.cpp)
target_compile_features(pnews-test-tool PRIVATE cxx_std_17)
target_folder(pnews-test-tool "Tests")
if(WIN32)
    add_test(NAME MailSubmissionTest.headers
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_TEST_MAIL_ADAPTER=${PROJECT_BINARY_DIR}/config/data/trn-mail.ps1"
            "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/mail-headers"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_mail_headers.cmake")
    set_tests_properties(MailSubmissionTest.headers PROPERTIES LABELS trn TIMEOUT 30)
    add_test(NAME MailSubmissionTest.nativeSmtp
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
            -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/test_mail_smtp.ps1"
            -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/mail-smtp"
            -Adapter "${PROJECT_BINARY_DIR}/config/data/trn-mail.ps1")
    set_tests_properties(MailSubmissionTest.nativeSmtp PROPERTIES LABELS trn TIMEOUT 90)
endif()
set(rnmail_cases header arguments interactive inclusion loop abort eof signature record saver pgp forward)
if(WIN32)
    list(APPEND rnmail_cases editor_failure editor_cancel mailer_failure mailer_unconfirmed mailer_handoff saver_failure missing_account)
endif()
foreach(case IN LISTS rnmail_cases)
    add_test(NAME RnmailTest.${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_TEST_RNMAIL=${TRN_GENERATED_RNMAIL_FILE}"
            "-DTRN_TEST_PNEWS_TOOL=$<TARGET_FILE:pnews-test-tool>"
            "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/rnmail-${case}"
            "-DTRN_TEST_CASE=${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_rnmail.cmake")
    set_tests_properties(RnmailTest.${case} PROPERTIES LABELS trn TIMEOUT 30)
endforeach()
set(pnews_cases header arguments interactive distribution inclusion loop abort eof eight_bit authorcopy saver pgp fetch)
if(WIN32)
    list(APPEND pnews_cases editor_failure editor_cancel artcheck_failure poster_failure nntplist_failure spell_failure pager_failure pgp_failure saver_failure missing_tool email_copy blind_copy moderated moderated_mime poster_reply email_failure moderated_failure)
endif()
foreach(case IN LISTS pnews_cases)
    add_test(NAME PnewsTest.${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_TEST_PNEWS=${TRN_GENERATED_PNEWS_FILE}"
            "-DTRN_TEST_PNEWS_TOOL=$<TARGET_FILE:pnews-test-tool>"
            "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/pnews-${case}"
            "-DTRN_TEST_CASE=${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_pnews.cmake")
    set_tests_properties(PnewsTest.${case} PROPERTIES LABELS trn TIMEOUT 30)
endforeach()

if(WIN32 AND POSTING_HOSTNAME MATCHES "^[A-Za-z0-9.-]+\\.[A-Za-z0-9.-]+$")
    add_test(NAME PnewsTest.nativeInews
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
            -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/test_pnews_nntp.ps1"
            -Helper "${TRN_GENERATED_PNEWS_FILE}"
            -Tool "$<TARGET_FILE:pnews-test-tool>"
            -Inews "$<TARGET_FILE:inews>" -Artcheck "$<TARGET_FILE:trn-artchk>"
            -Cmake "${CMAKE_COMMAND}" -Fixture "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_pnews.cmake"
            -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/pnews-native-inews")
    set_tests_properties(PnewsTest.nativeInews PROPERTIES LABELS trn TIMEOUT 60)
endif()

foreach(case IN ITEMS full body first_byte past_end empty append spaces)
    add_test(NAME NormSaverTest.${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_TEST_NORM_SAVER=${TRN_GENERATED_NORM_SAVER_FILE}"
            "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/norm-saver-${case}"
            "-DTRN_TEST_CASE=${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_norm_saver.cmake")
    set_tests_properties(NormSaverTest.${case} PROPERTIES LABELS trn)
endforeach()
if(WIN32)
    foreach(case IN ITEMS invalid_offset arity missing_input missing_parent same_file)
        add_test(NAME NormSaverTest.${case}
            COMMAND "${CMAKE_COMMAND}"
                "-DTRN_TEST_SAVER=${TRN_GENERATED_NORM_SAVER_FILE}"
                "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/norm-saver-${case}"
                "-DTRN_TEST_CASE=${case}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_saver_errors.cmake")
        set_tests_properties(NormSaverTest.${case} PROPERTIES LABELS trn)
    endforeach()
    add_test(NAME NormSaverTest.locked_output
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
            -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/test_saver_locked.ps1"
            -Helper "${TRN_GENERATED_NORM_SAVER_FILE}"
            -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/norm-saver-locked_output")
    set_tests_properties(NormSaverTest.locked_output PROPERTIES LABELS trn)
endif()

set(test_saved_mbox_char "${MBOX_CHAR}")
string(ASCII 1 CTRL_A)
set(TRN_TEST_MBOX_FORMAT mbox)
if(MBOX_CHAR STREQUAL CTRL_A)
    set(TRN_TEST_MBOX_FORMAT mmdf)
endif()
foreach(format IN ITEMS mbox mmdf)
    set(MBOX_CHAR "F")
    set(TRN_MBOX_SAVER_MMDF 0)
    if(format STREQUAL "mmdf")
        set(MBOX_CHAR "${CTRL_A}")
        set(TRN_MBOX_SAVER_MMDF 1)
    endif()
    set(test_helper_dir "${CMAKE_CURRENT_BINARY_DIR}/mbox-${format}")
    if(WIN32)
        set(test_mbox_helper "${test_helper_dir}/mbox.saver.cmd")
        configure_file(../data/mbox.saver.cmd.in "${test_mbox_helper}" COPYONLY)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../data/mbox.saver.ps1.in")
            configure_file(../data/mbox.saver.ps1.in "${test_helper_dir}/mbox.saver.ps1" @ONLY)
        endif()
    else()
        set(test_mbox_helper "${test_helper_dir}/mbox.saver")
        configure_file(../data/mbox.saver.in "${test_mbox_helper}" @ONLY)
    endif()
    set(mbox_cases full body first_byte past_end empty append record)
    if(WIN32)
        list(APPEND mbox_cases spaces unicode_envelope)
    endif()
    foreach(case IN LISTS mbox_cases)
        add_test(NAME MboxSaverTest.${format}_${case}
            COMMAND "${CMAKE_COMMAND}"
                "-DTRN_TEST_MBOX_SAVER=${test_mbox_helper}"
                "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/mbox-${format}-${case}"
                "-DTRN_TEST_CASE=${case}"
                "-DTRN_TEST_MBOX_FORMAT=${format}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_mbox_saver.cmake")
        set_tests_properties(MboxSaverTest.${format}_${case} PROPERTIES LABELS trn)
    endforeach()
    if(WIN32)
        foreach(case IN ITEMS invalid_offset arity missing_input missing_parent same_file)
            add_test(NAME MboxSaverTest.${format}_${case}
                COMMAND "${CMAKE_COMMAND}"
                    "-DTRN_TEST_SAVER=${test_mbox_helper}"
                    "-DTRN_TEST_HAS_ENVELOPE=ON"
                    "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/mbox-${format}-${case}"
                    "-DTRN_TEST_CASE=${case}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_saver_errors.cmake")
            set_tests_properties(MboxSaverTest.${format}_${case} PROPERTIES LABELS trn)
        endforeach()
        add_test(NAME MboxSaverTest.${format}_locked_output
            COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
                -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/test_saver_locked.ps1"
                -Helper "${test_mbox_helper}" -HasEnvelope
                -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/mbox-${format}-locked_output")
        set_tests_properties(MboxSaverTest.${format}_locked_output PROPERTIES LABELS trn)
    endif()
endforeach()
set(MBOX_CHAR "${test_saved_mbox_char}")

set(speller_cases correct multiple words clean failure cancel missing_tool pager_failure word_failure quote_pattern no_quotes empty)
if(WIN32)
    list(APPEND speller_cases insert delete header_order arity missing_input invalid_pattern invalid_options)
    add_test(NAME SpellerTest.locked
        COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
            -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/test_speller.ps1"
            -Helper "${TRN_GENERATED_SPELLER_FILE}"
            -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/speller-locked")
    set_tests_properties(SpellerTest.locked PROPERTIES LABELS trn)
endif()
foreach(case IN LISTS speller_cases)
    add_test(NAME SpellerTest.${case}
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_TEST_SPELLER=${TRN_GENERATED_SPELLER_FILE}"
            "-DTRN_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/speller-${case}"
            "-DTRN_TEST_CASE=${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_speller.cmake")
    set_tests_properties(SpellerTest.${case} PROPERTIES LABELS trn)
endforeach()

set(posting_host_match_segments_validation_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_posting_host_match_segments_validation.cmake")
set(posting_host_match_segments_configure_script
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/configure_trn.cmake")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsAcceptsZero
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=0"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsAcceptsLargeCount
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=15"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsRejectsText
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=bogus"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsRejectsNegative
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=-1"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${posting_host_match_segments_validation_script}")
set_tests_properties(
    ConfigCMakeTest.postingHostMatchSegmentsAcceptsZero
    ConfigCMakeTest.postingHostMatchSegmentsAcceptsLargeCount
    ConfigCMakeTest.postingHostMatchSegmentsRejectsText
    ConfigCMakeTest.postingHostMatchSegmentsRejectsNegative
    PROPERTIES LABELS trn)

set(unsubscribed_char_validation_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_unsubscribed_char_validation.cmake")
add_test(NAME ConfigCMakeTest.unsubscribedCharAcceptsSingleCharacter
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR=?"
        -P "${unsubscribed_char_validation_script}")
add_test(NAME ConfigCMakeTest.unsubscribedCharRejectsEmpty
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR="
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${unsubscribed_char_validation_script}")
add_test(NAME ConfigCMakeTest.unsubscribedCharRejectsMultipleCharacters
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR=!!"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${unsubscribed_char_validation_script}")
set_tests_properties(
    ConfigCMakeTest.unsubscribedCharAcceptsSingleCharacter
    ConfigCMakeTest.unsubscribedCharRejectsEmpty
    ConfigCMakeTest.unsubscribedCharRejectsMultipleCharacters
    PROPERTIES LABELS trn)

set(pnews_distribution_validation_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_pnews_distribution_validation.cmake")
set(name_parsing_policy_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_name_parsing_policy.cmake")
set(name_parsing_default_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_name_parsing_default.cmake")
add_test(NAME ConfigCMakeTest.nameTypeAcceptsBsd
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=bsd"
        -P "${pnews_distribution_validation_script}")
add_test(NAME ConfigCMakeTest.nameTypeAcceptsUsg
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=usg"
        -P "${pnews_distribution_validation_script}")
add_test(NAME ConfigCMakeTest.nameTypeAcceptsOther
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=other"
        -P "${pnews_distribution_validation_script}")
add_test(NAME ConfigCMakeTest.nameTypeRejectsUnknown
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=unknown"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${pnews_distribution_validation_script}")
add_test(NAME ConfigCMakeTest.nameTypeBsdDerivesPassNames
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=bsd"
        "-DTRN_TEST_EXPECT_PASS_NAMES=ON"
        "-DTRN_TEST_EXPECT_BERKELEY_NAMES=ON"
        -P "${name_parsing_policy_script}")
add_test(NAME ConfigCMakeTest.nameTypeUsgDerivesPassNames
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=usg"
        "-DTRN_TEST_EXPECT_PASS_NAMES=ON"
        "-DTRN_TEST_EXPECT_BERKELEY_NAMES=OFF"
        -P "${name_parsing_policy_script}")
add_test(NAME ConfigCMakeTest.nameTypeOtherDerivesFullnameFile
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=other"
        "-DTRN_TEST_EXPECT_PASS_NAMES=OFF"
        "-DTRN_TEST_EXPECT_BERKELEY_NAMES=OFF"
        -P "${name_parsing_policy_script}")
add_test(NAME ConfigCMakeTest.nameTypeDefaultUsesBsdProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_HAS_GETPWENT=ON"
        "-DTRN_TEST_PROBED_NAME_TYPE=bsd"
        "-DTRN_TEST_EXPECT_NAME_TYPE=bsd"
        -P "${name_parsing_default_script}")
add_test(NAME ConfigCMakeTest.nameTypeDefaultUsesUsgProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_HAS_GETPWENT=ON"
        "-DTRN_TEST_PROBED_NAME_TYPE=usg"
        "-DTRN_TEST_EXPECT_NAME_TYPE=usg"
        -P "${name_parsing_default_script}")
add_test(NAME ConfigCMakeTest.nameTypeDefaultUsesOtherProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_HAS_GETPWENT=ON"
        "-DTRN_TEST_PROBED_NAME_TYPE=other"
        "-DTRN_TEST_EXPECT_NAME_TYPE=other"
        -P "${name_parsing_default_script}")
add_test(NAME ConfigCMakeTest.nameTypeDefaultUsesOtherWithoutGetpwent
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_HAS_GETPWENT=OFF"
        "-DTRN_TEST_PROBED_NAME_TYPE=bsd"
        "-DTRN_TEST_EXPECT_NAME_TYPE=other"
        -P "${name_parsing_default_script}")
add_test(NAME ConfigCMakeTest.nameTypeRejectsBsdWithoutGetpwent
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_NAME_TYPE=bsd"
        "-DTRN_TEST_HAS_GETPWENT=OFF"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${name_parsing_default_script}")
add_test(NAME ConfigCMakeTest.pnewsDistributionCodeAcceptsNone
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_DISTRIBUTION_NAME=LOCAL_DIST"
        "-DTRN_TEST_DISTRIBUTION_VALUE=none"
        -P "${pnews_distribution_validation_script}")
add_test(NAME ConfigCMakeTest.pnewsDistributionCodeRejectsEmpty
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_DISTRIBUTION_NAME=LOCAL_DIST"
        "-DTRN_TEST_DISTRIBUTION_VALUE="
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${pnews_distribution_validation_script}")
set_tests_properties(
    ConfigCMakeTest.nameTypeAcceptsBsd
    ConfigCMakeTest.nameTypeAcceptsUsg
    ConfigCMakeTest.nameTypeAcceptsOther
    ConfigCMakeTest.nameTypeRejectsUnknown
    ConfigCMakeTest.nameTypeBsdDerivesPassNames
    ConfigCMakeTest.nameTypeUsgDerivesPassNames
    ConfigCMakeTest.nameTypeOtherDerivesFullnameFile
    ConfigCMakeTest.nameTypeDefaultUsesBsdProbe
    ConfigCMakeTest.nameTypeDefaultUsesUsgProbe
    ConfigCMakeTest.nameTypeDefaultUsesOtherProbe
    ConfigCMakeTest.nameTypeDefaultUsesOtherWithoutGetpwent
    ConfigCMakeTest.nameTypeRejectsBsdWithoutGetpwent
    ConfigCMakeTest.pnewsDistributionCodeAcceptsNone
    ConfigCMakeTest.pnewsDistributionCodeRejectsEmpty
    PROPERTIES LABELS trn)

set(tool_path_defaults_test_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_tool_path_defaults.cmake")
set(helper_tool_configure_data_script
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/configure_data.cmake")
add_test(NAME ConfigCMakeTest.toolPathDefaultsUseFoundTools
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_CONFIGURE_DATA_CMAKE:FILEPATH=${helper_tool_configure_data_script}"
        "-DTRN_TEST_SCENARIO=found"
        -P "${tool_path_defaults_test_script}")
add_test(NAME ConfigCMakeTest.toolPathDefaultsDisableMissingTools
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_CONFIGURE_DATA_CMAKE:FILEPATH=${helper_tool_configure_data_script}"
        "-DTRN_TEST_SCENARIO=missing"
        -P "${tool_path_defaults_test_script}")
add_test(NAME ConfigCMakeTest.toolPathDefaultsKeepOverrides
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_CONFIGURE_DATA_CMAKE:FILEPATH=${helper_tool_configure_data_script}"
        "-DTRN_TEST_SCENARIO=override"
        -P "${tool_path_defaults_test_script}")
set_tests_properties(
    ConfigCMakeTest.toolPathDefaultsUseFoundTools
    ConfigCMakeTest.toolPathDefaultsDisableMissingTools
    ConfigCMakeTest.toolPathDefaultsKeepOverrides
    PROPERTIES LABELS trn)

set(save_path_defaults_test_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_save_path_defaults.cmake")
add_test(NAME ConfigCMakeTest.savePathDefaultsUseFlatMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_MAKE_SAVE_DIRECTORIES=OFF"
        -P "${save_path_defaults_test_script}")
add_test(NAME ConfigCMakeTest.savePathDefaultsUseCategoryMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_MAKE_SAVE_DIRECTORIES=ON"
        -P "${save_path_defaults_test_script}")
set_tests_properties(
    ConfigCMakeTest.savePathDefaultsUseFlatMode
    ConfigCMakeTest.savePathDefaultsUseCategoryMode
    PROPERTIES LABELS trn)

set(termlib_default_test_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_termlib_default.cmake")
add_test(NAME ConfigCMakeTest.termlibDefaultUsesFoundProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBED_HAS_TERMLIB=ON"
        "-DTRN_TEST_EXPECT_HAS_TERMLIB=ON"
        -P "${termlib_default_test_script}")
add_test(NAME ConfigCMakeTest.termlibDefaultUsesMissingProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBED_HAS_TERMLIB=OFF"
        "-DTRN_TEST_EXPECT_HAS_TERMLIB=OFF"
        -P "${termlib_default_test_script}")
add_test(NAME ConfigCMakeTest.termlibOverrideCanEnableMissingProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_DEFAULT_HAS_TERMLIB=OFF"
        "-DTRN_TEST_HAS_TERMLIB_OVERRIDE=ON"
        "-DTRN_TEST_EXPECT_HAS_TERMLIB=ON"
        -P "${termlib_default_test_script}")
add_test(NAME ConfigCMakeTest.termlibOverrideCanDisableFoundProbe
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_DEFAULT_HAS_TERMLIB=ON"
        "-DTRN_TEST_HAS_TERMLIB_OVERRIDE=OFF"
        "-DTRN_TEST_EXPECT_HAS_TERMLIB=OFF"
        -P "${termlib_default_test_script}")
set_tests_properties(
    ConfigCMakeTest.termlibDefaultUsesFoundProbe
    ConfigCMakeTest.termlibDefaultUsesMissingProbe
    ConfigCMakeTest.termlibOverrideCanEnableMissingProbe
    ConfigCMakeTest.termlibOverrideCanDisableFoundProbe
    PROPERTIES LABELS trn)

set(news_source_defaults_test_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_news_source_defaults.cmake")
add_test(NAME ConfigCMakeTest.newsSourceProbeDetectsLocalOnly
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/news-source-probe-local"
        "-DTRN_TEST_CREATE_LOCAL_SPOOL=ON"
        "-DTRN_TEST_EXPECT_SERVER_NAME=local"
        "-DTRN_TEST_EXPECT_HAS_LOCAL_SPOOL=ON"
        "-DTRN_TEST_EXPECT_ACTIVE=%x/active"
        "-DTRN_TEST_EXPECT_GROUP_DESC=%x/newsgroups"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS=%x/subscriptions"
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=/usr/spool/news"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=/usr/spool/news"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=%x/overview.fmt"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceProbeDetectsNntpOnly
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/news-source-probe-nntp"
        "-DTRN_TEST_CREATE_NNTP_SOURCE=ON"
        "-DTRN_TEST_EXPECT_SERVER_NAME=news.example.org"
        "-DTRN_TEST_EXPECT_HAS_LOCAL_SPOOL=OFF"
        "-DTRN_TEST_EXPECT_ACTIVE=remote"
        "-DTRN_TEST_EXPECT_GROUP_DESC=none"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS="
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=none"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=remote"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=none"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceProbeDetectsBoth
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/news-source-probe-both"
        "-DTRN_TEST_CREATE_LOCAL_SPOOL=ON"
        "-DTRN_TEST_CREATE_NNTP_SOURCE=ON"
        "-DTRN_TEST_EXPECT_SERVER_NAME=news.example.org"
        "-DTRN_TEST_EXPECT_HAS_LOCAL_SPOOL=ON"
        "-DTRN_TEST_EXPECT_ACTIVE=remote"
        "-DTRN_TEST_EXPECT_GROUP_DESC=none"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS="
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=none"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=remote"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=none"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceProbeUsesReleaseDefaultWithoutSources
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_PROBE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/news-source-probe-none"
        "-DTRN_TEST_EXPECT_SERVER_NAME=news.gmane.io"
        "-DTRN_TEST_EXPECT_HAS_LOCAL_SPOOL=OFF"
        "-DTRN_TEST_EXPECT_ACTIVE=remote"
        "-DTRN_TEST_EXPECT_GROUP_DESC=none"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS="
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=none"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=remote"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=none"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceDefaultsUseNntpMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_SERVER_NAME=news.example.org"
        "-DTRN_TEST_HAS_LOCAL_SPOOL=OFF"
        "-DTRN_TEST_EXPECT_ACTIVE=remote"
        "-DTRN_TEST_EXPECT_GROUP_DESC=none"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS="
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=none"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=remote"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=none"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceEmptyDefaultReplacesLegacyPath
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_SCENARIO=empty-default-fallback"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceDefaultsUseLocalMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_SERVER_NAME=local"
        "-DTRN_TEST_HAS_LOCAL_SPOOL=ON"
        "-DTRN_TEST_EXPECT_ACTIVE=%x/active"
        "-DTRN_TEST_EXPECT_GROUP_DESC=%x/newsgroups"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS=%x/subscriptions"
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=/usr/spool/news"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=/usr/spool/news"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=%x/overview.fmt"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceDefaultsUseBothMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_SERVER_NAME=news.example.org"
        "-DTRN_TEST_HAS_LOCAL_SPOOL=ON"
        "-DTRN_TEST_EXPECT_ACTIVE=remote"
        "-DTRN_TEST_EXPECT_GROUP_DESC=none"
        "-DTRN_TEST_EXPECT_SUBSCRIPTIONS="
        "-DTRN_TEST_EXPECT_NEWS_SPOOL=none"
        "-DTRN_TEST_EXPECT_OVERVIEW_DIR=remote"
        "-DTRN_TEST_EXPECT_OVERVIEW_FMT=none"
        -P "${news_source_defaults_test_script}")
add_test(NAME ConfigCMakeTest.newsSourceDefaultsRejectLocalWithoutSpool
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_SERVER_NAME=local"
        "-DTRN_TEST_HAS_LOCAL_SPOOL=OFF"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${news_source_defaults_test_script}")
set_tests_properties(
    ConfigCMakeTest.newsSourceProbeDetectsLocalOnly
    ConfigCMakeTest.newsSourceProbeDetectsNntpOnly
    ConfigCMakeTest.newsSourceProbeDetectsBoth
    ConfigCMakeTest.newsSourceProbeUsesReleaseDefaultWithoutSources
    ConfigCMakeTest.newsSourceDefaultsUseNntpMode
    ConfigCMakeTest.newsSourceEmptyDefaultReplacesLegacyPath
    ConfigCMakeTest.newsSourceDefaultsUseLocalMode
    ConfigCMakeTest.newsSourceDefaultsUseBothMode
    ConfigCMakeTest.newsSourceDefaultsRejectLocalWithoutSpool
    PROPERTIES LABELS trn)

foreach(case IN ITEMS first missing_log missing_binary missing_debugger)
    add_test(NAME ConfigCMakeTest.failureBacktrace_${case}
        COMMAND "${CMAKE_COMMAND}"
        "-DCASE=${case}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/failure-backtrace-${case}"
        "-DREPORT_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/ReportTestFailure.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_failure_backtrace.cmake")
endforeach()
