OBJS = sim.o memory.o io.o image.o warn.o
INCL = arm.h

CFLAGS = -Wall -Werror -O2

ifneq ($(DEBUG),)
	CFLAGS += -ggdb
endif

sim: ${OBJS} ${INCL}
	cc ${OBJS} -o $@

clean:
	rm -f *~
	rm -f *.o
	rm -f sim
