SRCDIR=src
TESTDIR=tests

OBJDIR=obj
BINDIR=bin
TGTDIR=target
DBGDIR=dbg
COVDIR=cov

CC=gcc
CFLAGS=-std=c99 -c -Wall -Wextra -Wpedantic `llvm-config --cflags`
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core analysis native bitwriter --system-libs` -std=c99
MAINFLAGS=-O2
DBGFLAGS=-DDEBUG -Og
COVCFLAGS=-fprofile-arcs -ftest-coverage
COVLDFLAGS= -lgcov --coverage

CSRC=$(wildcard $(SRCDIR)/*.c)
DEPS=$(wildcard $(SRCDIR)/*.h)

OBJ=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CSRC))
DBG_OBJ=$(patsubst $(SRCDIR)/%.c,$(DBGDIR)/%.o,$(CSRC))

.PHONY: clean compile dis main valgrind debug test

ifdef SRC
SRC_BASE=$(basename $(notdir $(SRC)))
endif

main: CFLAGS+=$(MAINFLAGS)
main: $(OBJDIR) $(BINDIR) $(BINDIR)/main

$(BINDIR)/main: $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(DBGDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

debug: CFLAGS+=$(DBGFLAGS) $(COVCFLAGS)
debug: LDFLAGS+=$(COVLDFLAGS)
debug: $(DBGDIR) $(DBG_OBJ) $(DBGDIR)/main
$(DBGDIR)/main: $(DBG_OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

$(OBJDIR) $(BINDIR) $(TGTDIR) $(COVDIR):
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

test: clean debug
test: $(COVDIR)
	$(SRCDIR)/test.py $(DBGDIR)/main $(TESTDIR)
	gcov -f -b $(DBGDIR)/*.gcda > /dev/null
	mv *.gcov $(DBGDIR)/
	lcov -c -d $(DBGDIR) -o $(DBGDIR)/cov.info > /dev/null 2> /dev/null
	genhtml -o $(COVDIR)/ $(DBGDIR)/cov.info > /dev/null

clean:
	-rm $(OBJDIR)/*
	-rm $(BINDIR)/*
	-rm $(TGTDIR)/*
	-rm $(DBGDIR)/*
	-rm -rf $(COVDIR)/*
