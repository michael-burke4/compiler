CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror
DEPS=strvec.h token.h
OBJ=main.o strvec.o token.o

main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm *.o main
