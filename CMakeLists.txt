cmake_minimum_required(VERSION 3.22)

project(trn VERSION 5.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckIncludeFile)
include(Configure)

if(WIN32)
    find_package(pdcurses REQUIRED)
    add_library(curses ALIAS pdcurses)
endif()
find_package(sockets REQUIRED)
find_package(BISON REQUIRED)

configure()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(util
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    env.cpp env.h env-internal.h
    util2.cpp util2.h
    wildmat.cpp wildmat.h
    config.h.in
    cmake/Configure.cmake
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("Template Files" REGULAR_EXPRESSION "^.*\\.in$")
if(WIN32)
    target_compile_definitions(util PUBLIC _CRT_DECLARE_NONSTDC_NAMES=1 _CRT_INTERNAL_NONSTDC_NAMES=1)
endif()
target_include_directories(util PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET util PROPERTY FOLDER Libraries)

add_library(nntp
    nntpauth.cpp nntpauth.h
    nntpclient.cpp nntpclient.h
    nntpinit.cpp nntpinit.h
)
target_link_libraries(nntp PUBLIC sockets util)
set_property(TARGET nntp PROPERTY FOLDER Libraries)

add_library(tool
    util3.cpp util3.h
)
target_link_libraries(tool PUBLIC nntp)
set_property(TARGET tool PROPERTY FOLDER Libraries)

bison_target(parsedate parsedate.y ${CMAKE_CURRENT_BINARY_DIR}/parsedate.c)
add_library(parsedate
    ${BISON_parsedate_OUTPUTS} parsedate.h
)
set_target_properties(parsedate PROPERTIES FOLDER Libraries)

add_library(libtrn
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    addng.cpp addng.h
    art.cpp art.h
    artio.cpp artio.h
    artsrch.cpp artsrch.h
    artstate.h
    autosub.cpp autosub.h
    backpage.cpp backpage.h
    bits.cpp bits.h
    cache.cpp cache.h
    charsubst.cpp charsubst.h
    color.cpp color.h
    common.cpp common.h
    config2.h
    datasrc.cpp datasrc.h
    decode.cpp decode.h
    edit_dist.cpp edit_dist.h
    final.cpp final.h
    hash.cpp hash.h
    head.cpp head.h
    help.cpp help.h
    init.cpp init.h
    intrp.cpp intrp.h
    kfile.cpp kfile.h
    last.cpp last.h
    list.cpp list.h
    mempool.cpp mempool.h
    mime.cpp mime.h
    msdos.h
    ndir.cpp ndir.h
    ng.cpp ng.h
    ngdata.cpp ngdata.h
    ngsrch.cpp ngsrch.h
    ngstuff.cpp ngstuff.h
    nntp.cpp nntp.h
    only.cpp only.h
    opt.cpp opt.h
    patchlevel.h
    rcln.cpp rcln.h
    rcstuff.cpp rcstuff.h
    respond.cpp respond.h
    rt-mt.cpp rt-mt.h
    rt-ov.cpp rt-ov.h
    rt-page.cpp rt-page.h
    rt-process.cpp rt-process.h
    rt-select.cpp rt-select.h
    rt-util.cpp rt-util.h
    rt-wumpus.cpp rt-wumpus.h
    rthread.cpp rthread.h
    sacmd.cpp sacmd.h
    sadesc.cpp sadesc.h
    sadisp.cpp sadisp.h
    samain.cpp samain.h
    samisc.cpp samisc.h
    sathread.cpp sathread.h
    scan.cpp scan.h
    scanart.cpp scanart.h
    scmd.cpp scmd.h
    score-easy.cpp score-easy.h
    score.cpp score.h
    scorefile.cpp scorefile.h
    scoresave.cpp scoresave.h
    sdisp.cpp sdisp.h
    search.cpp search.h
    smisc.cpp smisc.h
    sorder.cpp sorder.h
    spage.cpp spage.h
    sw.cpp sw.h
    term.cpp term.h
    trn.cpp trn.h
    typedef.h
    utf.cpp utf.h
    univ.cpp univ.h
    url.cpp url.h
    util.cpp util.h
    uudecode.cpp uudecode.h
)
if(WIN32)
    target_sources(libtrn PRIVATE popen.cpp)
endif()
source_group("Header Files" REGULAR_EXPRESSION "^.*\.h$")
target_include_directories(libtrn PUBLIC .)
target_link_libraries(libtrn PUBLIC parsedate curses nntp)
set_property(TARGET libtrn PROPERTY FOLDER Libraries)

add_executable(inews inews.cpp)
target_link_libraries(inews PUBLIC nntp tool)
set_property(TARGET inews PROPERTY FOLDER Tools)

add_executable(nntplist nntplist.cpp)
target_link_libraries(nntplist PUBLIC nntp tool)
set_property(TARGET nntplist PROPERTY FOLDER Tools)

add_executable(trn-artchk trn-artchk.cpp)
target_link_libraries(trn-artchk PUBLIC nntp tool)
set_property(TARGET trn-artchk PROPERTY FOLDER Tools)

add_executable(trn main.cpp)
target_link_libraries(trn PUBLIC libtrn)

add_subdirectory(tests)
