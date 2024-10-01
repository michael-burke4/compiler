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

OBJ=$(filter-out $(OBJDIR)/main.o, $(OBJ_PRE1))

.PHONY: clean compile dis main valgrind

ifdef SRC
SRC_BASE=$(basename $(notdir $(SRC)))
endif

main: $(OBJDIR) $(BINDIR) $(BINDIR)/main

$(BINDIR)/main: $(OBJ) $(OBJDIR)/main.o
	$(LD) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR) $(BINDIR) $(TGTDIR):
	-mkdir $@

compile: $(TGTDIR) $(BINDIR)/main
ifdef SRC
	$(BINDIR)/main $(SRC) -o $(OBJDIR)/$(SRC_BASE).bc
	llc --filetype=obj $(OBJDIR)/$(SRC_BASE).bc -o $(OBJDIR)/$(SRC_BASE).o
	$(LD) $(OBJDIR)/$(SRC_BASE).o -o $(TGTDIR)/$(SRC_BASE)
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

valgrind: $(TGTDIR) $(BINDIR)/main
ifdef SRC
ifdef LCF # annoying, but if you want full leak check + list, set LCF to something like LCF=1 when calling make valgrind
	valgrind --leak-check=full --show-leak-kinds=all $(BINDIR)/main $(SRC) -o $(OBJDIR)/$(SRC_BASE).bc
else
	valgrind $(BINDIR)/main $(SRC) -o $(OBJDIR)/$(SRC_BASE).bc
endif
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
