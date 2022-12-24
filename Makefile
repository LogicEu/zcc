# zcc makefile

TARGET = zcc

CC = gcc
STD = -std=c89
WFLAGS = -Wall -Wextra -pedantic
OPT = -O2 -fno-stack-protector
INC = -Izlibc/src/include -Iutopia -Isrc
LIB = zlibc utopia
NOSTD = -nostdlib -nostartfiles

SRCDIR = src
TMPDIR = tmp
LIBDIR = lib
CRTDIR = zlibc/src/crt/

SCRIPT = build.sh

CRT = $(wildcard $(CRTDIR)/*.c)
SRC = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(TMPDIR)/%.o,$(SRC))
CRTO = $(patsubst $(CRTDIR)/%.c,$(TMPDIR)/%.o,$(CRT))
LIBS = $(patsubst %,$(LIBDIR)/lib%.a,$(LIB))
LINC = -L$(LIBDIR)
LINC += $(patsubst %,-l%,$(LIB))
NOMAIN = $(filter-out $(TMPDIR)/main.o,$(OBJS))

OS=$(shell uname -s)
ifeq ($(OS),Darwin)
	OSLIB = -lSystem
	NOSTD += -e _start
	SUFFIX = .dylib
else
	OSLIB = -lgcc -lc
	NOSTD += -e start
	SUFFIX = .so
endif

DLIBS = $(patsubst %,$(LIBDIR)/lib%$(SUFFIX),$(LIB))
CFLAGS = $(STD) $(WFLAGS) $(OPT) $(INC)
LFLAGS = $(NOSTD) $(OPT) $(LINC) $(OSLIB)

$(TARGET): $(OBJS) $(CRTO) $(LIBS)
	$(CC) $(OBJS) $(CRTO) -o $@ $(LFLAGS)

.PHONY: test shared clean install uninstall

shared: $(OBJS) $(CRTO) $(DLIBS)
	$(CC) $(OBJS) $(CRTO) -o $(TARGET) $(LFLAGS)

test: $(NOMAIN) $(CRTO) $(LIBS)
	$(CC) -c $@.c -o $@.o $(CFLAGS)
	$(CC) $(NOMAIN) $(CRTO) $@.o $(LFLAGS)
	rm $@.o

$(LIBDIR)/lib%.a: %
	cd $^ && $(MAKE) && cp bin/*.a ../$(LIBDIR)

$(LIBDIR)/libutopia$(SUFFIX): utopia $(LIBDIR)/libzlibc$(SUFFIX)
	cd $^ && $(MAKE) shared && cp bin/*$(SUFFIX) ../$(LIBDIR)

$(LIBDIR)/libzlibc$(SUFFIX): zlibc
	cd $^ && $(MAKE) shared && cp bin/*$(SUFFIX) ../$(LIBDIR)

$(TMPDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(LIBS): | $(LIBDIR)

$(DLIBS): | $(LIBDIR)
    
$(OBJS): | $(TMPDIR)

$(CRTO): $(CRT) | $(TMPDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TMPDIR):
	mkdir -p $@

$(LIBDIR):
	mkdir -p $@

clean: $(SCRIPT)
	./$^ $@

install: $(SCRIPT)
	./$^ $@

uninstall: $(SCRIPT)
	./$^ $@
