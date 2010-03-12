OBJS = sim.o memory.o io.o image.o warn.o forth.o decode.o disassemble.o execute.o arm.o undo.o
INCL = sim.h

CFLAGS = -Wall -Werror -std=c99

ifneq ($(DEBUG),)
	CFLAGS += -ggdb
else
	CFLAGS += -O2
endif

sim: ${OBJS} ${INCL}
	cc ${OBJS} -o $@

clean:
	rm -f *~
	rm -f *.o
	rm -f sim
