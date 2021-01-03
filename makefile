.POSIX:
CC      = cc
CFLAGS  = -Wall -Wextra -Wpedantic -Os
LDFLAGS =
LDLIBS  = -lncurses -lpanel
PREFIX  = $(HOME)/.local

cbonsai: cbonsai.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ cbonsai.c $(LDLIBS)

clean:
	rm -f cbonsai

install: cbonsai
	@mkdir -p $(DESTDIR)$(PREFIX)/bin/
	install -m 755 cbonsai $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/cbonsai
