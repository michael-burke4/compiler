SRCDIR=src
OBJDIR=obj
BINDIR=bin
TGTDIR=target

CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror -Og `llvm-config --cflags` -Wno-deprecated
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core executionengine mcjit interpreter analysis native bitwriter --system-libs` -std=c99

CSRC=$(wildcard $(SRCDIR)/*.c)
DEPS=$(wildcard $(SRCDIR)/*.h)

OBJ_PRE1=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CSRC))

OBJ_PRE2=$(filter-out $(OBJDIR)/main.o, $(OBJ_PRE1))
OBJ=$(filter-out $(OBJDIR)/test.o, $(OBJ_PRE2))

.PHONY: clean compile dis main test

ifdef SRC
SRC_BASE=$(basename $(notdir $(SRC)))
endif

main: $(OBJDIR) $(BINDIR) $(BINDIR)/main

test: $(OBJDIR) $(BINDIR) $(BINDIR)/test

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

$(TGTDIR):
	-mkdir $@

compile: $(TGTDIR) $(BINDIR)/main
ifdef SRC
	$(BINDIR)/main $(SRC) -o $(OBJDIR)/$(SRC_BASE).bc
	llc --filetype=obj $(OBJDIR)/$(SRC_BASE).bc -o $(OBJDIR)/$(SRC_BASE).o
	$(LD) $(OBJDIR)/$(SRC_BASE).o -o $(TGTDIR)/$(SRC_BASE)
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

dis: $(BINDIR)/main
ifdef SRC
	$(BINDIR)/main $(SRC) -o $(OBJDIR)/$(SRC_BASE).bc
	llvm-dis $(OBJDIR)/$(SRC_BASE).bc -o $(OBJDIR)/$(SRC_BASE).ll
	cat $(OBJDIR)/$(SRC_BASE).ll
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

clean:
	-rm $(OBJDIR)/*
	-rm $(BINDIR)/*
	-rm $(TGTDIR)/*
