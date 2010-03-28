#
# This file is part of arm-sim: http://madscientistroom.org/arm-sim
#
# Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
# reversed. (See the file COPYRIGHT for details.)
#

OBJS = sim.o memory.o io.o file.o warn.o forth.o decode.o disassemble.o execute.o arm.o undo.o
INCL = sim.h

CFLAGS = -Wall -Werror -std=c99

ifneq ($(DEBUG),)
	CFLAGS += -ggdb -DDEBUG
else
	CFLAGS += -O2
endif

sim: ${OBJS} ${INCL}
	cc ${OBJS} -o $@

clean:
	rm -f *~
	rm -f *.o
	rm -f sim
