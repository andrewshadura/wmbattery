ICONDIR = /usr/share/icons/wmbattery
BINDIR  = /usr/X11R6/bin
MANDIR  = /usr/X11R6/man/man1
LIBDIR  = -L/usr/X11R6/lib
LIBS    = -lXpm -lXext
CFLAGS  = -g -DICONDIR=\"$(ICONDIR)\" -O2 -Wall

wmbattery: wmbattery.c wmbattery.h
	$(CC) $(CFLAGS) $(LIBDIR) $(LIBS) wmbattery.c -o wmbattery

clean:
	rm -f wmbattery

install: wmbattery
	install -d $(PREFIX)/$(ICONDIR) $(PREFIX)/$(BINDIR) $(PREFIX)/$(MANDIR)
	install -s wmbattery $(PREFIX)/$(BINDIR)
	install -m 644 wmbattery.1 $(PREFIX)/$(MANDIR)
	install -m 644 *.xpm $(PREFIX)/$(ICONDIR)

test: wmbattery
	./wmbattery
