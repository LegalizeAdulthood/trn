include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_env.cpp
    test_file_ref.cpp
    test_generated_config.cpp
    test_path_list.cpp
    test_string_case_compare.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
target_include_directories(config-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_folder(config-test "Tests")
gtest_discover_tests(config-test PROPERTIES LABELS trn)

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
    foreach(case IN ITEMS bytes invalid_offset arity missing_input missing_parent locked_output same_file)
        add_test(NAME NormSaverTest.${case}
            COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
                -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/test_norm_saver.ps1"
                -Helper "${TRN_GENERATED_NORM_SAVER_FILE}"
                -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/norm-saver-${case}"
                -Case "${case}")
        set_tests_properties(NormSaverTest.${case} PROPERTIES LABELS trn)
    endforeach()
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
        list(APPEND mbox_cases spaces)
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
        foreach(case IN ITEMS bytes boundary unicode_envelope invalid_offset arity missing_input missing_parent locked_output same_file)
            add_test(NAME MboxSaverTest.${format}_${case}
                COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
                    -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/test_mbox_saver.ps1"
                    -Helper "${test_mbox_helper}"
                    -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/mbox-${format}-${case}"
                    -Case "${case}" -Mmdf "${TRN_MBOX_SAVER_MMDF}")
            set_tests_properties(MboxSaverTest.${format}_${case} PROPERTIES LABELS trn)
        endforeach()
    endif()
endforeach()
set(MBOX_CHAR "${test_saved_mbox_char}")

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

add_test(NAME ConfigCMakeTest.installLayoutContainsRuntimeHelpers
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DTRN_TEST_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/install-layout"
        "-DTRN_TEST_CONFIG=$<CONFIG>"
        "-DTRN_TEST_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DTRN_TEST_PRIVATE_LIBDIR=${TRN_INSTALL_PRIVATE_LIBDIR}"
        "-DTRN_TEST_MANDIR=${CMAKE_INSTALL_MANDIR}"
        "-DTRN_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DTRN_TEST_MBOX_SAVER_HELPER_NAME=${TRN_INSTALL_MBOX_SAVER_HELPER_NAME}"
        "-DTRN_TEST_MBOX_FORMAT=${TRN_TEST_MBOX_FORMAT}"
        "-DTRN_TEST_NORM_SAVER_HELPER_NAME=${TRN_INSTALL_NORM_SAVER_HELPER_NAME}"
        "-DTRN_TEST_PNEWS_HELPER_NAME=${TRN_INSTALL_PNEWS_HELPER_NAME}"
        "-DTRN_TEST_PNEWS_HEADER_NAME=${TRN_INSTALL_PNEWS_HEADER_NAME}"
        "-DTRN_TEST_RNMAIL_HELPER_NAME=${TRN_INSTALL_RNMAIL_HELPER_NAME}"
        "-DTRN_TEST_SPELLER_HELPER_NAME=${TRN_INSTALL_SPELLER_HELPER_NAME}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_install_layout.cmake")
set_tests_properties(
    ConfigCMakeTest.installLayoutContainsRuntimeHelpers
    PROPERTIES LABELS trn)

add_test(NAME ConfigCMakeTest.packageZipContainsInstallContents
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DTRN_TEST_PACKAGE_DIR=${CMAKE_CURRENT_BINARY_DIR}/package-layout"
        "-DTRN_TEST_CPACK_COMMAND=${CMAKE_CPACK_COMMAND}"
        "-DTRN_TEST_CONFIG=$<CONFIG>"
        "-DTRN_TEST_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DTRN_TEST_PRIVATE_LIBDIR=${TRN_INSTALL_PRIVATE_LIBDIR}"
        "-DTRN_TEST_MANDIR=${CMAKE_INSTALL_MANDIR}"
        "-DTRN_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DTRN_TEST_MBOX_SAVER_HELPER_NAME=${TRN_INSTALL_MBOX_SAVER_HELPER_NAME}"
        "-DTRN_TEST_MBOX_FORMAT=${TRN_TEST_MBOX_FORMAT}"
        "-DTRN_TEST_NORM_SAVER_HELPER_NAME=${TRN_INSTALL_NORM_SAVER_HELPER_NAME}"
        "-DTRN_TEST_PNEWS_HELPER_NAME=${TRN_INSTALL_PNEWS_HELPER_NAME}"
        "-DTRN_TEST_PNEWS_HEADER_NAME=${TRN_INSTALL_PNEWS_HEADER_NAME}"
        "-DTRN_TEST_RNMAIL_HELPER_NAME=${TRN_INSTALL_RNMAIL_HELPER_NAME}"
        "-DTRN_TEST_SPELLER_HELPER_NAME=${TRN_INSTALL_SPELLER_HELPER_NAME}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_package_layout.cmake")
set_tests_properties(
    ConfigCMakeTest.packageZipContainsInstallContents
    PROPERTIES LABELS trn)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    input_signal_probes.h
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    wait_status_probe.h
    @ONLY
)

set(HAS_TERMLIB "${DEFAULT_HAS_TERMLIB}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    termlib_probe.h
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    platform_policy_default.h
    @ONLY
)

set(SERVER_NAME "${DEFAULT_SERVER_NAME}")
set(HAS_LOCAL_SPOOL "${DEFAULT_HAS_LOCAL_SPOOL}")
set(ACTIVE_TIMES "none")
configure_news_source_defaults()
set(ACTIVE "${DEFAULT_ACTIVE}")
set(GROUP_DESC "${DEFAULT_GROUP_DESC}")
set(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}")
set(NEWS_SPOOL "${DEFAULT_NEWS_SPOOL}")
configure_news_overview_dir_default()
set(OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}")
configure_news_overview_format_default()
set(OVERVIEW_FMT "${DEFAULT_OVERVIEW_FMT}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    news_source_default.h
    @ONLY
)

function(configure_news_source_config output server_name has_local_spool)
    set(SERVER_NAME "${server_name}")
    set(HAS_LOCAL_SPOOL "${has_local_spool}")
    set(ACTIVE_TIMES "none")
    configure_news_source_defaults()
    set(ACTIVE "${DEFAULT_ACTIVE}")
    set(GROUP_DESC "${DEFAULT_GROUP_DESC}")
    set(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}")
    set(NEWS_SPOOL "${DEFAULT_NEWS_SPOOL}")
    configure_news_overview_dir_default()
    set(OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}")
    configure_news_overview_format_default()
    set(OVERVIEW_FMT "${DEFAULT_OVERVIEW_FMT}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
        "${output}"
        @ONLY
    )
endfunction()

configure_news_source_config(news_source_nntp.h "news.example.org" OFF)
configure_news_source_config(news_source_local.h "local" ON)
configure_news_source_config(news_source_both.h "news.example.org" ON)

set(ACTIVE_TIMES "none")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_times_default.h
    @ONLY
)

set(ACTIVE_TIMES "%x/active.times")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_times_configured.h
    @ONLY
)

set(ACTIVE "%x/test-active")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_configured.h
    @ONLY
)

set(MAIL_FILE "${DEFAULT_MAIL_FILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    mail_file_default.h
    @ONLY
)

set(MAIL_FILE "%./mailbox")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    mail_file_configured.h
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    mimecap_default.h
    @ONLY
)

set(NEWS_LIB "${DEFAULT_NEWS_LIB}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    news_lib_default.h
    @ONLY
)

set(NEWS_LIB "/var/lib/trn/news")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    news_lib_configured.h
    @ONLY
)

set(GROUP_DESC "/var/lib/trn/newsgroups")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    group_desc_configured.h
    @ONLY
)

set(SUBSCRIPTIONS "/var/lib/trn/subscriptions")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    subscriptions_configured.h
    @ONLY
)

set(OVERVIEW_DIR "/var/spool/news/overview")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_dir_configured.h
    @ONLY
)

set(OVERVIEW_FMT "/var/lib/trn/overview.fmt")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_fmt_configured.h
    @ONLY
)

set(POSTING_HOST_MATCH_SEGMENTS "${DEFAULT_POSTING_HOST_MATCH_SEGMENTS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_host_match_segments_default.h
    @ONLY
)

set(POSTING_HOST_MATCH_SEGMENTS "4")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_host_match_segments_configured.h
    @ONLY
)

set(NAME_TYPE "${DEFAULT_NAME_TYPE}")
configure_name_parsing_policy("${NAME_TYPE}")
set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_default.h
    @ONLY
)

set(NAME_TYPE "other")
configure_name_parsing_policy("${NAME_TYPE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_fullname_file.h
    @ONLY
)

set(NAME_TYPE "usg")
configure_name_parsing_policy("${NAME_TYPE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_usg.h
    @ONLY
)

set(NAME_TYPE "bsd")
configure_name_parsing_policy("${NAME_TYPE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_berkeley.h
    @ONLY
)

set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    account_name_files_default.h
    @ONLY
)

set(PASSWORD_FILE "/var/lib/trn/passwd")
set(LOGIN_DIR_FIELD "7")
set(FULLNAMEFILE "%./profile-name")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    account_name_files_configured.h
    @ONLY
)

set(TRNDIR "${DEFAULT_TRNDIR}")
set(TRNMACRO "${DEFAULT_TRNMACRO}")
set(RNMACRO "${DEFAULT_RNMACRO}")
set(RCNAME "${DEFAULT_RCNAME}")
set(LOCKNAME "${DEFAULT_LOCKNAME}")
set(LASTNAME "${DEFAULT_LASTNAME}")
set(SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}")
set(NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    user_state_paths_default.h
    @ONLY
)

set(TRNDIR "%./trn-test")
set(TRNMACRO "%+/test-macros")
set(RNMACRO "%./test-rnmac")
set(RCNAME "%./test-newsrc")
set(LOCKNAME "%./test-rnlock")
set(LASTNAME "%./test-rnlast")
set(SIGNATURE_FILE "%./test-signature")
set(NNTP_AUTH_FILE "%./test-nntpauth")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    user_state_paths_configured.h
    @ONLY
)

set(GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}")
set(DEFACCESS "${DEFAULT_DEFACCESS}")
set(TRNACCESS "${DEFAULT_TRNACCESS}")
set(NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    data_file_paths_default.h
    @ONLY
)

set(GLOBAL_INIT "%X/test-INIT")
set(DEFACCESS "%X/test-access.def")
set(TRNACCESS "%+/test-access")
set(NEWSNEWSNAME "%X/test-newsnews")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    data_file_paths_configured.h
    @ONLY
)

set(OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}")
set(VARYNAME "${DEFAULT_VARYNAME}")
set(HEADNAME "${DEFAULT_HEADNAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    article_path_templates_default.h
    @ONLY
)

set(OV_FILE_NAME "/test-overview")
set(VARYNAME "%Y/test-vary.%$")
set(HEADNAME "%./test-head.%$")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    article_path_templates_configured.h
    @ONLY
)

set(SAVEDIR "${DEFAULT_SAVEDIR}")
set(SAVENAME "${DEFAULT_SAVENAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    save_path_templates_default.h
    @ONLY
)

configure_save_path_defaults(ON)
set(SAVEDIR "${DEFAULT_SAVEDIR}")
set(SAVENAME "${DEFAULT_SAVENAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    save_path_templates_category.h
    @ONLY
)
configure_save_path_defaults("${TRN_MAKE_SAVE_DIRECTORIES}")

set(SAVEDIR "%p/test-save")
set(SAVENAME "test-%a")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    save_path_templates_configured.h
    @ONLY
)

set(KILL_GLOBAL "${DEFAULT_KILL_GLOBAL}")
set(KILL_LOCAL "${DEFAULT_KILL_LOCAL}")
set(KILL_THREADS "${DEFAULT_KILL_THREADS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    kill_file_paths_default.h
    @ONLY
)

set(KILL_GLOBAL "%p/test-global-kill")
set(KILL_LOCAL "%p/%c/test-local-kill")
set(KILL_THREADS "%+/test-thread-kill")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    kill_file_paths_configured.h
    @ONLY
)

set(UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}")
set(SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    selection_policy_default.h
    @ONLY
)

set(UNSUBSCRIBED_CHAR "?")
set(SELECTION_CHARS "xyz789")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    selection_policy_configured.h
    @ONLY
)

set(LOCALTIME_FMT "${DEFAULT_LOCALTIME_FMT}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    local_time_format_default.h
    @ONLY
)

set(LOCALTIME_FMT "test %Y")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    local_time_format_configured.h
    @ONLY
)

set(MAIL_CALL "${DEFAULT_MAIL_CALL}")
set(NO_FIREWORKS "${DEFAULT_NO_FIREWORKS}")
set(TILDE_NAME "${DEFAULT_TILDE_NAME}")
set(VALIDATE_XREF_SITE "${DEFAULT_VALIDATE_XREF_SITE}")
set(USE_FTP "${DEFAULT_USE_FTP}")
set(REPLYTO_POSTER_CHECKING "${DEFAULT_REPLYTO_POSTER_CHECKING}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    feature_policy_default.h
    @ONLY
)

set(MAIL_CALL OFF)
set(NO_FIREWORKS OFF)
set(TILDE_NAME OFF)
set(VALIDATE_XREF_SITE ON)
set(USE_FTP OFF)
set(REPLYTO_POSTER_CHECKING ON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    feature_policy_configured.h
    @ONLY
)

set(SH "${DEFAULT_SH}")
set(PIPE_SAVER "${DEFAULT_PIPE_SAVER}")
set(SHAR_SAVER "${DEFAULT_SHAR_SAVER}")
set(CUSTOM_SAVER "${DEFAULT_CUSTOM_SAVER}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    shell_command_defaults_default.h
    @ONLY
)

set(SH "/test/sh")
set(PIPE_SAVER "test-pipe %A %b")
set(SHAR_SAVER "test-shar %A")
set(CUSTOM_SAVER "test-custom %A %e")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    shell_command_defaults_configured.h
    @ONLY
)

set(VERIFY_RIPEM "${DEFAULT_VERIFY_RIPEM}")
set(VERIFY_PGP "${DEFAULT_VERIFY_PGP}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    verification_commands_default.h
    @ONLY
)

set(VERIFY_RIPEM "")
set(VERIFY_PGP "")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    verification_commands_disabled.h
    @ONLY
)

set(VERIFY_RIPEM "test-ripem %A")
set(VERIFY_PGP "test-pgp %A")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    verification_commands_configured.h
    @ONLY
)

set(CALL_INEWS "${DEFAULT_CALL_INEWS}")
set(NEWS_POSTER "${DEFAULT_NEWS_POSTER}")
set(MAIL_POSTER "${DEFAULT_MAIL_POSTER}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_helper_paths_default.h
    @ONLY
)

set(CALL_INEWS "/test/bin/inews -h <%h")
set(NEWS_POSTER "/test/bin/Pnews -h %h")
set(MAIL_POSTER "/test/bin/Rnmail -h %h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_helper_paths_configured.h
    @ONLY
)

set(CALL_INEWS "${DEFAULT_CALL_INEWS}")
set(NEWS_POSTER "${DEFAULT_NEWS_POSTER}")
set(MAIL_POSTER "${DEFAULT_MAIL_POSTER}")
set(MAIL_HEADER "${DEFAULT_MAIL_HEADER}")
set(NEWS_HEADER "${DEFAULT_NEWS_HEADER}")
set(CANCEL_HEADER "${DEFAULT_CANCEL_HEADER}")
set(SUPERSEDE_HEADER "${DEFAULT_SUPERSEDE_HEADER}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_header_templates_default.h
    @ONLY
)

set(MAIL_HEADER [=[test-mail\n]=])
set(NEWS_HEADER [=[test-news\n]=])
set(CANCEL_HEADER [=[test-cancel\n]=])
set(SUPERSEDE_HEADER [=[test-supersede\n]=])
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_header_templates_configured.h
    @ONLY
)

set(YOU_SAID "${DEFAULT_YOU_SAID}")
set(ATTRIBUTION "${DEFAULT_ATTRIBUTION}")
set(FORWARD_POSTER "${DEFAULT_FORWARD_POSTER}")
set(FORWARD_HEADER "${DEFAULT_FORWARD_HEADER}")
set(FORWARD_MSG "${DEFAULT_FORWARD_MSG}")
set(FORWARD_MSG_END "${DEFAULT_FORWARD_MSG_END}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    reply_forward_templates_default.h
    @ONLY
)

set(SHELL_START "${DEFAULT_SHELL_START}")
set(PAGER "${DEFAULT_PAGER}")
set(DIFF_PROGRAM "${DEFAULT_DIFF_PROGRAM}")
set(ED_PROGRAM "${DEFAULT_ED_PROGRAM}")
set(ISPELL_PROGRAM "${DEFAULT_ISPELL_PROGRAM}")
set(ISPELL_OPTIONS "${DEFAULT_ISPELL_OPTIONS}")
set(SPELL_PROGRAM "${DEFAULT_SPELL_PROGRAM}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/Speller.in"
    helper_tools_default
    @ONLY
)

set(SHELL_START "#!/test/sh")
set(PAGER "/test/bin/more")
set(DIFF_PROGRAM "/test/bin/diff")
set(ED_PROGRAM "/test/bin/ed")
set(ISPELL_PROGRAM "/test/bin/ispell")
set(ISPELL_OPTIONS "-x -Tlatin1")
set(SPELL_PROGRAM "/test/bin/spell")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/Speller.in"
    helper_tools_configured
    @ONLY
)

set(NAME_TYPE "${DEFAULT_NAME_TYPE}")
set(LOCAL_DIST "${DEFAULT_LOCAL_DIST}")
set(ORGANIZATION_DIST "${DEFAULT_ORGANIZATION_DIST}")
set(CITY_DIST "${DEFAULT_CITY_DIST}")
set(STATE_DIST "${DEFAULT_STATE_DIST}")
set(MULTISTATE_DIST "${DEFAULT_MULTISTATE_DIST}")
set(COUNTRY_DIST "${DEFAULT_COUNTRY_DIST}")
set(CONTINENT_DIST "${DEFAULT_CONTINENT_DIST}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/Pnews.in"
    pnews_distribution_defaults
    @ONLY
)

set(NAME_TYPE "bsd")
set(LOCAL_DIST "local-test")
set(ORGANIZATION_DIST "org-test")
set(CITY_DIST "city-test")
set(STATE_DIST "state-test")
set(MULTISTATE_DIST "multi-test")
set(COUNTRY_DIST "country-test")
set(CONTINENT_DIST "continent-test")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/Pnews.in"
    pnews_distribution_configured
    @ONLY
)

set(NAME_TYPE "${DEFAULT_NAME_TYPE}")
configure_name_parsing_policy("${NAME_TYPE}")
set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
set(TRNDIR "${DEFAULT_TRNDIR}")
set(TRNMACRO "${DEFAULT_TRNMACRO}")
set(RNMACRO "${DEFAULT_RNMACRO}")
set(RCNAME "${DEFAULT_RCNAME}")
set(LOCKNAME "${DEFAULT_LOCKNAME}")
set(LASTNAME "${DEFAULT_LASTNAME}")
set(SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}")
set(NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}")
set(GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}")
set(DEFACCESS "${DEFAULT_DEFACCESS}")
set(TRNACCESS "${DEFAULT_TRNACCESS}")
set(NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}")
set(OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}")
set(VARYNAME "${DEFAULT_VARYNAME}")
set(HEADNAME "${DEFAULT_HEADNAME}")
set(SAVEDIR "${DEFAULT_SAVEDIR}")
set(SAVENAME "${DEFAULT_SAVENAME}")
set(KILL_GLOBAL "${DEFAULT_KILL_GLOBAL}")
set(KILL_LOCAL "${DEFAULT_KILL_LOCAL}")
set(KILL_THREADS "${DEFAULT_KILL_THREADS}")
set(UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}")
set(SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}")
set(LOCALTIME_FMT "${DEFAULT_LOCALTIME_FMT}")
set(MAIL_CALL "${DEFAULT_MAIL_CALL}")
set(NO_FIREWORKS "${DEFAULT_NO_FIREWORKS}")
set(TILDE_NAME "${DEFAULT_TILDE_NAME}")
set(VALIDATE_XREF_SITE "${DEFAULT_VALIDATE_XREF_SITE}")
set(USE_FTP "${DEFAULT_USE_FTP}")
set(REPLYTO_POSTER_CHECKING "${DEFAULT_REPLYTO_POSTER_CHECKING}")
set(SH "${DEFAULT_SH}")
set(PIPE_SAVER "${DEFAULT_PIPE_SAVER}")
set(SHAR_SAVER "${DEFAULT_SHAR_SAVER}")
set(CUSTOM_SAVER "${DEFAULT_CUSTOM_SAVER}")
set(VERIFY_RIPEM "${DEFAULT_VERIFY_RIPEM}")
set(VERIFY_PGP "${DEFAULT_VERIFY_PGP}")
set(CALL_INEWS "${DEFAULT_CALL_INEWS}")
set(NEWS_POSTER "${DEFAULT_NEWS_POSTER}")
set(MAIL_POSTER "${DEFAULT_MAIL_POSTER}")
set(MAIL_HEADER "${DEFAULT_MAIL_HEADER}")
set(NEWS_HEADER "${DEFAULT_NEWS_HEADER}")
set(CANCEL_HEADER "${DEFAULT_CANCEL_HEADER}")
set(SUPERSEDE_HEADER "${DEFAULT_SUPERSEDE_HEADER}")
set(YOU_SAID "${DEFAULT_YOU_SAID}")
set(ATTRIBUTION "${DEFAULT_ATTRIBUTION}")
set(FORWARD_POSTER "${DEFAULT_FORWARD_POSTER}")
set(FORWARD_HEADER "${DEFAULT_FORWARD_HEADER}")
set(FORWARD_MSG "${DEFAULT_FORWARD_MSG}")
set(FORWARD_MSG_END "${DEFAULT_FORWARD_MSG_END}")
set(NAME_TYPE "${DEFAULT_NAME_TYPE}")
set(LOCAL_DIST "${DEFAULT_LOCAL_DIST}")
set(ORGANIZATION_DIST "${DEFAULT_ORGANIZATION_DIST}")
set(CITY_DIST "${DEFAULT_CITY_DIST}")
set(STATE_DIST "${DEFAULT_STATE_DIST}")
set(MULTISTATE_DIST "${DEFAULT_MULTISTATE_DIST}")
set(COUNTRY_DIST "${DEFAULT_COUNTRY_DIST}")
set(CONTINENT_DIST "${DEFAULT_CONTINENT_DIST}")
set(SHELL_START "${DEFAULT_SHELL_START}")
set(PAGER "${DEFAULT_PAGER}")
set(DIFF_PROGRAM "${DEFAULT_DIFF_PROGRAM}")
set(ED_PROGRAM "${DEFAULT_ED_PROGRAM}")
set(ISPELL_PROGRAM "${DEFAULT_ISPELL_PROGRAM}")
set(ISPELL_OPTIONS "${DEFAULT_ISPELL_OPTIONS}")
set(SPELL_PROGRAM "${DEFAULT_SPELL_PROGRAM}")

configure_file(cmake/test_generated_config.h.in test_generated_config.h @ONLY)

foreach(case IN ITEMS first missing_log missing_binary missing_debugger)
    add_test(NAME ConfigCMakeTest.failureBacktrace_${case}
        COMMAND "${CMAKE_COMMAND}"
        "-DCASE=${case}"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/failure-backtrace-${case}"
        "-DREPORT_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/ReportTestFailure.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_failure_backtrace.cmake")
endforeach()
