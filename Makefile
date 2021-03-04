.POSIX:
CC	= cc
CFLAGS	+= -D_POSIX_C_SOURCE=200809L -std=c18 -Wextra -Wall -Wshadow -Wpointer-arith -Wcast-qual \
           -Wstrict-prototypes -Wmissing-prototypes -pedantic
LDLIBS	= $(shell pkg-config --libs ncurses panel)
PREFIX	= /usr/local

cbonsai: cbonsai.c cbonsai.h

install: cbonsai
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 cbonsai $(DESTDIR)$(PREFIX)/bin/cbonsai

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/cbonsai

clean:
	rm -f cbonsai

.PHONY: install uninstall clean
