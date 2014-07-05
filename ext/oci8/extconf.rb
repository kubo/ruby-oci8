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

RUBY_OCI8_VERSION = File.read("#{File.dirname(__FILE__)}/../../VERSION").chomp

oraconf = OraConf.get()

def replace_keyword(source, target, replace)
  puts "creating #{target} from #{source}"
  open(source, "rb") { |f|
    buf = f.read
    replace.each do |key, value|
      buf.gsub!('@@' + key + '@@', value)
    end
    open(target, "wb") {|fw|
      fw.write buf
    }
  }        
end

$CFLAGS += oraconf.cflags
saved_libs = $libs
$libs += oraconf.libs

oci_actual_client_version = 0x08000000
funcs = {}
YAML.load(open(File.dirname(__FILE__) + '/apiwrap.yml')).each do |key, val|
  key = key[0..-4] if key[-3..-1] == '_nb'
  ver = val[:version]
  ver_major = (ver / 100)
  ver_minor = (ver / 10) % 10
  ver_update = ver % 10
  ver = ((ver_major << 24) | (ver_minor << 20) | (ver_update << 12))
  funcs[ver] ||= []
  funcs[ver] << key
end
funcs.keys.sort.each do |version|
  next if version == 0x08000000
  verstr = format('%d.%d.%d', ((version >> 24) & 0xFF), ((version >> 20) & 0xF), ((version >> 12) & 0xFF))
  puts "checking for Oracle #{verstr} API - start"
  result = catch :result do
    funcs[version].sort.each do |func|
      unless have_func(func)
        throw :result, "fail"
      end
    end
    oci_actual_client_version = version
    "pass"
  end
  puts "checking for Oracle #{verstr} API - #{result}"
  break if result == 'fail'
end

have_type('oratext', 'ociap.h')
have_type('OCIDateTime*', 'ociap.h')
have_type('OCIInterval*', 'ociap.h')
have_type('OCICallbackLobRead2', 'ociap.h')
have_type('OCICallbackLobWrite2', 'ociap.h')
have_type('OCIAdmin*', 'ociap.h')
have_type('OCIAuthInfo*', 'ociap.h')
have_type('OCIMsg*', 'ociap.h')
have_type('OCICPool*', 'ociap.h')

if with_config('oracle-version')
  oci_client_version = OCI8::OracleVersion.new(with_config('oracle-version')).to_i
else
  oci_client_version = oci_actual_client_version
end
$defs << "-DORACLE_CLIENT_VERSION=#{format('0x%08x', oci_client_version)}"

if with_config('runtime-check')
  $defs << "-DRUNTIME_API_CHECK=1"
  $libs = saved_libs
end

$objs = ["oci8lib.o", "env.o", "error.o", "oci8.o", "ocihandle.o",
         "connection_pool.o",
         "stmt.o", "bind.o", "metadata.o", "attr.o",
         "lob.o", "oradate.o",
         "ocinumber.o", "ocidatetime.o", "object.o", "apiwrap.o",
         "encoding.o", "oranumber_util.o", "thread_util.o"]

if RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/
  $defs << "-DUSE_WIN32_C"
  $objs << "win32.o"
end

# Checking gcc or not
if oraconf.cc_is_gcc
  $CFLAGS += " -Wall"
end

have_func("localtime_r")
have_func("dladdr")

# ruby 1.8 headers
have_header("intern.h")
have_header("util.h")
# ruby 1.9.1 headers
have_header("ruby/util.h")
have_type('rb_encoding', ['ruby/ruby.h', 'ruby/encoding.h'])
# ruby 2.0.0 headers
have_header("ruby/thread.h")

# $! in C API
have_var("ruby_errinfo", "ruby.h") # ruby 1.8
have_func("rb_errinfo", "ruby.h")  # ruby 1.9

have_func("rb_set_end_proc", "ruby.h")
have_func("rb_class_superclass", "ruby.h")
have_func("rb_thread_blocking_region", "ruby.h")
have_func("rb_thread_call_without_gvl", "ruby/thread.h")
if (defined? RUBY_ENGINE) && RUBY_ENGINE == 'rbx'
  have_func("rb_str_buf_cat_ascii", "ruby.h")
  have_func("rb_enc_str_buf_cat", "ruby.h")
end

# replace files
replace = {
  'OCI8_CLIENT_VERSION' => oraconf.version,
  'OCI8_MODULE_VERSION' => RUBY_OCI8_VERSION
}

# make ruby script before running create_makefile.
replace_keyword(File.dirname(__FILE__) + '/../../lib/oci8.rb.in', '../../lib/oci8.rb', replace)

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
else
  raise 'unsupported ruby engine: ' + RUBY_ENGINE
end

case RUBY_PLATFORM
when /mswin32|cygwin|mingw32|bccwin32/
  plthook_src = "plthook_win32.c"
when /darwin/
  plthook_src = "plthook_osx.c"
else
  plthook_src = "plthook_elf.c"
end

print "checking for plt_hook... "
STDOUT.flush
if xsystem(cc_command("").gsub(CONFTEST_C, plthook_src))
  puts "yes"
  $objs << plthook_src.gsub(/\.c$/, '.o')
  $objs << "hook_funcs.o"
  $defs << "-DHAVE_PLT_HOOK"
else
  puts "no"
end

$defs << "-DInit_oci8lib=Init_#{so_basename}"
$defs << "-Doci8lib=#{so_basename}"
$defs << "-DOCI8LIB_VERSION=\\\"#{RUBY_OCI8_VERSION}\\\""
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
