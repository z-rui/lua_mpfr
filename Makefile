CC=gcc
CFLAGS=-Wall -g -fPIC
CFLAGS+=-O2
LDFLAGS=-shared
LIBS=-lmpfr -lm

mpfr.so: lua_mpfr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
