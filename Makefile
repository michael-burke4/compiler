CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror
DEPS=strvec.h token.h scan.h parse.h ast.h error.h
OBJ=strvec.o token.o scan.o parse.o ast.o error.o


main: $(OBJ) main.o
	$(CC) -o $@ $^ $(CFLAGS)

test: $(OBJ) test.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm *.o main test
