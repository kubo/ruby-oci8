#   --*- ruby -*--
# This is based on yoshidam's oracle.rb.
#
# sample one liner:
#  ruby -r oci8 -e 'OCI8.new("scott", "tiger", nil).exec("select * from emp") do |r| puts r.join(","); end'
#  # select all data from emp and print them as CVS format.

ENV['ORA_SDTZ'] = ENV['TZ'] unless ENV['ORA_SDTZ']

if RUBY_PLATFORM =~ /cygwin/
  # Cygwin manages environment variables by itself.
  # They don't synchroize with Win32's ones.
  # This set some Oracle's environment variables to win32's enviroment.
  require 'fiddle'
  win32setenv = Fiddle::Function.new( Fiddle.dlopen('Kernel32.dll')['SetEnvironmentVariableA'],
                                        [Fiddle::TYPE_VOIDP,Fiddle::TYPE_VOIDP],
                                        Fiddle::TYPE_INT )

  ['NLS_LANG', 'TNS_ADMIN', 'LOCAL'].each do |name|
    val = ENV[name]
    win32setenv.call(name, val && val.dup)
  end
  ENV.each do |name, val|
    win32setenv.call(name, val && val.dup) if name =~ /^ORA/
  end
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

dll_dir = nil
begin
  require 'ruby_installer/runtime' # RubyInstaller2 for Windows

  dll_arch = proc do |file|
    begin
      File.open(file, 'rb') do |f|
        if f.read(2) == 'MZ' # IMAGE_DOS_HEADER.e_magic
          f.seek(60, IO::SEEK_SET)
          pe_offset = f.read(4).unpack('V')[0] # IMAGE_DOS_HEADER.e_lfanew
          f.seek(pe_offset)
          if f.read(4) == "PE\000\000" # IMAGE_NT_HEADERS.Signature
            case f.read(2).unpack('v')[0] # IMAGE_FILE_HEADER.Machine
            when 0x014c # IMAGE_FILE_MACHINE_I386
              :x86
            when 0x8664 # IMAGE_FILE_MACHINE_AMD64
              :x64
            end
          end
        end
      end
    rescue
      nil
    end
  end

  ruby_arch = [nil].pack('P').size == 8 ? :x64 : :x86
  ENV['PATH'].split(File::PATH_SEPARATOR).each do |path|
    if !path.empty? && dll_arch.call(File.join(path, 'OCI.DLL')) == ruby_arch
      dll_dir = RubyInstaller::Runtime.add_dll_directory(path)
      break
    end
  end
rescue LoadError
end

begin
  require so_basename
rescue LoadError
  require 'oci8/check_load_error'
  OCI8::Util::check_load_error($!)
  raise
ensure
  dll_dir.remove if dll_dir
end

require 'oci8/version.rb'
if OCI8::VERSION != OCI8::LIB_VERSION
  require 'rbconfig'
  so_name = so_basename + "." + RbConfig::CONFIG['DLEXT']
  raise "VERSION MISMATCH! #{so_name} version is #{OCI8::LIB_VERSION}, but oci8.rb version is #{OCI8::VERSION}."
end

require 'oci8/encoding-init.rb'
require 'oci8/oracle_version.rb'

class OCI8
  # @private
  ORAVER_8_0  = OCI8::OracleVersion.new(8, 0)
  # @private
  ORAVER_8_1  = OCI8::OracleVersion.new(8, 1)
  # @private
  ORAVER_9_0  = OCI8::OracleVersion.new(9, 0)
  # @private
  ORAVER_9_2  = OCI8::OracleVersion.new(9, 2)
  # @private
  ORAVER_10_1 = OCI8::OracleVersion.new(10, 1)
  # @private
  ORAVER_10_2 = OCI8::OracleVersion.new(10, 2)
  # @private
  ORAVER_11_1 = OCI8::OracleVersion.new(11, 1)
  # @private
  ORAVER_12_1 = OCI8::OracleVersion.new(12, 1)
  # @private
  ORAVER_18 = OCI8::OracleVersion.new(18)
  # @private
  ORAVER_23 = OCI8::OracleVersion.new(23)

  # @private
  @@oracle_client_version = OCI8::OracleVersion.new(self.oracle_client_vernum)

  # Returns an OCI8::OracleVersion of the Oracle client version.
  #
  # If this library is configured without '--with-runtime-check',
  # and compiled for Oracle 10.1 or lower, the major and minor
  # numbers are determined at compile-time. The rests are zeros.
  #
  # If this library is configured with '--with-runtime-check'
  # and the runtime Oracle library is Oracle 10.1 or lower, the
  # major and minor numbers are determined at run-time. The
  # rests are zeros.
  #
  # Otherwise, it is the version retrieved from an OCI function
  # OCIClientVersion().
  #
  # @return [OCI8::OracleVersion] Oracle client version
  # @see OCI8#oracle_server_version
  def self.oracle_client_version
    @@oracle_client_version
  end

  # defined for backward compatibility.
  # @private
  CLIENT_VERSION = @@oracle_client_version.major.to_s +
    @@oracle_client_version.minor.to_s +
    @@oracle_client_version.update.to_s
end

require 'oci8/ocihandle.rb'
require 'oci8/datetime.rb'
require 'oci8/oci8.rb'
require 'oci8/cursor.rb'
require 'oci8/bindtype.rb'
require 'oci8/metadata.rb'
require 'oci8/compat.rb'
require 'oci8/object.rb'
require 'oci8/connection_pool.rb'
require 'oci8/properties.rb'
