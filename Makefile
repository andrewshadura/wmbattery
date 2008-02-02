# Note: this is just a stub Makefile, to make it easy for you to
# run configure and build pdmenu. The full-fledged Makefile is created
# by ./configure, and is called `makeinfo'. You might want to edit it.

include makeinfo

all: wmbattery

clean:
	rm -f wmbattery *.o

distclean: clean
	rm -f config.status config.cache config.log makeinfo config.h configure

install: all
	$(INSTALL) -d $(bindir) $(man1dir) $(icondir)
	$(INSTALL_PROGRAM) wmbattery $(bindir)
	$(INSTALL_DATA) $(srcdir)/wmbattery.1x $(man1dir)/wmbattery.1x
	$(INSTALL_DATA) $(srcdir)/*.xpm $(icondir)

uninstall:
	rm -rf $(bindir)/wmbattery $(man1dir)/wmbattery.1x $(icondir)

OBJS=wmbattery.o acpi.o sonypi.o

ifdef USE_HAL
LIBS+=$(shell pkg-config --libs hal)
OBJS+=simplehal.o
CFLAGS+=-DHAL
simplehal.o: simplehal.c
	$(CC) $(CFLAGS) $(shell pkg-config --cflags hal) -c simplehal.c -o simplehal.o
endif

wmbattery: $(OBJS)
	$(CC) -o wmbattery $(LDFLAGS) $(OBJS) $(LIBS)

wmbattery.o: wmbattery.c wmbattery.h

configure: configure.ac
	autoconf

config.status: configure
	./configure

makeinfo: autoconf/makeinfo.in config.status
	./config.status
