# zcc makefile

CC = gcc
SRC = src/*.c
MAIN = main.c
TEST = test.c
EXE = zcc
TEXE = ztest

STD = -std=c89 -nostdlib -nostartfiles -fno-stack-protector
OPT = -O2 
WFLAGS = -Wall -Wextra
INC = -I. -Isrc -Izlibc/src/include -Iutopia

LDIR = lib
LIB = zlibc utopia

LSTATIC = $(patsubst %,lib%.a,$(LIB))
LPATHS = $(patsubst %,$(LDIR)/%,$(LSTATIC))
LFLAGS = $(patsubst %,-L%,$(LDIR))
LFLAGS += $(patsubst %,-l%,$(LIB))

OS = $(shell uname -s)
ifeq ($(OS),Darwin)
    OSFLAGS=-e _start -lSystem
else
    OSFLAGS=-e start -lgcc -lc
endif

CFLAGS = $(STD) $(OPT) $(WFLAGS) $(INC) $(LFLAGS) $(OSFLAGS)

$(EXE): $(LPATHS) $(SRC) $(MAIN)
	$(CC) -o $@ $(SRC) $(MAIN) $(CFLAGS)

$(LPATHS): $(LDIR) $(LSTATIC)
	mv *.a lib/

$(LDIR):
	mkdir -p $@

$(LDIR)%.a: %
	cd $^ && $(MAKE) && mv bin/$@ ../

exe: $(SRC) $(MAIN)
	$(CC) -o $(EXE) $^ $(CFLAGS)

test: $(SRC) $(TEST)
	$(CC) -o $(TEXE) $^ $(CFLAGS)

clean: build.sh
	./$^ $@
