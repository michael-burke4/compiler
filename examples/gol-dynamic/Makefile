COMPILERDIR=../..
CC=$(COMPILERDIR)/bin/main
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

%.bc: %.ptxt $(CC)
	$(CC) $< -o $@

%.ptxt: %.txt $(DEPS)
	$(PP) $< -o $@

$(CC):
	make --directory=$(COMPILERDIR)

clean:
	-rm *.o *.ptxt gol
