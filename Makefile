ICONDIR = /usr/share/icons/wmbattery
BINDIR  = /usr/X11R6/bin
MANDIR  = /usr/X11R6/man/man1
INCDIR  = -I/usr/X11R6/include
LIBDIR  = -L/usr/X11R6/lib
LIBS    = -lXpm -lXext
CFLAGS  = $(INCDIR) -g -DICONDIR=\"$(ICONDIR)\" -O2 -Wall

wmbattery: wmbattery.c wmbattery.h
	$(CC) $(CFLAGS) $(LIBDIR) $(LIBS) wmbattery.c -o wmbattery

clean:
	rm -f wmbattery

install: wmbattery
	install -c -d $(PREFIX)$(ICONDIR) $(PREFIX)$(BINDIR) $(PREFIX)$(MANDIR)
	install -c -s wmbattery $(PREFIX)$(BINDIR)
	install -c -m 644 wmbattery.1x $(PREFIX)$(MANDIR)/wmbattery.1
	install -c -m 644 *.xpm $(PREFIX)$(ICONDIR)

test: wmbattery
	./wmbattery
