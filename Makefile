DAEMONNAME = des-lsr
VERSION_MAJOR = 0
VERSION_MINOR = 1
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)
DESTDIR ?= /usr/sbin
PREFIX ?=

DIR_BIN = $(PREFIX)$(DESTDIR)
DIR_ETC = $(PREFIX)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d
MODULES=des-lsr des-lsr_packethandler des-lsr_routinglogic
UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = *.c *.h Makefile *.conf *.init *.default ChangeLog TODO

FILE_DEFAULT = ./$(DAEMONNAME).default
FILE_ETC = ./$(DAEMONNAME).conf
FILE_INIT = ./$(DAEMONNAME).init

LIBS = dessert dessert-extra m
CFLAGS += -ggdb -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
LDFLAGS += $(addprefix -l,$(LIBS))

all: lsr

clean:
	rm -f *.o *.tar.gz ||  true
	rm -f $(DAEMONNAME) || true
	rm -rf $(DAEMONNAME).dSYM || true

install:
	mkdir -p $(DIR_BIN)
	install -m 744 $(DAEMONNAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 644 $(FILE_ETC) $(DIR_ETC)
	mkdir -p $(DIR_DEFAULT)
	install -m 644 $(FILE_DEFAULT) $(DIR_DEFAULT)/$(DAEMONNAME)
	mkdir -p $(DIR_INIT)
	install -m 755 $(FILE_INIT) $(DIR_INIT)/$(DAEMONNAME)

lsr: $(addsuffix .o,$(MODULES)) des-lsr.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))

android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert 
android: lsr package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir $(DAEMONNAME)-$(VERSION)
	cp -R $(TARFILES) $(DAEMONNAME)-$(VERSION)
	tar -czf $(DAEMONNAME)-$(VERSION).tar.gz $(DAEMONNAME)-$(VERSION)
	rm -rf $(DAEMONNAME)-$(VERSION)

debian: tarball
	cp $(DAEMONNAME)-$(VERSION).tar.gz ../debian/tarballs/$(DAEMONNAME)_$(VERSION).orig.tar.gz
