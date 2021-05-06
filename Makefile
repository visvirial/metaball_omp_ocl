CC = gcc
CFLAGS  = $(shell pkg-config --cflags sdl) -ggdb -Wall -O3 -std=gnu99 -fopenmp -mtune=native -march=native
LDFLAGS = $(CFLAGS)
LDLIBS  = $(shell pkg-config --libs sdl) -lm -lOpenCL

.PHONY: all clean

all: metaball

clean:
	$(RM) metaball *.o metaball.cl

metaball.cl: metaball_common.h metaball.base.cl
	cat metaball_common.h metaball.base.cl >$@

metaball: metaball.o metaball.cl
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)
metaball.o: metaball_common.h

