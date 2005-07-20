VERSION = 0.1.12
RUBY = ruby

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

# internal use only
dist:
	-rm -rf ruby-oci8-$(VERSION)
	mkdir ruby-oci8-$(VERSION)
	tar cf - `cat MANIFEST` | (cd ruby-oci8-$(VERSION); tar xf - )
	tar cfz ruby-oci8-$(VERSION).tar.gz ruby-oci8-$(VERSION)

dist-check: dist
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)"
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)" check
	cd ruby-oci8-$(VERSION)/src && $(MAKE) RUBY="$(RUBY)" sitearchdir=../work sitelibdir=../work site-install
	cd ruby-oci8-$(VERSION)/test && $(RUBY) -I../work -I../support test_all.rb
