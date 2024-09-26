SRCDIR=src
OBJDIR=obj
BINDIR=bin

CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror -Og `llvm-config --cflags` -Wno-deprecated
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core executionengine mcjit interpreter analysis native bitwriter --system-libs` -std=c99

CSRC=$(wildcard $(SRCDIR)/*.c)
DEPS=$(wildcard $(SRCDIR)/*.h)

OBJ_PRE1=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CSRC))

OBJ_PRE2=$(filter-out $(OBJDIR)/main.o, $(OBJ_PRE1))
OBJ=$(filter-out $(OBJDIR)/test.o, $(OBJ_PRE2))

.PHONY: clean compile dis

ifdef SRC
SRC_BASE=$(basename $(notdir $(SRC)))
endif

$(BINDIR)/main: $(OBJ) $(OBJDIR)/main.o
	$(LD) -o $@ $^ $(LDFLAGS)

$(BINDIR)/test: $(OBJ) $(OBJDIR)/test.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR):
	-mkdir $@
$(BINDIR):
	-mkdir $@

compile: $(BINDIR)/main
ifdef SRC
	$(BINDIR)/main $(SRC)
	llc --filetype=obj $(SRC_BASE).bc
	$(LD) $(SRC_BASE).o -o $(BINDIR)/$(SRC_BASE)
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

dis: $(BINDIR)/main
ifdef SRC
	$(BINDIR)/main $(SRC)
	llvm-dis $(basename $(SRC)).bc
	cat $(basename $(SRC)).ll
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

clean:
	-rm $(OBJDIR)/*
