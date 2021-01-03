PREFIX = /usr/local

CFLAGS = -Wall -pedantic
LDLIBS = -lncurses -lpanel

cbonsai: cbonsai.c

.PHONY: install
install: cbonsai
	install -TDm0755 cbonsai $(DESTDIR)$(PREFIX)/bin/cbonsai

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/cbonsai

.PHONY: clean
clean:
	rm -f cbonsai *.o
