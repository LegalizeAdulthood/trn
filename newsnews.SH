: see end of file for revision information
case $CONFIG in
    '') . ./config.sh ;;
esac
trnversion=`sed -e 's/.*" \(.*\)\"/\1/' $src/patchlevel.h 2>/dev/null`
echo "Extracting newsnews (with version substitution)"
cat >newsnews <<!GROK!THIS!
			*** NEWS NEWS ***

Welcome to trn $trnversion.

Type 'h' at any prompt to get help.  The old (rn-compatible) help
is usually available by typing 'H' as well.

This message will not be displayed again unless it is updated with new
information.

Wayne Davison  <trn@blorf.net>
!GROK!THIS!
#
# This software is Copyright (C) 1991-1997 by Wayne Davison.
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
