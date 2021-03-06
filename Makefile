#
# This file is part of arm-sim: http://madscientistroom.org/arm-sim
#
# Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
# reversed. (See the file COPYRIGHT for details.)
#

SRC  = sim.c memory.c io.c file.c warn.c dtc.c decode.c disassemble.c execute.c arm.c undo.c forth.c
OBJS = $(patsubst %.c, objects/%.o, ${SRC})
INCL = sim.h arm.h
AUTOS = fwords.inc

CFLAGS = -Wall -Werror -std=c99

ifneq ($(DEBUG),)
	CFLAGS += -ggdb -DDEBUG
else
	CFLAGS += -O2
endif

sim: clean ${OBJS} ${INCL}
	cc ${OBJS} -o $@

objects/forth.o: fwords.inc

fwords.inc: forth.c forth.h gen_fword_inc.pl
	./gen_fword_inc.pl < $< > $@

.PHONY: objects
objects:
	@mkdir -p objects

objects/%.o: %.c ${INCL} objects
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -f *~
	rm -rf objects
	rm -f sim
	rm -f ${AUTOS}
