CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror
DEPS=strvec.h token.h scan.h parse.h ast.h
OBJ=main.o strvec.o token.o scan.o parse.o ast.o

main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm *.o main
