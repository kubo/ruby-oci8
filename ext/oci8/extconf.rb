require 'mkmf'
require File.dirname(__FILE__) + '/oraconf'
require File.dirname(__FILE__) + '/apiwrap'

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
$libs += oraconf.libs

oci_actual_client_version = 800
funcs = {}
YAML.load(open(File.dirname(__FILE__) + '/apiwrap.yml')).each do |key, val|
  funcs[val[:version]] ||= []
  funcs[val[:version]] << key
end
funcs.keys.sort.each do |version|
  next if version == 800
  puts "checking for Oracle #{version.to_s.gsub(/(.)(.)$/, '.\1.\2')} API - start"
  result = catch :result do
    funcs[version].sort.each do |func|
      unless have_func(func)
        throw :result, "fail"
      end
    end
    oci_actual_client_version = version
    "pass"
  end
  puts "checking for Oracle #{version.to_s.gsub(/(.)(.)$/, '.\1.\2')} API - #{result}"
  break if result == 'fail'
end
$defs << "-DACTUAL_ORACLE_CLIENT_VERSION=#{oci_actual_client_version}"

if with_config('oracle-version')
  oci_client_version = with_config('oracle-version').to_i
else
  oci_client_version = oci_actual_client_version
end
$defs << "-DORACLE_CLIENT_VERSION=#{oci_client_version}"

if with_config('runtime-check')
  $defs << "-DRUNTIME_API_CHECK=1"
end

$objs = ["oci8lib.o", "env.o", "error.o", "oci8.o",
         "stmt.o", "bind.o", "metadata.o", "attr.o",
         "rowid.o", "lob.o", "oradate.o",
         "ocinumber.o", "ocidatetime.o", "object.o", "apiwrap.o",
         "xmldb.o"]

# Checking gcc or not
if oraconf.cc_is_gcc
  $CFLAGS += " -Wall"
end

have_func("localtime_r")

# ruby 1.8 headers
have_header("intern.h")
have_header("util.h")
# ruby 1.9 headers
have_header("ruby/util.h")

# $! in C API
have_var("ruby_errinfo", "ruby.h") # ruby 1.8
have_func("rb_errinfo", "ruby.h")  # ruby 1.9

# replace files
replace = {
  'OCI8_CLIENT_VERSION' => oraconf.version,
  'OCI8_MODULE_VERSION' => RUBY_OCI8_VERSION
}

# make ruby script before running create_makefile.
replace_keyword(File.dirname(__FILE__) + '/../../lib/oci8.rb.in', '../../lib/oci8.rb', replace)

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

create_makefile("oci8lib")

exit 0
