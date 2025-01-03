CC=../../bin/main
PP=./preprocess.py
LC=llc
LD=clang

.PHONY: clean

SRC=$(wildcard *.txt)
DEPS=$(wildcard *.head)


gol: $(subst .txt,.o,$(SRC))
	$(LD) $^ -o $@

%.o: %.bc
	$(LC) --filetype=obj $< -o $@

%.bc: %.ptxt
	$(CC) $< -o $@

%.ptxt: %.txt $(DEPS)
	$(PP) $< -o $@

clean:
	-rm *.o *.ptxt gol
