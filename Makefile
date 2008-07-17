VERSION = `cat VERSION`
RUBY = ruby -w

all: build

build: config.save setup

config.save: lib/oci8.rb.in
	$(RUBY) setup.rb config

setup:
	$(RUBY) setup.rb setup

check: build
	$(RUBY) setup.rb test

clean:
	$(RUBY) setup.rb clean

distclean:
	$(RUBY) setup.rb distclean

install:
	$(RUBY) setup.rb install

site-install:
	$(RUBY) setup.rb install

format_c_source:
	astyle --options=none --style=linux --indent=spaces=4 --brackets=linux --suffix=none ext/oci8/*.[ch]

# internal use only
.PHONY: rdoc

rdoc:
	$(RUBY) custom-rdoc.rb -o rdoc -U README ext/oci8 lib

dist:
	-rm -rf ruby-oci8-$(VERSION)
	mkdir ruby-oci8-$(VERSION)
	tar cf - `cat dist-files` | (cd ruby-oci8-$(VERSION); tar xf - )
	tar cfz ruby-oci8-$(VERSION).tar.gz ruby-oci8-$(VERSION)

dist-check: dist
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)"
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)" check
	cd ruby-oci8-$(VERSION)/src && $(MAKE) RUBY="$(RUBY)" sitearchdir=../work sitelibdir=../work site-install
	cd ruby-oci8-$(VERSION)/test && $(RUBY) -I../work -I../support test_all.rb
