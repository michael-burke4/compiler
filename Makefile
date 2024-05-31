CC=clang
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -Werror -Og `llvm-config --cflags` -Wno-deprecated
LD=clang
LDFLAGS=`llvm-config --cxxflags --ldflags --libs core executionengine mcjit interpreter analysis native bitwriter --system-libs` -std=c99
DEPS=typecheck.h symbol_table.h ht.h stack.h util.h token.h scan.h parse.h ast.h error.h print.h codegen.h
OBJ=typecheck.o symbol_table.c ht.o stack.o util.o token.o scan.o parse.o ast.o error.o print.o codegen.o

.PHONY: clean

main: $(OBJ) main.o
	$(LD) -o $@ $^ $(LDFLAGS)

test: $(OBJ) test.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	-rm *.o *.bc *.ll main test
