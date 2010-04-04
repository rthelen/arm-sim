# This file is part of arm-sim: http://madscientistroom.org/arm-sim/
#
# Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
# reversed. (See the file COPYRIGHT for details.)

# This file is derived from David Frech's muForth gen_dict_chain.sed script
# which has the copyright notice:

# This file is part of muFORTH: http://muforth.nimblemachines.com/
#
# Copyright (c) 2002-2009 David Frech. All rights reserved, and all wrongs
# reversed. (See the file COPYRIGHT for details.)


#
# It looks a bit complicated. ;-) The idea is to automagically convert from a
# list of the C names of functions that implement forth words to a list of
# their forth names, formatted to initialize a C array. There are a few
# exceptional cases, but mostly it's pretty straightforward.
#
# sed rocks!
#

# keep any #if* #else or #endif lines, so we can include optional sections
/^#(if|else|endif)/p

# lose lines *not* starting with "void fword_"
/^void fword_do_/!d

s/^void fword_do_//
s/(.*)\(F f\).*$/\1/

# now we've got the name, save it in hold space
h

s/push_//
s/less/</
s/equal/=/
s/zero/0/
s/reset/!/
s/star(_|$)/*/
s/backslash/\\\\/
s/slash/\//
s/plus/+/
s/minus/-/
s/shift_left/<</
s/shift_right/>>/
s/fetch/@/
s/store/!/
s/(.*)_chain/\.\1\./
s/set_(.*)_code/<\1>/
s/lbracket/[/
s/rbracket/]/
s/semicolon/;/
s/colon/:/
s/comma$/,/
s/tick/'/
s/exit/^/
s/q/?/
s/colon/:/
s/forward/>/
s/to(_|$)/>/
s/from(_|$)/>/
s/(.*)_size/#\1/

# turn foo_ to (foo)
s/((.*)_)$/\(\2\)/

s/([a-z])_([^a-z])/\1\2/g
s/([^a-z])_([a-z])/\1\2/g
s/([^a-z])_([^a-z])/\1\2/g
s/([a-z])_([a-z])/\1-\2/g

# concat hold & pattern
H
g

# output final string for array initializer
s/(.*)\n(.*)/	{ "(\2)", forth_do_\1, 0 },/

