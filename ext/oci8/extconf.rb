if RUBY_VERSION < "1.9"
  puts "Ruby-oci8 doesn't work ruby 1.8 since ruby-oci8 2.2.0."
  exit 1
end

begin
  require 'mkmf'
rescue LoadError
  if /linux/ =~ RUBY_PLATFORM
    raise <<EOS
You need to install a ruby development package ruby-devel, ruby-dev or so.
EOS
  end
  raise
end

require File.dirname(__FILE__) + '/oraconf'
require File.dirname(__FILE__) + '/apiwrap'
require File.dirname(__FILE__) + '/../../lib/oci8/oracle_version.rb'
require File.dirname(__FILE__) + '/../../lib/oci8/version.rb'

oraconf = OraConf.get()

$CFLAGS += oraconf.cflags
saved_libs = $libs
$libs += oraconf.libs

saved_defs = $defs.clone
fmt = "%s"
def fmt.%(x)
  x ? super : "failed"
end
oci_major_version = checking_for 'OCI_MAJOR_VERSION in oci.h', fmt do
  try_constant('OCI_MAJOR_VERSION', 'oci.h')
end
if oci_major_version
  oci_minor_version = checking_for 'OCI_MINOR_VERSION in oci.h', fmt do
    try_constant('OCI_MINOR_VERSION', 'oci.h')
  end
else
  if have_func('OCILobGetLength2')
    oci_major_version = 10
    oci_minor_version = 1
  elsif have_func('OCIStmtPrepare2')
    raise "Ruby-oci8 #{OCI8::VERSION} doesn't support Oracle 9iR2. Use ruby-oci8 2.1.x instead."
  elsif have_func('OCILogon2')
    raise "Ruby-oci8 #{OCI8::VERSION} doesn't support Oracle 9iR1. Use ruby-oci8 2.1.x instead."
  elsif have_func('OCIEnvCreate')
    raise "Ruby-oci8 #{OCI8::VERSION} doesn't support Oracle 8i. Use ruby-oci8 2.0.x instead."
  else
    raise "Ruby-oci8 #{OCI8::VERSION} doesn't support Oracle 8. Use ruby-oci8 2.0.x instead."
  end
end
$defs = saved_defs

if with_config('oracle-version')
  oraver = OCI8::OracleVersion.new(with_config('oracle-version'))
else
  oraver = OCI8::OracleVersion.new(oci_major_version, oci_minor_version)
end
$defs << "-DORACLE_CLIENT_VERSION=#{format('0x%08x', oraver.to_i)}"

if with_config('runtime-check')
  $defs << "-DRUNTIME_API_CHECK=1"
  $libs = saved_libs
end

$objs = ["oci8lib.o", "env.o", "error.o", "oci8.o", "ocihandle.o",
         "connection_pool.o",
         "stmt.o", "bind.o", "metadata.o", "attr.o",
         "lob.o", "oradate.o",
         "ocinumber.o", "ocidatetime.o", "object.o", "apiwrap.o",
         "encoding.o", "oranumber_util.o", "thread_util.o", "util.o"]

if RUBY_PLATFORM =~ /mswin32|cygwin|mingw/
  $defs << "-DUSE_WIN32_C"
  $objs << "win32.o"
end

# Checking gcc or not
if oraconf.cc_is_gcc
  $CFLAGS += " -Wall"
end

have_func("localtime_r")
have_func("dladdr")
have_func("dlmodinfo")
have_func("dlgetname")

# ruby 2.0.0 headers
have_header("ruby/thread.h")

have_func("rb_class_superclass", "ruby.h")
have_func("rb_thread_call_without_gvl", "ruby/thread.h")
have_func("rb_sym2str", "ruby.h")
if (defined? RUBY_ENGINE) && RUBY_ENGINE == 'rbx'
  have_func("rb_str_buf_cat_ascii", "ruby.h")
  have_func("rb_enc_str_buf_cat", "ruby.h")
end

ruby_engine = (defined? RUBY_ENGINE) ? RUBY_ENGINE : 'ruby'

so_basename = 'oci8lib_'
case ruby_engine
when 'ruby'
  # The extension library name includes the ruby ABI (application binary interface)
  # version. It is for binary gems which contain more than one extension library
  # and use correct one depending on the ABI version.
  #
  #  ruby version |  ABI version
  # --------------+--------------
  #    1.8.6      |   1.8
  #    1.8.7      |   1.8
  # --------------+--------------
  #    1.9.0      |   1.9.0
  #    1.9.1      |   1.9.1
  #    1.9.2      |   1.9.1
  #    1.9.3      |   1.9.1
  # --------------+--------------
  #    2.0.0      |   2.0.0
  # --------------+--------------
  #    2.1.0      |   2.1.0
  #     ...       |    ...
  #
  # The ABI version of ruby 2.1.1 will be 2.1.0.
  # See "ABI Compatibility" in <URL:http://www.ruby-lang.org/en/news/2013/12/21/semantic-versioning-after-2-1-0/>.
  #
  case RUBY_VERSION
  when /^1\.9\.0/
    raise 'unsupported ruby version: 1.9.0'
  when /^1\.9/
    so_basename += '191'
  when /^1\.8/
    so_basename += '18'
  else
    so_basename += RUBY_VERSION.gsub(/(\d+)\.(\d+).(.*)/, '\1\20')
  end
when 'rbx'
  so_basename += 'rbx'
when 'jruby'
  raise "Ruby-oci8 doesn't support jruby because its C extension support is in development in jruby 1.6 and deprecated in jruby 1.7."
when 'truffleruby'
  so_basename += 'truffleruby'
else
  raise 'unsupported ruby engine: ' + RUBY_ENGINE
end

print "checking for plthook... "
STDOUT.flush
case RUBY_PLATFORM
when /mswin32|cygwin|mingw/
  plthook_src = "plthook_win32.c"
when /darwin/
  plthook_src = "plthook_osx.c"
else
  plthook_src = "plthook_elf.c"
end
FileUtils.copy(File.dirname(__FILE__) + "/" + plthook_src, CONFTEST_C)
if xsystem(cc_command(""))
  FileUtils.rm_f("#{CONFTEST}.#{$OBJEXT}")
  puts plthook_src
  $objs << plthook_src.gsub(/\.c$/, '.o')
  $objs << "hook_funcs.o"
  $defs << "-DHAVE_PLTHOOK"
  have_library('dbghelp', 'ImageDirectoryEntryToData', ['windows.h', 'dbghelp.h']) if RUBY_PLATFORM =~ /cygwin/
  $libs += ' -lws2_32' if RUBY_PLATFORM =~ /cygwin/
else
  puts "no"
end

$defs << "-DInit_oci8lib=Init_#{so_basename}"
$defs << "-Doci8lib=#{so_basename}"
$defs << "-DOCI8LIB_VERSION=\\\"#{OCI8::VERSION}\\\""
$defs << "-DCHAR_IS_NOT_A_SHORTCUT_TO_ID" if ruby_engine != 'ruby'

create_header()

# make dependency file
open("depend", "w") do |f|
  extconf_opt = ''
  ['oracle-version', 'runtime-check'].each do |arg|
    opt = with_config(arg)
    case opt
    when String
      extconf_opt += " --with-#{arg}=#{opt}"
    when true
      extconf_opt += " --with-#{arg}=yes"
    when false
      extconf_opt += " --with-#{arg}=no"
    end
  end
  f.puts("Makefile: $(srcdir)/extconf.rb $(srcdir)/oraconf.rb")
  f.puts("\t$(RUBY) $(srcdir)/extconf.rb#{extconf_opt}")
  $objs.each do |obj|
    f.puts("#{obj}: $(srcdir)/#{obj.sub(/\.o$/, ".c")} $(srcdir)/oci8.h apiwrap.h Makefile")
  end
  f.puts("apiwrap.c apiwrap.h: $(srcdir)/apiwrap.c.tmpl $(srcdir)/apiwrap.h.tmpl $(srcdir)/apiwrap.yml $(srcdir)/apiwrap.rb")
  f.puts("\t$(RUBY) $(srcdir)/apiwrap.rb")
end

create_apiwrap()

case RUBY_PLATFORM
when /mingw32/
  # Drop '-s' option from LDSHARED and explicitly run 'strip' to get the map file.
  if RbConfig::MAKEFILE_CONFIG["LDSHARED"].gsub!(/-s\b/, '')
    alias :oci8_configuration_orig :configuration
    def configuration(*args)
      prefix = [nil].pack('P').size == 4 ? '_' : ''
      oci8_configuration_orig(*args) << <<EOS

# Dirty hack to get the map file.
all__: all
	nm $(DLLIB) | grep ' [TtBb] #{prefix}[A-Za-z]' > $(TARGET).map
	strip -s $(DLLIB)
EOS
    end
  end
end

create_makefile(so_basename)

exit 0
