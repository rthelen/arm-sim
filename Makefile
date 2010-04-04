#
# This file is part of arm-sim: http://madscientistroom.org/arm-sim
#
# Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
# reversed. (See the file COPYRIGHT for details.)
#

SRC  = sim.c memory.c io.c file.c warn.c dtc.c decode.c disassemble.c execute.c arm.c undo.c forth.c
OBJS = $(patsubst %.c, objects/%.o, ${SRC})
AUTOS = fword_dict.c fword_imm_dict.c fword_decls.c
INCL = sim.h arm.h

CFLAGS = -Wall -Werror -std=c99

ifneq ($(DEBUG),)
	CFLAGS += -ggdb -DDEBUG
else
	CFLAGS += -O2
endif

sim: ${OBJS} ${INCL}
	cc ${OBJS} -o $@

objects/forth.o: ${AUTOS}

fword_dict.c : forth.c gen_dict_chain.sed
	sed -E -f gen_dict_chain.sed < $< > $@

fword_imm_dict.c : forth.c gen_dict_imm.sed
	sed -E -f gen_dict_imm.sed < $< > $@

fword_decls.c : forth.c gen_dict_decls.sed
	sed -E -f gen_dict_decls.sed < $< > $@

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
