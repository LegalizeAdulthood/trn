case $CONFIG in
    '') . ./config.sh ;;
esac
echo "Extracting myinstall (with variable substitutions)"
$spitshell >myinstall <<!GROK!THIS!
$startsh
# myinstall.SH
# 
# This software is Copyright 1995 by Wayne Davison.
#
# Permission is hereby granted to copy, reproduce, redistribute or otherwise
# use this software as long as: there is no monetary profit gained
# specifically from the use or reproduction of this software, it is not
# sold, rented, traded or otherwise marketed, and this copyright notice is
# included prominently in any copy made. 
#
# The author make no claims as to the fitness or correctness of this software
# for any use whatsoever, and it is provided as is. Any use of this software
# is at the user's own risk. 
export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh \$0; kill \$\$)

objdir=`pwd`
!GROK!THIS!
$cat >>myinstall <<'!NO!SUBS!'

case "$1" in
-*) args=$1
    shift
    case "$args" in
    *f*) tofile=$1 ; shift ;;
    esac
esac
case "$#" in
0|1) echo "Usage: myinstall [-opts] dir file [file...]"
     exit 1
esac
dir=$1
shift
for file in $* ; do
    case "$args" in
    *f*) ;;
    *)   tofile=`echo $file | sed -e 's!.*/\(.*\)!\1!' 2>/dev/null`
    esac
    if test -f $dir/$tofile ; then
	case "$args" in
	*d*)
	    if cmp -s $file $dir/$tofile ; then
		continue
	    fi
	    ;;
	esac
	case "$args" in
	*o*) rm -f $dir/$tofile.old
	     mv $dir/$tofile $dir/$tofile.old;;
	*n*) cat <<EOM

** You should diff $tofile & $tofile.new in $dir **
EOM
	     tofile="$tofile.new";;
	esac
    elif test -d $dir/$tofile ; then
	case "$args" in
	*o*) rm -rf $dir/$tofile.old
	     mv $dir/$tofile $dir/$tofile.old;;
	esac
    fi

    case "$args" in
    *R*)
	fromname=`basename $file`
	fromdir=`expr $file : '\(.*\)/[^/]*'`
	if test "X$fromname" = X -o "X$fromdir" = X; then
	    exit 1
	fi
	cd $dir
	(cd $fromdir ; tar cf - $fromname) | tar xpf -
	cd $objdir
	;;
    *)
	cp $file $dir/$tofile
	;;
    esac

    case "$args" in
    *s*) strip $dir/$tofile
    esac
    case "$args" in
    *x*) chmod +x $dir/$tofile
    esac
    case "$args" in
    *r*) chmod +r $dir/$tofile
    esac
done
!NO!SUBS!
$eunicefix myinstall
chmod 755 myinstall
