# zcc makefile

CC = gcc
SRC = src/*.c
MAIN = main.c
TEST = test.c
EXE = zcc
TEXE = ztest

STD = -std=c89 -nostdlib -nostartfiles -fno-stack-protector -e _start
OPT = -O2 
WFLAGS = -Wall -Wextra
INC = -I. -Isrc -Izlibc/src/include

LDIR = lib
LIB = zlibc utopia

LSTATIC = $(patsubst %,lib%.a,$(LIB))
LPATHS = $(patsubst %,$(LDIR)/%,$(LSTATIC))
LFLAGS = $(patsubst %,-L%,$(LDIR))
LFLAGS += $(patsubst %,-l%,$(LIB))

OS = $(shell uname -s)
ifeq ($(OS),Darwin)
	OSFLAGS=-lSystem
endif

CFLAGS = $(STD) $(OPT) $(WFLAGS) $(INC) $(LFLAGS) $(OSFLAGS)

$(EXE): $(LPATHS) $(SRC) $(MAIN)
	$(CC) -o $@ $(SRC) $(MAIN) $(CFLAGS)

$(LPATHS): $(LDIR) $(LSTATIC)
	mv *.a lib/

$(LDIR):
	mkdir $@

$(LDIR)%.a: %
	cd $^ && make && mv $@ ../

exe: $(SRC) $(MAIN)
	$(CC) -o $(EXE) $^ $(CFLAGS)

test: $(SRC) $(TEST)
	$(CC) -o $(TEXE) $^ $(CFLAGS)

clean: build.sh
	./$^ $@
