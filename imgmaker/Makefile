# SPDX-License-Identifier: 0BSD
# Author: Lasse Collin

CC = cc
CFLAGS = -g -m32
LDFLAGS = -llzma -lssl -lcrypto -static

PROGS = mpcimg2

all: $(PROGS)

.c:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
clean:
	-rm -f $(PROGS)
