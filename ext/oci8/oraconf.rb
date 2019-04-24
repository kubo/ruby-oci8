# coding: ascii
require 'mkmf'

# compatibility for ruby-1.9
RbConfig = Config unless defined? RbConfig

if RUBY_PLATFORM =~ /mswin32|mswin64|cygwin|mingw32|bccwin32/
  # Windows
  require 'win32/registry'
  module Registry

    class OracleHome
      attr_reader :name
      attr_reader :path
      def initialize(name, path)
        @name = name
        @path = path
      end
    end

    def self.oracle_homes
      homes = []
      Win32::Registry::HKEY_LOCAL_MACHINE.open('SOFTWARE\Oracle') do |key|
        key.each_key do |subkey_name|
          subkey = key.open(subkey_name)
          begin
            homes << OracleHome.new(subkey['ORACLE_HOME_NAME'], subkey['ORACLE_HOME'].chomp('\\'))
          rescue Win32::Registry::Error
            # ignore errors
          end
        end
      end
      homes
    end

  end
end

# minimal implementation to read information of a shared object.
class MiniSOReader
  attr_reader :file_format
  attr_reader :cpu
  attr_reader :endian
  attr_reader :bits

  def initialize(filename)
    f = open(filename, 'rb')
    begin
      case file_header = f.read(2)
      when "\177E"
        # Linux, Solaris and HP-UX(64-bit)
        read_elf(f) if f.read(2) == 'LF'
      when "MZ"
        # Windows
        read_pe(f)
      when "\x02\x10"
        # HP-UX PA-RISC1.1
        read_parisc(f)
      when "\xfe\xed"
        # Big-endian Mach-O File
        read_mach_o_be(f)
      when "\xce\xfa"
        # 32-bit Little-endian Mach-O File
        read_mach_o_le(f, 32)
      when "\xcf\xfa"
        # 64-bit Little-endian Mach-O File
        read_mach_o_le(f, 64)
      when "\xca\xfe"
        # Universal binary
        read_mach_o_unversal(f)
      else
        # AIX and Tru64
        raise format("unknown file header: %02x %02x", file_header[0].to_i, file_header[1].to_i)
      end
    ensure
      f.close
    end
  end

  private
  # ELF format
  def read_elf(f)
    # 0-3 "\177ELF"
    @file_format = :elf
    # 4
    case f.read(1).unpack('C')[0]
    when 1
      @bits = 32
    when 2
      @bits = 64
    else
      raise 'Invalid ELF class'
    end
    # 5
    case f.read(1).unpack('C')[0]
    when 1
      @endian = :little
      pack_type_short = 'v'
    when 2
      @endian = :big
      pack_type_short = 'n'
    else
      raise 'Invalid ELF byte order'
    end
    # 6
    raise 'Invalid ELF header version' if f.read(1) != "\x01"
    # 16-17
    f.seek(16, IO::SEEK_SET)
    raise 'Invalid ELF filetype' if f.read(2).unpack(pack_type_short)[0] != 3
    # 18-19
    case archtype = f.read(2).unpack(pack_type_short)[0]
    when 2
      @cpu = :sparc
    when 3
      @cpu = :i386
    when 15
      @cpu = :parisc
    when 18
      @cpu = :sparc32plus
    when 20
      @cpu = :ppc
    when 21
      @cpu = :ppc64
    when 22
      @cpu = :s390
    when 43
      @cpu = :sparcv9
    when 50
      @cpu = :ia64
    when 62
      @cpu = :x86_64
    else
      raise "Invalid ELF archtype: #{archtype}"
    end
  end

  # PE/COFF format
  def read_pe(f)
    # 0-1 "MZ"
    @file_format = :pe
    # 60-63
    f.seek(60, IO::SEEK_SET)
    pe_offset = f.read(4).unpack('V')[0]
    # read PE signature
    f.seek(pe_offset)
    raise 'invalid pe format' if f.read(4) != "PE\000\000"
    # read COFF header
    case machine = f.read(2).unpack('v')[0]
    when 0x014c
      @cpu = :i386
      @endian = :little
      @bits = 32
    when 0x0200
      @cpu = :ia64
      @endian = :little
      @bits = 64
    when 0x8664
      @cpu = :x86_64
      @endian = :little
      @bits = 64
    else
      raise "Invalid coff machine: #{machine}"
    end
  end

  # HP-UX PA-RISC(32 bit)
  def read_parisc(f)
    # 0-1 system_id  - CPU_PA_RISC1_1
    @file_format = :pa_risc
    # 2-3 a_magic    - SHL_MAGIC
    raise 'invalid a_magic' if f.read(2).unpack('n')[0] != 0x10e
    @bits = 32
    @endian = :big
    @cpu = :parisc
  end

  # Big-endian Mach-O File
  def read_mach_o_be(f)
    @file_format = :mach_o
    @endian = :big
    case f.read(2)
    when "\xfa\xce" # feedface
      @cpu = :ppc
      @bits = 32
    when "\xfa\xcf" # feedfacf
      @cpu = :ppc64
      @bits = 64
    else
      raise "unknown file format"
    end
  end

  def read_mach_o_le(f, bits)
    @file_format = :mach_o
    @endian = :little
    raise 'unknown file format' if f.read(2) != "\xed\xfe"
    case bits
    when 32
      @cpu = :i386
      @bits = 32
    when 64
      @cpu = :x86_64
      @bits = 64
    end
  end

  def read_mach_o_unversal(f)
    raise 'unknown file format' if f.read(2) != "\xba\xbe" # cafebabe
    @file_format = :universal
    nfat_arch = f.read(4).unpack('N')[0]
    @cpu = []
    @endian = []
    @bits = []
    nfat_arch.times do
      case cputype = f.read(4).unpack('N')[0]
      when 7
        @cpu    << :i386
        @endian << :little
        @bits   << 32
      when 7 + 0x01000000
        @cpu    << :x86_64
        @endian << :little
        @bits   << 64
      when 18
        @cpu    << :ppc
        @endian << :big
        @bits   << 32
      when 18 + 0x01000000
        @cpu    << :ppc64
        @endian << :big
        @bits   << 64
      else
        raise "Unknown mach-o cputype: #{cputype}"
      end
      f.seek(4 * 4, IO::SEEK_CUR)
    end
  end
end

class OraConf
  attr_reader :cc_is_gcc
  attr_reader :version
  attr_reader :cflags
  attr_reader :libs

  def initialize
    raise 'use OraConf.get instead'
  end

  def self.get
    original_CFLAGS = $CFLAGS
    original_defs = $defs
    begin
      # check Oracle instant client
      if with_config('instant-client')
        puts <<EOS
=======================================================

  '--with-instant-client' is an obsolete option. ignore it.

=======================================================
EOS
      end
      inc, lib = dir_config('instant-client')
      if inc && lib
        OraConfIC.new(inc, lib)
      else
	base = guess_ic_dir
	if base
          inc, lib = guess_dirs_from_ic_base(base)
          OraConfIC.new(inc, lib)
	else
	  OraConfFC.new
	end
      end
    rescue
      case ENV['LANG']
      when /^ja/
        lang = 'ja'
      else
        lang = 'en'
      end
      print <<EOS
---------------------------------------------------
Error Message:
  #{$!.to_s.gsub(/\n/, "\n  ")}
Backtrace:
  #{$!.backtrace.join("\n  ")}
---------------------------------------------------
See:
 * http://www.rubydoc.info/github/kubo/ruby-oci8/file/docs/install-full-client.md for Oracle full client
 * http://www.rubydoc.info/github/kubo/ruby-oci8/file/docs/install-instant-client.md for Oracle instant client
 * http://www.rubydoc.info/github/kubo/ruby-oci8/file/docs/install-on-osx.md for OS X
 * http://www.rubydoc.info/github/kubo/ruby-oci8/file/docs/report-installation-issue.md to report an issue.

EOS
      exc = RuntimeError.new
      exc.set_backtrace($!.backtrace)
      raise exc
    ensure
      $CFLAGS = original_CFLAGS
      $defs = original_defs
    end
  end

  # Guess the include and directory paths from 
  def self.guess_dirs_from_ic_base(ic_dir)
    if ic_dir =~ /^\/usr\/lib(?:64)?\/oracle\/(\d+(?:\.\d+)*)\/client(64)?\/lib(?:64)?/
      # rpm package
      #   x86 rpms after 11.1.0.7.0:
      #    library: /usr/lib/oracle/X.X/client/lib/
      #    include: /usr/include/oracle/X.X/client/
      #
      #   x86_64 rpms after 11.1.0.7.0:
      #    library: /usr/lib/oracle/X.X/client64/lib/
      #    include: /usr/include/oracle/X.X/client64/
      #
      #   x86 rpms before 11.1.0.6.0:
      #    library: /usr/lib/oracle/X.X.X.X/client/lib/
      #    include: /usr/include/oracle/X.X.X.X/client/
      #
      #   x86_64 rpms before 11.1.0.6.0:
      #    library: /usr/lib/oracle/X.X.X.X/client64/lib/
      #    include: /usr/include/oracle/X.X.X.X/client64/
      #
      #   third-party x86_64 rpms(*1):
      #    library: /usr/lib64/oracle/X.X.X.X/client/lib/
      #          or /usr/lib64/oracle/X.X.X.X/client/lib64/
      #    include: /usr/include/oracle/X.X.X.X/client/
      #
      #   *1 These had been used before Oracle released official x86_64 rpms.
      #
      lib_dir = ic_dir
      inc_dir = "/usr/include/oracle/#{$1}/client#{$2}"
    else
      # zip package
      lib_dir = ic_dir
      inc_dir = "#{ic_dir}/sdk/include"
    end

    [inc_dir, lib_dir]
  end

  def self.ld_envs
    @@ld_envs
  end

  private

  def self.make_proc_to_check_cpu(*expect)
    Proc.new do |file|
      so = MiniSOReader.new(file)
      if expect.include? so.cpu
        true
      else
        puts "  skip: #{file} is for #{so.cpu} cpu."
        false
      end
    end
  end

  def self.guess_ic_dir
    puts "attempting to locate oracle-instantclient..."
    puts "checking load library path... "
    STDOUT.flush

    # get library load path names
    oci_basename = 'libclntsh'
    oci_glob_postfix = '.[0-9]*'
    nls_data_basename = ['libociei', 'libociicus']
    @@ld_envs = %w[LD_LIBRARY_PATH]
    so_ext = 'so'
    nls_data_ext = nil
    check_proc = nil
    size_of_pointer = begin
                        # the size of a pointer.
                        [nil].pack('P').size
                      rescue ArgumentError
                        # Rubinius 1.2.3 doesn't support Array#pack('P').
                        # Use Integer#size, which returns the size of long.
                        1.size
                      end
    is_32bit = size_of_pointer == 4
    is_big_endian = "\x01\x02".unpack('s')[0] == 0x0102
    case RUBY_PLATFORM
    when /mswin32|mswin64|cygwin|mingw32|bccwin32/
      oci_basename = 'oci'
      oci_glob_postfix = ''
      nls_data_basename = ['oraociei*', 'oraociicus*']
      @@ld_envs = %w[PATH]
      so_ext = 'dll'
      check_proc = make_proc_to_check_cpu(is_32bit ? :i386 : :x86_64)
    when /i.86-linux/
      check_proc = make_proc_to_check_cpu(:i386)
    when /ia64-linux/
      check_proc = make_proc_to_check_cpu(:ia64)
    when /x86_64-linux/
      # RUBY_PLATFORM depends on the compilation environment.
      # Even though it is x86_64-linux, the compiled ruby may
      # be a 32-bit executable.
      check_proc = make_proc_to_check_cpu(is_32bit ? :i386 : :x86_64)
    when /solaris/
      if is_32bit
        @@ld_envs = %w[LD_LIBRARY_PATH_32 LD_LIBRARY_PATH]
        if is_big_endian
          check_proc = make_proc_to_check_cpu(:sparc, :sparc32plus)
        else
          check_proc = make_proc_to_check_cpu(:i386)
        end
      else
        @@ld_envs = %w[LD_LIBRARY_PATH_64 LD_LIBRARY_PATH]
        if is_big_endian
          check_proc = make_proc_to_check_cpu(:sparcv9)
        else
          check_proc = make_proc_to_check_cpu(:x86_64)
        end
      end
    when /aix/
      oci_glob_postfix = ''
      @@ld_envs = %w[LIBPATH]
      so_ext = 'a'
      nls_data_ext = 'so'
    when /hppa.*-hpux/
      if is_32bit
        @@ld_envs = %w[SHLIB_PATH]
      end
      so_ext = 'sl'
    when /darwin/
      @@ld_envs = %w[DYLD_LIBRARY_PATH]
      so_ext = 'dylib'
      if is_32bit
        if is_big_endian
          this_cpu = :ppc    # 32-bit big-endian
        else
          this_cpu = :i386   # 32-bit little-endian
        end
      else
        if is_big_endian
          this_cpu = :ppc64  # 64-bit big-endian
        else
          this_cpu = :x86_64 # 64-bit little-endian
        end
      end
      check_proc = Proc.new do |file|
        so = MiniSOReader.new(file)
        if so.file_format == :universal
          if so.cpu.include? this_cpu
            true
          else
            if so.cpu.size > 1
              arch_types = so.cpu[0..-2].join(', ') + ' and ' + so.cpu[-1].to_s
            else
              arch_types = so.cpu[0]
            end
            puts "  skip: #{file} is for #{arch_types} cpu."
            false
          end
        else
          if so.cpu == this_cpu
            true
          else
            puts "  skip: #{file} is for #{so.cpu} cpu."
            false
          end
        end
      end
    end

    glob_name = "#{oci_basename}.#{so_ext}#{oci_glob_postfix}"
    ld_path = nil
    file = nil
    @@ld_envs.each do |env|
      if ENV[env].nil?
        puts "  #{env} is not set."
        next
      end
      puts "  #{env}... "
      ld_path, file = check_lib_in_path(ENV[env], glob_name, check_proc)
      break if ld_path
    end

    if ld_path.nil?
      case RUBY_PLATFORM
      when /linux/
        open("|/sbin/ldconfig -p") do |f|
          print "  checking ld.so.conf... "
          STDOUT.flush
          while line = f.gets
            if line =~ /libclntsh\.so\..* => (\/.*)\/libclntsh\.so\.(.*)/
              file = "#$1/libclntsh.so.#$2"
              path = $1
              next if (check_proc && !check_proc.call(file))
              ld_path = path
              puts "yes"
              break
            end
          end
          puts "no"
        end
      when /solaris/
        if is_32bit
          crle_cmd = 'crle'
        else
          crle_cmd = 'crle -64'
        end
        open('|env LANG=C /usr/bin/' + crle_cmd) do |f|
          while line = f.gets
            if line =~ /Default Library Path[^:]*:\s*(\S*)/
              puts "  checking output of `#{crle_cmd}'... "
              ld_path, file = check_lib_in_path($1, glob_name, check_proc)
              break
            end
          end
        end
      when /darwin/
        fallback_path = ENV['DYLD_FALLBACK_LIBRARY_PATH']
        if fallback_path.nil?
          puts "  DYLD_FALLBACK_LIBRARY_PATH is not set."
        else
          puts "  checking DYLD_FALLBACK_LIBRARY_PATH..."
          ld_path, file = check_lib_in_path(fallback_path, glob_name, check_proc)
        end
        if ld_path.nil?
          puts "  checking OCI_DIR..."
          ld_path, file = check_lib_in_path(ENV['OCI_DIR'], glob_name, check_proc)
          if ld_path
            puts "  checking dependent shared libraries in #{file}..."
            open("|otool -L #{file}") do |f|
              f.gets # discard the first line
              while line = f.gets
                line =~ /^\s+(\S+)/
                libname = $1
                case libname
                when /^@rpath\/libclntsh\.dylib/, /^@rpath\/libnnz\d\d\.dylib/, /^@loader_path\/libnnz\d\d\.dylib/
                  # No need to check the real path.
                  # The current instant client doesn't use @rpath or @loader_path.
                when /\/libclntsh\.dylib/, /\/libnnz\d\d.dylib/
                  raise <<EOS unless File.exists?(libname)
The output of "otool -L #{file}" is:
  | #{IO.readlines("|otool -L #{file}").join('  | ')}
Ruby-oci8 doesn't work without DYLD_LIBRARY_PATH or DYLD_FALLBACK_LIBRARY_PATH
because the dependent file "#{libname}" doesn't exist.

If you need to use ruby-oci8 without DYLD_LIBRARY_PATH or DYLD_FALLBACK_LIBRARY_PATH,
download "fix_oralib.rb" in https://github.com/kubo/fix_oralib_osx
and execute it in the directory "#{File.dirname(file)}" as follows to fix the path.

  cd #{File.dirname(file)}
  curl -O https://raw.githubusercontent.com/kubo/fix_oralib_osx/master/fix_oralib.rb
  ruby fix_oralib.rb

Note: DYLD_* environment variables are unavailable for security reasons on OS X 10.11 El Capitan.
EOS
                end
              end
            end
          end
        end
        if ld_path.nil?
          fallback_path = ENV['DYLD_FALLBACK_LIBRARY_PATH']
          if fallback_path.nil?
            fallback_path = "#{ENV['HOME']}/lib:/usr/local/lib:/lib:/usr/lib"
            puts "  checking the default value of DYLD_FALLBACK_LIBRARY_PATH..."
            ld_path, file = check_lib_in_path(fallback_path, glob_name, check_proc)
          end
        end
        if ld_path.nil?
          raise <<EOS
Set the environment variable DYLD_LIBRARY_PATH, DYLD_FALLBACK_LIBRARY_PATH or
OCI_DIR to point to the Instant client directory.

If DYLD_LIBRARY_PATH or DYLD_FALLBACK_LIBRARY_PATH is set, the environment
variable must be set at runtime also.

If OCI_DIR is set, dependent shared library paths are checked. If the checking
is passed, ruby-oci8 works without DYLD_LIBRARY_PATH or DYLD_FALLBACK_LIBRARY_PATH.

Note: OCI_DIR should be absolute path.
Note: DYLD_* environment variables are unavailable for security reasons on OS X 10.11 El Capitan.
EOS
        end
      end
    end

    if ld_path
      nls_data_ext ||= so_ext # nls_data_ext is same with so_ext by default.
      nls_data_basename.each do |basename|
        if Dir.glob(File.join(ld_path, "#{basename}.#{nls_data_ext}")).size > 0
          puts "  #{file} looks like an instant client."
          return ld_path
        end
      end
      puts "  #{file} looks like a full client."
    end
    nil
  end

  def self.check_lib_in_path(paths, glob_name, check_proc)
    return nil if paths.nil?
    paths.split(File::PATH_SEPARATOR).each do |path|
      next if path.nil? or path == ''
      print "    checking #{path}... "
      path.gsub!(/\\/, '/') if /mswin32|mswin64|cygwin|mingw32|bccwin32/ =~ RUBY_PLATFORM
      files = Dir.glob(File.join(path, glob_name))
      if files.empty?
        puts "no"
        next
      end
      STDOUT.flush
      next if (check_proc && !check_proc.call(files[0]))
      puts "yes"
      return path, files[0]
    end
    nil
  end

  def init
    check_cc()
    @cc_is_gcc = check_cc_is_gcc()
    @lp64 = check_lp64()
    check_system_header()
    check_ruby_header()
  end

  def check_cc
    print "checking for cc... "
    STDOUT.flush
    if try_run("int main() { return 0; }")
      puts "ok"
    else
      puts "failed"
      raise "C compiler doesn't work correctly."
    end
  end # check_cc

  def check_cc_is_gcc
    # bcc defines __GNUC__. why??
    return false if RUBY_PLATFORM =~ /bccwin32/

    print "checking for gcc... "
    STDOUT.flush
    if macro_defined?("__GNUC__", "")
      print "yes\n"
      return true
    else
      print "no\n"
      return false
    end
  end # check_cc_is_gcc

  def check_lp64
    print "checking for LP64... "
    STDOUT.flush
    if try_run("int main() { return sizeof(long) == 8 ? 0 : 1; }")
      puts "yes"
      true
    else
      puts "no"
      false
    end
  end # check_lp64

  def check_system_header
    if not have_header('sys/types.h')
      raise <<EOS
A standard C header file 'sys/types.h' doesn't exist.
Did you install glibc-devel(redhat) or libc6-dev(debian/ubuntu)?
EOS
    end
  end

  def check_ruby_header
    print "checking for ruby header... "
    STDOUT.flush
    rubyhdrdir = RbConfig::CONFIG["rubyhdrdir"] || RbConfig::CONFIG['archdir']
    unless File.exist?(rubyhdrdir + '/ruby.h')
      puts "failed"
      if RUBY_PLATFORM =~ /darwin/ and File.exist?("#{RbConfig::CONFIG['archdir']}/../universal-darwin8.0/ruby.h")
        raise <<EOS
#{RbConfig::CONFIG['archdir']}/ruby.h doesn't exist.
Run the following commands to fix the problem.

  cd #{RbConfig::CONFIG['archdir']}
  sudo ln -s ../universal-darwin8.0/* ./
EOS
      else
        raise <<EOS
#{RbConfig::CONFIG['archdir']}/ruby.h doesn't exist.
Install the ruby development library.
EOS
      end
    end
    puts "ok"
    $stdout.flush
  end # check_ruby_header

  def try_link_oci
    original_libs = $libs
    begin
      $libs += " -L#{CONFIG['libdir']} " + @libs
      have_func("OCIEnvCreate", "oci.h")
    ensure
      $libs = original_libs
    end
  end

  if RUBY_PLATFORM =~ /mswin32|mswin64|cygwin|mingw32|bccwin32/ # when Windows

    def get_libs(lib_dir)
      case RUBY_PLATFORM
      when /cygwin|x64-mingw32/
        regex = ([nil].pack('P').size == 8) ? / T (OCI\w+)/ : / T _(OCI\w+)/
        oci_funcs = YAML.load(open(File.dirname(__FILE__) + '/apiwrap.yml')).keys.collect do |func|
          func =~ /_nb$/ ? $` : func
        end
        open("OCI.def", "w") do |f|
          f.puts("EXPORTS")
          open("|nm #{lib_dir}/MSVC/OCI.LIB") do |r|
            while line = r.gets
              f.puts($1) if regex =~ line and oci_funcs.include?($1)
            end
          end
        end
        command = "dlltool -d OCI.def -D OCI.DLL -l libOCI.a"
        print("Running: '#{command}' ...")
        STDOUT.flush
        system(command)
        puts("done")
        "-L. -lOCI"
      when /bccwin32/
        # replace '/' to '\\' because bcc linker misunderstands
        # 'C:/foo/bar/OCI.LIB' as unknown option.
        lib = "#{lib_dir}/BORLAND/OCI.LIB"
        return lib.tr('/', '\\') if File.exist?(lib)
        raise <<EOS
#{lib} does not exist.

Your Oracle may not support Borland C++.
If you want to run this module, run the following command at your own risk.
  cd #{lib_dir.tr('/', '\\')}
  mkdir Borland
  cd Borland
  coff2omf ..\\MSVC\\OCI.LIB OCI.LIB
EOS
        exit 1
      else
        "\"#{lib_dir}/MSVC/OCI.LIB\""
      end
    end

  else
    # Unix
    def get_libs(lib_dir)
      case RUBY_PLATFORM
      when /solaris/
        " -L#{lib_dir} -R#{lib_dir} -lclntsh"
      when /linux/,/darwin/
        " -L#{lib_dir} -Wl,-rpath,#{lib_dir} -lclntsh"
      else
        " -L#{lib_dir} -lclntsh"
      end
    end

  end
end

# OraConf for Full Client
class OraConfFC < OraConf
  def initialize
    init

    @oracle_home = get_home()
    if RUBY_PLATFORM =~ /freebsd/ && @oracle_home == '/usr/local/oracle8-client'
      raise "Oralce 8i is not supported."
    else
      @version = get_version()
    end
    @cflags = get_cflags()
    $CFLAGS += @cflags

    if !@lp64 && File.exist?("#{@oracle_home}/lib32")
      # ruby - 32bit
      # oracle - 64bit
      use_lib32 = true
    else
      use_lib32 = false
    end

    if RUBY_PLATFORM =~ /mswin32|mswin64|cygwin|mingw32|bccwin32/
      lib_dir = "#{@oracle_home}/oci/lib"
    elsif use_lib32
      lib_dir = "#{@oracle_home}/lib32"
    else
      lib_dir = "#{@oracle_home}/lib"
    end
    @libs = get_libs(lib_dir)
    return if try_link_oci()

    raise 'cannot compile OCI'
  end

  private

  def get_version
    print("Get the version of Oracle from SQL*Plus... ")
    STDOUT.flush
    version = nil
    dev_null = RUBY_PLATFORM =~ /mswin32|mswin64|mingw32|bccwin32/ ? "nul" : "/dev/null"
    if File.exist?("#{@oracle_home}/bin/plus80.exe")
      sqlplus = "plus80.exe"
    else
      sqlplus = "sqlplus"
    end
    Logging::open do
      ENV['NLS_LANG'] = 'american_america.us7ascii'
      open("|#{@oracle_home}/bin/#{sqlplus} < #{dev_null}") do |f|
        while line = f.gets
          print line
          if line =~ /(\d+)\.(\d)\.(\d)/
            version = $1 + $2 + $3
            break
          end
        end
      end
    end
    if version.nil?
      raise 'cannot get Oracle version from sqlplus'
    end
    puts version
    version
  end # get_version

  if RUBY_PLATFORM =~ /mswin32|mswin64|cygwin|mingw32|bccwin32/ # when Windows

    def is_valid_home?(oracle_home)
      return false if oracle_home.nil?
      sqlplus = "#{oracle_home}/bin/sqlplus.exe"
      print("checking for ORACLE_HOME(#{oracle_home})... ")
      STDOUT.flush
      if File.exist?(sqlplus)
        puts("yes")
        true
      else
        puts("no")
        false
      end
    end

    def get_home()
      oracle_home = ENV['ORACLE_HOME']
      if oracle_home.nil?
        oracle_homes = Registry::oracle_homes.select do |home|
          is_valid_home?(home.path)
        end
        if oracle_homes.empty?
          raise <<EOS
Set the environment variable ORACLE_HOME if Oracle Full Client.
Append the path of Oracle client libraries to #{OraConf.ld_envs[0]} if Oracle Instant Client.
EOS
        end
        if oracle_homes.length == 1
          oracle_home = oracle_homes[0].path
        else
          default_path = ''
          if RUBY_PLATFORM =~ /cygwin/
             path_sep = ':'
             dir_sep = '/'
          else
             path_sep = ';'
             dir_sep = '\\'
          end
          ENV['PATH'].split(path_sep).each do |path|
            path.chomp!(dir_sep)
            if File.exist?("#{path}/OCI.DLL")
              default_path = path
              break
            end
          end
          puts "---------------------------------------------------"
          puts "Multiple Oracle Homes are found."
          printf "   %-15s : %s\n", "[NAME]", "[PATH]"
          oracle_homes.each do |home|
            if RUBY_PLATFORM =~ /cygwin/
              path = `cygpath -u '#{home.path}'`.chomp!
            else
              path = home.path
            end
            if default_path.downcase == "#{path.downcase}#{dir_sep}bin"
              oracle_home = home
            end
            printf "   %-15s : %s\n", home.name, home.path
          end
          if oracle_home.nil?
            puts "default oracle home is not found."
            puts "---------------------------------------------------"
            raise 'Cannot get ORACLE_HOME. Please set the environment valiable ORACLE_HOME.'
          else
            printf "use %s\n", oracle_home.name
            puts "run ohsel.exe to use another oracle home."
            puts "---------------------------------------------------"
            oracle_home = oracle_home.path
          end
        end
      end
      if RUBY_PLATFORM =~ /cygwin/
        oracle_home = oracle_home.sub(/^([a-zA-Z]):/, "/cygdrive/\\1")
      end
      oracle_home.gsub(/\\/, '/')
    end

    def get_cflags
      unless File.exist?("#{@oracle_home}/OCI/INCLUDE/OCI.H")
        raise "'#{@oracle_home}/OCI/INCLUDE/OCI.H' does not exists. Please install 'Oracle Call Interface'."
      end
      if RUBY_PLATFORM =~ /cygwin|mingw32/
        " \"-I#{@oracle_home}/OCI/INCLUDE\" -D_int64=\"long long\""
      else
        " \"-I#{@oracle_home}/OCI/INCLUDE\""
      end
    end

  else # when UNIX

    def get_home
      oracle_home = ENV['ORACLE_HOME']
      if oracle_home.nil?
        msg = <<EOS
Set the environment variable ORACLE_HOME if Oracle Full Client.
Append the path of Oracle client libraries to #{OraConf.ld_envs[0]} if Oracle Instant Client.
EOS

        # check sudo environment
        sudo_command = ENV['SUDO_COMMAND']
        if /\w*make\b/ =~ sudo_command
          msg += <<EOS

The 'sudo' command unset some environment variables for security reasons.
Use it only when running 'make install' as follows
     make
     sudo make install
EOS
        end
        if /\w+\/gem\b/ =~ sudo_command
          msg += <<EOS

The 'sudo' command unset some environment variables for security reasons.
Pass required varialbes as follows
     sudo env #{OraConf.ld_envs[0]}=$#{OraConf.ld_envs[0]} #{sudo_command}
  or 
     sudo env ORACLE_HOME=$ORACLE_HOME #{sudo_command}
EOS
        end
        raise msg
      end
      oracle_home
    end

    def get_cflags
      cflags = ''
      ok = false
      original_CFLAGS = $CFLAGS
      begin
        for i in ["rdbms/public", "rdbms/demo", "network/public", "plsql/public"]
          cflags += " -I#{@oracle_home}/#{i}"
          $CFLAGS += " -I#{@oracle_home}/#{i}"
          print("try #{cflags}\n");
          if have_header("oci.h")
            ok = true
            break
          end
        end
        unless ok
          if @version.to_i >= 1000
            oci_h = "#{@oracle_home}/rdbms/public/oci.h"
          else
            oci_h = "#{@oracle_home}/rdbms/demo/oci.h"
          end
          unless File.exist?(oci_h)
            raise "'#{oci_h}' does not exists. Install 'Oracle Call Interface' component."
          end
          raise 'Cannot get proper cflags.'
        end
        cflags
      ensure
        $CFLAGS = original_CFLAGS
      end
    end # get_cflags

  end
end

# OraConf for Instant Client
class OraConfIC < OraConf
  def initialize(inc_dir, lib_dir)
    init

    if RUBY_PLATFORM =~ /mswin32|mswin64|cygwin|mingw32|bccwin32/ # when Windows
      unless File.exist?("#{lib_dir}/sdk/lib/msvc/oci.lib")
        raise <<EOS
Could not compile with Oracle instant client.
#{lib_dir}/sdk/lib/msvc/oci.lib could not be found.
EOS
        raise 'failed'
      end
      @cflags = " \"-I#{inc_dir}\""
      @cflags += " -D_int64=\"long long\"" if RUBY_PLATFORM =~ /cygwin|mingw32/
      @libs = get_libs("#{lib_dir}/sdk/lib")
      ld_path = nil
    else
      @cflags = " -I#{inc_dir}"
      # set ld_path and so_ext
      case RUBY_PLATFORM
      when /aix/
        ld_path = 'LIBPATH'
        so_ext = 'a'
      when /hppa.*-hpux/
        if @lp64
          ld_path = 'LD_LIBRARY_PATH'
        else
          ld_path = 'SHLIB_PATH'
        end
        so_ext = 'sl'
      when /darwin/
        ld_path = 'DYLD_LIBRARY_PATH'
        so_ext = 'dylib'
      else
        ld_path = 'LD_LIBRARY_PATH'
        so_ext = 'so'
      end
      # check Oracle client library.
      unless File.exist?("#{lib_dir}/libclntsh.#{so_ext}")
        files = Dir.glob("#{lib_dir}/libclntsh.#{so_ext}.*")
        if files.empty?
          raise <<EOS
Could not compile with Oracle instant client.
'#{lib_dir}/libclntsh.#{so_ext}' could not be found.
Did you install instantclient-basic?
EOS
        else
          file = File.basename(files.sort[-1])
          raise <<EOS
Could not compile with Oracle instant client.
#{lib_dir}/libclntsh.#{so_ext} could not be found.
You may need to make a symbolic link.
   cd #{lib_dir}
   ln -s #{file} libclntsh.#{so_ext}
EOS
        end
        raise 'failed'
      end
      @libs = get_libs(lib_dir)
    end
    unless File.exist?("#{inc_dir}/oci.h")
          raise <<EOS
'#{inc_dir}/oci.h' does not exist.
Install 'Instant Client SDK'.
EOS
    end
    $CFLAGS += @cflags
    if try_link_oci()
      major = try_constant("OCI_MAJOR_VERSION", "oci.h")
      minor = try_constant("OCI_MINOR_VERSION", "oci.h")
      if major and minor
        @version = format('%d%d0', major, minor)
      else
        # 10.1.0 doesn't have OCI_MAJOR_VERSION and OCI_MINOR_VERSION in oci.h.
        @version = "1010"
        if RUBY_PLATFORM =~ /darwin/ and 1.size == 8 and `sw_vers -productVersion`.chomp == "10.7"
          $stderr.print <<EOS
WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN!

64-bit Oracle instant client doesn't work on OS X Lion.
See: https://forums.oracle.com/forums/thread.jspa?threadID=2187558

The compilation is continued because the issue may be fixed in future.

WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN! WARN!
EOS
        end
      end
      return
    end

    if RUBY_PLATFORM =~ /darwin/
      open('mkmf.log', 'r') do |f|
        while line = f.gets
          if line.include? '/libclntsh.dylib load command 8 unknown cmd field'
            raise <<EOS
Intel mac instant client is for Mac OS X 10.5.
It doesn't work on Mac OS X 10.4 or before.

You have three workarounds.
  1. Compile ruby as ppc binary and use it with ppc instant client.
  2. Use JRuby and JDBC
  3. Use a third-party ODBC driver and ruby-odbc.
EOS
            # '
          end

          case line
          when /cputype \(\d+, architecture \w+\) does not match cputype \(\d+\) for specified -arch flag: (\w+)/
            missing_arch = $1
          when /Undefined symbols for architecture (\w+)/
            missing_arch = $1
          when /missing required architecture (\w+) in file/
            missing_arch = $1
          end

          if missing_arch
            if [nil].pack('p').size == 8
              my_arch = 'x86_64'
            elsif "\x01\x02".unpack('s')[0] == 0x0201
              my_arch = 'i386'
            else
              my_arch = 'ppc'
            end
            raise <<EOS
Could not compile with Oracle instant client.
You may need to set the environment variable RC_ARCHS or ARCHFLAGS as follows:

    RC_ARCHS=#{my_arch}
    export RC_ARCHS
or
    ARCHFLAGS='-arch #{my_arch}'
    export RC_ARCHS

If it does not fix the problem, delete all '-arch #{missing_arch}'
in '#{RbConfig::CONFIG['archdir']}/rbconfig.rb'.
EOS
          end
        end
      end
    end

    unless ld_path.nil?
      raise <<EOS
Could not compile with Oracle instant client.
You may need to set a environment variable:
    #{ld_path}=#{lib_dir}
    export #{ld_path}
EOS
    end
    raise 'failed'
  end
end
