add_library(libtrn
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
    datasrc.cpp datasrc.h
    decode.cpp decode.h
    edit_dist.cpp edit_dist.h
    enum-flags.h
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
    mime.cpp mime.h mime-internal.h
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
    string-algos.h
    sw.cpp sw.h
    term.cpp term.h
    trn.cpp trn.h
    utf.cpp utf.h
    univ.cpp univ.h
    url.cpp url.h
    util.cpp util.h
    uudecode.cpp uudecode.h
)
# We require <filesystem>
target_compile_features(libtrn PUBLIC cxx_std_17)
source_group("Header Files" REGULAR_EXPRESSION "^.*\\.h$")
target_include_directories(libtrn PUBLIC .)
target_link_libraries(libtrn PUBLIC parsedate curses nntp)
set_property(TARGET libtrn PROPERTY FOLDER Libraries)
