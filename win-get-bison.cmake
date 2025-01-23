set(ZIP_FILE "../bison-2.4.1.zip")
file(DOWNLOAD "http://user.xmission.com/~legalize/trn/bison-2.4.1.zip" "${ZIP_FILE}"
    EXPECTED_MD5 "6d728df45f1cf60f5569da4963322ec3")
file(ARCHIVE_EXTRACT INPUT "${ZIP_FILE}" DESTINATION "../bison-2.4.1")
