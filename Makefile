SRCDIR=src
OBJDIR=obj
BINDIR=bin
TGTDIR=target
DBGDIR=dbg

CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror -O2 `llvm-config --cflags` -Wno-deprecated
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core executionengine mcjit interpreter analysis native bitwriter --system-libs` -std=c99
DBGFLAGS=-DDEBUG -g

CSRC=$(wildcard $(SRCDIR)/*.c)
DEPS=$(wildcard $(SRCDIR)/*.h)

OBJ=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CSRC))
DBG_OBJ=$(patsubst $(SRCDIR)/%.c,$(DBGDIR)/%.o,$(CSRC))

.PHONY: clean compile dis main valgrind debug

ifdef SRC
SRC_BASE=$(basename $(notdir $(SRC)))
endif

main: $(OBJDIR) $(BINDIR) $(BINDIR)/main

$(BINDIR)/main: $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o $(DBGDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

debug: CFLAGS+=$(DBGFLAGS)
debug: $(DBGDIR) $(DBG_OBJ) $(DBGDIR)/main
$(DBGDIR)/main: $(DBG_OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

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
	-rm $(DBGDIR)/*
