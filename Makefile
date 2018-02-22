CPPFLAGS=-Wall -O2 -msse2

all: magpie amtx

magpie: magpie.c

amtx: amtx.c

.PHONY: clean

clean:
	rm -f amtx
	rm -f magpie
