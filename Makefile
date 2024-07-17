CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror -Og `llvm-config --cflags` -Wno-deprecated
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core executionengine mcjit interpreter analysis native bitwriter --system-libs` -std=c99
DEPS=typecheck.h symbol_table.h ht.h stack.h util.h token.h scan.h parse.h ast.h error.h print.h codegen.h scope.h
OBJ=typecheck.o symbol_table.o ht.o stack.o util.o token.o scan.o parse.o ast.o error.o print.o codegen.o scope.o

.PHONY: clean compile

main: $(OBJ) main.o
	$(LD) -o $@ $^ $(LDFLAGS)

test: $(OBJ) test.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

compile: main
ifdef SRC
	./main $(SRC)
	llc --filetype=obj $(subst .txt,.bc,$(notdir $(SRC)))
	gcc $(subst .txt,.o,$(notdir $(SRC))) -o $(subst .txt,,$(notdir $(SRC)))
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

dis: main
ifdef SRC
	./main $(SRC)
	llvm-dis $(subst .txt,.bc,$(notdir $(SRC)))
	cat $(subst .txt,.ll,$(notdir $(SRC)))
else
	$(error no SRC supplied. Please specify SRC=srcfile)
endif

clean:
	-rm *.o *.bc *.ll
