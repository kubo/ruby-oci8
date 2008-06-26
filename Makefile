VERSION = `cat VERSION`
RUBY = ruby
GEM = gem
CONFIG_OPT = 

all: build

build: config.save setup

config.save: lib/oci8.rb.in
	$(RUBY) setup.rb config $(CONFIG_OPT)

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

gem:
	$(GEM) build ruby-oci8.gemspec

binary_gem: build
	$(GEM) build ruby-oci8.gemspec -- current

# internal use only
dist: ruby-oci8.spec
	-rm -rf ruby-oci8-$(VERSION)
	mkdir ruby-oci8-$(VERSION)
	tar cf - `cat dist-files` | (cd ruby-oci8-$(VERSION); tar xf - )
	tar cfz ruby-oci8-$(VERSION).tar.gz ruby-oci8-$(VERSION)

dist-check: dist
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)"
	cd ruby-oci8-$(VERSION) && $(MAKE) RUBY="$(RUBY)" check
	cd ruby-oci8-$(VERSION)/src && $(MAKE) RUBY="$(RUBY)" sitearchdir=../work sitelibdir=../work site-install
	cd ruby-oci8-$(VERSION)/test && $(RUBY) -I../work -I../support test_all.rb

ruby-oci8.spec: ruby-oci8.spec.in
	$(RUBY) -rerb -e "open('ruby-oci8.spec', 'w').write(ERB.new(open('ruby-oci8.spec.in').read).result)"
