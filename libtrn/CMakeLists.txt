add_library(libtrn
    addng.cpp include/trn/addng.h
    art.cpp include/trn/art.h
    artio.cpp include/trn/artio.h
    artsrch.cpp include/trn/artsrch.h
    include/trn/artstate.h
    autosub.cpp include/trn/autosub.h
    backpage.cpp include/trn/backpage.h
    bits.cpp include/trn/bits.h
    cache.cpp include/trn/cache.h
    include/trn/change_dir.h
    charsubst.cpp include/trn/charsubst.h
    color.cpp include/trn/color.h
    datasrc.cpp include/trn/datasrc.h
    decode.cpp include/trn/decode.h
    edit_dist.cpp include/trn/edit_dist.h
    include/trn/enum-flags.h
    final.cpp include/trn/final.h
    hash.cpp include/trn/hash.h
    head.cpp include/trn/head.h
    help.cpp include/trn/help.h
    init.cpp include/trn/init.h
    intrp.cpp include/trn/intrp.h
    kfile.cpp include/trn/kfile.h
    last.cpp include/trn/last.h
    list.cpp include/trn/list.h
    mempool.cpp include/trn/mempool.h
    mime.cpp include/trn/mime.h include/trn/mime-internal.h
    ng.cpp include/trn/ng.h
    ngdata.cpp include/trn/ngdata.h
    ngsrch.cpp include/trn/ngsrch.h
    ngstuff.cpp include/trn/ngstuff.h
    nntp.cpp include/trn/nntp.h
    only.cpp include/trn/only.h
    opt.cpp include/trn/opt.h
    include/trn/patchlevel.h
    rcln.cpp include/trn/rcln.h
    rcstuff.cpp include/trn/rcstuff.h
    respond.cpp include/trn/respond.h
    rt-ov.cpp include/trn/rt-ov.h
    rt-page.cpp include/trn/rt-page.h
    rt-process.cpp include/trn/rt-process.h
    rt-select.cpp include/trn/rt-select.h
    rt-util.cpp include/trn/rt-util.h
    rt-wumpus.cpp include/trn/rt-wumpus.h
    rthread.cpp include/trn/rthread.h
    sacmd.cpp include/trn/sacmd.h
    sadesc.cpp include/trn/sadesc.h
    sadisp.cpp include/trn/sadisp.h
    samain.cpp include/trn/samain.h
    samisc.cpp include/trn/samisc.h
    sathread.cpp include/trn/sathread.h
    scan.cpp include/trn/scan.h
    scanart.cpp include/trn/scanart.h
    scmd.cpp include/trn/scmd.h
    score-easy.cpp include/trn/score-easy.h
    score.cpp include/trn/score.h
    scorefile.cpp include/trn/scorefile.h
    scoresave.cpp include/trn/scoresave.h
    sdisp.cpp include/trn/sdisp.h
    search.cpp include/trn/search.h
    smisc.cpp include/trn/smisc.h
    sorder.cpp include/trn/sorder.h
    spage.cpp include/trn/spage.h
    include/trn/string-algos.h
    sw.cpp include/trn/sw.h
    terminal.cpp include/trn/terminal.h
    trn.cpp include/trn/trn.h
    utf.cpp include/trn/utf.h
    univ.cpp include/trn/univ.h
    url.cpp include/trn/url.h
    util.cpp include/trn/util.h
    uudecode.cpp include/trn/uudecode.h
)
# We require <filesystem>
target_compile_features(libtrn PUBLIC cxx_std_17)
source_group("Header Files" REGULAR_EXPRESSION "^.*\\.h$")
target_include_directories(libtrn PUBLIC include)
target_link_libraries(libtrn PUBLIC parsedate curses nntp)
target_folder(libtrn "Libraries")
