require 'mkmf'
require File.dirname(__FILE__) + '/oraconf'

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

oci_major_version = try_constant("OCI_MAJOR_VERSION", "oci.h")
oci_minor_version = try_constant("OCI_MINOR_VERSION", "oci.h")

oci_major_version &&= oci_major_version.to_i
oci_minor_version &&= oci_minor_version.to_i

saved_defs = $defs
if oci_major_version.nil? or oci_minor_version.nil?
  if have_func("OCIPing")
    oci_major_version = 10
    oci_minor_version = 1
  elsif have_func("OCISessionPoolCreate") and
      have_func("OCISessionGet") and
      have_func("OCIEnvNlsCreate") and
      have_func("OCIStmtPrepare2") and
      have_func("OCINlsCharSetNameToId")
    oci_major_version = 9
    oci_minor_version = 2
  elsif have_func("OCIRowidToChar") and
      have_func("OCIFEnvCreate") and
      have_func("OCILogon2") and
      have_func("OCIStmtFetch2") and
      have_func("OCIAnyDataGetType") and
      have_func("OCIConnectionPoolCreate")
    oci_major_version = 9
    oci_minor_version = 0
  elsif have_func("OCIEnvCreate") and
      have_func("OCITerminate") and
      have_func("OCILobOpen") and
      have_func("OCILobClose") and
      have_func("OCILobCreateTemporary") and
      have_func("OCILobGetChunkSize") and
      have_func("OCILobLocatorAssign") and
      have_func("OCIReset")
    oci_major_version = 8
    oci_minor_version = 1
  else
    oci_major_version = 8
    oci_minor_version = 0
  end
end
$defs = saved_defs
puts "checking Oracle client version... #{oci_major_version}.#{oci_minor_version}"

if specified_oracle_version = with_config("oracle-version")
  major, minor, = specified_oracle_version.split('.').map { |v| v.to_i }
  if major > oci_major_version or
      (major == oci_major_version and minor > oci_minor_version)
    raise "the specified Oracle version #{specified_oracle_version} is larger than #{oci_major_version}.#{oci_minor_version}."
  end
  oci_major_version = major
  oci_minor_version = minor
end
puts "build for Oracle client version... #{oci_major_version}.#{oci_minor_version}"

$defs << "-DBUILD_FOR_ORACLE_VERSION_MAJOR=#{oci_major_version}"
$defs << "-DBUILD_FOR_ORACLE_VERSION_MINOR=#{oci_minor_version}"
$defs << "-DRBOCI_VERSION=\"#{RUBY_OCI8_VERSION}\""

$objs = ["oci8lib.o", "env.o", "error.o", "oci8.o",
         "stmt.o", "bind.o", "metadata.o", "attr.o",
         "rowid.o", "lob.o", "oradate.o",
         "ocinumber.o", "ocidatetime.o", "object.o"]
$objs << "xmldb.o" if oci_major_version >= 10

# Checking gcc or not
if oraconf.cc_is_gcc
  $CFLAGS += " -Wall"
end

have_func("localtime_r")

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
  ['instant-client', 'oracle-version'].each do |arg|
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
    f.puts("#{obj}: $(srcdir)/#{obj.sub(/\.o$/, ".c")} $(srcdir)/oci8.h Makefile")
  end
end

create_makefile("oci8lib")

exit 0
