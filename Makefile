# zcc makefile

CC = gcc
SRC = src/*.c
EXE = zcc

STD = -std=c89
OPT = -O2 
WFLAGS = -Wall -Wextra
INC = -I. -Isrc

LDIR = lib
LIB = utopia

LSTATIC = $(patsubst %,lib%.a,$(LIB))
LPATHS = $(patsubst %,$(LDIR)/%,$(LSTATIC))
LFLAGS = $(patsubst %,-L%,$(LDIR))
LFLAGS += $(patsubst %,-l%,$(LIB))

CFLAGS = $(STD) $(OPT) $(WFLAGS) $(INC)

$(EXE): $(LPATHS) $(SRC) $(MAIN)
	$(CC) -o $@ $(SRC) $(MAIN) $(CFLAGS) $(LFLAGS)

$(LPATHS): $(LDIR) $(LSTATIC)
	mv *.a lib/

$(LDIR):
	mkdir $@

$(LDIR)%.a: %
	cd $^ && make && mv $@ ../

exe: $(SRC) $(MAIN)
	$(CC) -o $(EXE) $^ $(CFLAGS) $(LFLAGS)

clean: build.sh
	./$^ $@
