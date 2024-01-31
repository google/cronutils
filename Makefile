# Copyright 2010 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

all: runalarm runstat runlock

runalarm: runalarm.c subprocess.c

runlock: runlock.c subprocess.c tempdir.c

runstat: runstat.c subprocess.c tempdir.c

CFLAGS+=-Wall -Werror -Wextra -D_XOPEN_SOURCE=500 -g -ansi -pedantic-errors -Wwrite-strings -Wcast-align -Wcast-qual -Winit-self -Wformat=2 -Wuninitialized -Wmissing-declarations -Wpointer-arith -Wstrict-aliasing -fstrict-aliasing

SOURCES = runalarm.c runlock.c runstat.c subprocess.c subprocess.h tempdir.c tempdir.h Makefile runalarm.1 runlock.1 runstat.1 version examples cronutils.spec runcron regtest.sh tests

prefix = usr/local
BINDIR = $(prefix)/bin
MANDIR = $(prefix)/share/man/man1
VERSION = $(shell cat version)

install:
	mkdir -p -m 755 $(DESTDIR)/$(BINDIR) $(DESTDIR)/$(MANDIR)
	install -m 755 runalarm runlock runstat $(DESTDIR)/$(BINDIR)
	install -m 644 runalarm.1 runlock.1 runstat.1 $(DESTDIR)/$(MANDIR)

clean:
	rm -f runalarm runlock runstat

distclean: clean
	rm -f *~ \#*

dist:
	rm -rf cronutils-$(VERSION) cronutils-$(VERSION).tar cronutils-$(VERSION).tar.gz
	mkdir cronutils-$(VERSION)
	cp -r $(SOURCES) cronutils-$(VERSION)
	tar cf cronutils-$(VERSION).tar cronutils-$(VERSION)
	gzip -9 cronutils-$(VERSION).tar
	rm -rf cronutils-$(VERSION)

test: all
	./regtest.sh

.PHONY: dist clean install distclean test
