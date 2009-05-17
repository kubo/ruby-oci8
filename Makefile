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

#
# for Windows
#
RUBY_18 = c:\ruby
RUBY_191 = c:\ruby-1.9.1
GEMPKG = ruby-oci8-2.0.2-x86-mswin32-60.gem

ext\oci8\oci8lib_18.so:
	$(RUBY_18)\bin\ruby -r fileutils -e "FileUtils.rm_rf('ruby18')"
	md ruby18
	cd ruby18
	$(RUBY_18)\bin\ruby ..\setup.rb config -- --with-runtime-check
	$(RUBY_18)\bin\ruby ..\setup.rb setup
	rem $(RUBY_18)\bin\ruby ..\setup.rb test
	cd ..
	copy ruby18\ext\oci8\oci8lib_18.so ext\oci8\oci8lib_18.so

ext\oci8\oci8lib_191.so:
	$(RUBY_191)\bin\ruby -r fileutils -e "FileUtils.rm_rf('ruby191')"
	md ruby191
	cd ruby191
	$(RUBY_191)\bin\ruby ..\setup.rb config -- --with-runtime-check
	$(RUBY_191)\bin\ruby ..\setup.rb setup
	rem $(RUBY_191)\bin\ruby ..\setup.rb test
	cd ..
	copy ruby191\ext\oci8\oci8lib_191.so ext\oci8\oci8lib_191.so
	copy ruby191\lib\oci8.rb lib\oci8.rb

$(GEMPKG): ext\oci8\oci8lib_18.so ext\oci8\oci8lib_191.so ruby-oci8.gemspec
	$(RUBY_191)\bin\gem build ruby-oci8.gemspec -- current

test-win32-ruby18: $(GEMPKG)
	$(RUBY_18)\bin\gem install $(GEMPKG) --no-rdoc --no-ri --local
	$(RUBY_18)\bin\ruby -rubygems test\test_all.rb

test-win32-ruby191: $(GEMPKG)
	$(RUBY_191)\bin\gem install $(GEMPKG) --no-rdoc --no-ri --local
	$(RUBY_191)\bin\ruby test\test_all.rb

test-win32: test-win32-ruby18 test-win32-ruby191
