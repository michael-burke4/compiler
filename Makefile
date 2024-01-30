CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror -Og
DEPS=symbol_table.h ht.h stack.h util.h token.h scan.h parse.h ast.h error.h print.h
OBJ=symbol_table.c ht.o stack.o util.o token.o scan.o parse.o ast.o error.o print.c


main: $(OBJ) main.o
	$(CC) -o $@ $^ $(CFLAGS)

test: $(OBJ) test.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm *.o main test
