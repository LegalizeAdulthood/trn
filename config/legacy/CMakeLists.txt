add_library(legacy INTERFACE
    Configure
    Makefile.SH
    MANIFEST.txt
    Rnmail.SH
    hints/aix_rs.sh
    hints/altos486.sh
    hints/convexos.sh
    hints/dec_osf1.sh
    hints/dgux.sh
    hints/domainos.sh
    hints/dynix.sh
    hints/freebsd.sh
    hints/hp9000_800.sh
    hints/hpux.sh
    hints/i386.sh
    hints/irix.sh
    hints/isc_2_2_1.sh
    hints/isc_3_2_2.sh
    hints/isc_3_2_3.sh
    hints/linux.sh
    hints/mc6000.sh
    hints/mips.sh
    hints/next.sh
    hints/sco_3.sh
    hints/sco_3_2_4.sh
    hints/sco_xenix.sh
    hints/sgi.sh
    hints/solaris_2.sh
    hints/sunos.sh
    hints/svr4.sh
    U/bin.U
    U/filexp.U
    U/filexp2.U
    U/Loc.U
    U/locdist.U
    U/mansrc.U
    U/mimecap.U
    U/perlpath.U
)
source_group("MetaConfig Units" REGULAR_EXPRESSION "^.*\\.U$")
source_group("Shell Templates" REGULAR_EXPRESSION "^.*\\.SH$")
source_group("Configure Hints" REGULAR_EXPRESSION "^.*hints/.*\\.sh$")
target_folder(legacy "Libraries")
