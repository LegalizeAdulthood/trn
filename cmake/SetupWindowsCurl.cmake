# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson
cmake_minimum_required(VERSION 3.23)

if("$ENV{RUNNER_TEMP}" STREQUAL "" OR "$ENV{GITHUB_PATH}" STREQUAL "")
    message(FATAL_ERROR "RUNNER_TEMP and GITHUB_PATH are required")
endif()

# SMTP envelope parameters require curl 8.16.0 or newer.
set(curl_version "8.23.0_3")
set(curl_root "$ENV{RUNNER_TEMP}/trn-curl")
set(curl_archive "curl-${curl_version}-win64-mingw")
file(MAKE_DIRECTORY "${curl_root}")
file(DOWNLOAD
    "https://curl.se/windows/dl-${curl_version}/${curl_archive}.zip"
    "${curl_root}/curl.zip"
    EXPECTED_HASH SHA256=3e1a6c28194f0e2197efed90c1fedd766649b90c1257d49ee176dfccff75498e
    TLS_VERIFY ON)
file(ARCHIVE_EXTRACT INPUT "${curl_root}/curl.zip" DESTINATION "${curl_root}")
set(curl_bin "${curl_root}/${curl_archive}/bin")
execute_process(COMMAND "${curl_bin}/curl.exe" --version COMMAND_ERROR_IS_FATAL ANY)
file(APPEND "$ENV{GITHUB_PATH}" "${curl_bin}\n")
