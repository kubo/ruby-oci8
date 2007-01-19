require 'mkmf'

unless defined? macro_defined?
  # ruby 1.6 doesn't have 'macro_defined?'.
  def macro_defined?(macro, src, opt="")
    try_cpp(src + <<"SRC", opt)
#ifndef #{macro}
# error
#endif
SRC
  end
end

module Logging
  unless Logging.respond_to?(:open)
    # emulate Logging::open of ruby 1.6.8 or later.

    if $log.nil? # ruby 1.6.2 doesn't have $log.
      $log = open('mkmf.log', 'w')
    end
    def Logging::open
      begin
        $stderr.reopen($log)
        $stdout.reopen($log)
        yield
      ensure
        $stderr.reopen($orgerr)
        $stdout.reopen($orgout)
      end
    end
  end
end # module Logging

module MiniRegistry
  class MiniRegistryError < StandardError
    attr_reader :api_name
    attr_reader :code
    def initialize(api_name, code)
      @api_name = api_name
      @code = code
    end
  end
  if RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/
    # Windows
    require 'Win32API' # raise LoadError when UNIX.

    # I looked in Win32Module by MoonWolf <URL:http://www.moonwolf.com/ruby/>,
    # copy the minimum code and reorganize it.
    ERROR_SUCCESS = 0
    ERROR_FILE_NOT_FOUND = 2

    HKEY_LOCAL_MACHINE = 0x80000002
    RegOpenKeyExA = Win32API.new('advapi32', 'RegOpenKeyExA', 'LPLLP', 'L')
    RegQueryValueExA = Win32API.new('advapi32','RegQueryValueExA','LPPPPP','L')
    RegCloseKey = Win32API.new('advapi32', 'RegCloseKey', 'L', 'L')

    def get_reg_value(root, subkey, name)
      result = [0].pack('L')
      code = RegOpenKeyExA.call(root, subkey, 0, 0x20019, result)
      if code != ERROR_SUCCESS
        raise MiniRegistryError.new("Win32::RegOpenKeyExA", code)
      end
      hkey = result.unpack('L')[0]
      begin
        lpcbData = [0].pack('L')
        code = RegQueryValueExA.call(hkey, name, nil, nil, nil, lpcbData)
        if code == ERROR_FILE_NOT_FOUND
          return nil
        elsif code != ERROR_SUCCESS
          raise MiniRegistryError.new("Win32::RegQueryValueExA",code)
        end
        len = lpcbData.unpack('L')[0]
        lpType = "\0\0\0\0"
        lpData = "\0"*len
        lpcbData = [len].pack('L')
        code = RegQueryValueExA.call(hkey, name, nil, lpType, lpData, lpcbData)
        if code != ERROR_SUCCESS
          raise MiniRegistryError.new("Win32::RegQueryValueExA",code)
        end
        lpData.unpack('Z*')[0]
      ensure
        RegCloseKey.call(hkey)
      end
    end
    def get_local_registry(subkey, name)
      get_reg_value(HKEY_LOCAL_MACHINE, subkey, name)
    end
  else
    # UNIX
    def get_local_registry(subkey, name)
      nil
    end
  end
end # module MiniRegistry

class OraConf
  include MiniRegistry

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
    ic_dir = nil
    begin
      # check Oracle instant client
      ic_dir = with_config('instant-client') || check_ic_dir
      if ic_dir
        OraConfIC.new(ic_dir)
      else
        OraConfFC.new()
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
error messages:
#{$!.to_str}
---------------------------------------------------
See:
 * http://ruby-oci8.rubyforge.org/#{lang}/HowToInstall.html
 * http://ruby-oci8.rubyforge.org/#{lang}/ReportInstallProblem.html

EOS
      exc = RuntimeError.new
      exc.set_backtrace($!.backtrace)
      raise exc
    ensure
      $CFLAGS = original_CFLAGS
      $defs = original_defs
    end
  end

  def self.ld_envs
    @@ld_envs
  end

  private

  def self.check_ic_dir
    print "checking for load library path... "
    STDOUT.flush

    # get library load path names
    oci_basename = 'libclntsh'
    oci_glob_postfix = '.[0-9]*'
    ocidata_basename = ['libociei', 'libociicus']
    @@ld_envs = %w[LD_LIBRARY_PATH]
    so_ext = 'so'
    check_proc = nil
    case RUBY_PLATFORM
    when /mswin32|cygwin|mingw32|bccwin32/
      oci_basename = 'oci'
      oci_glob_postfix = ''
      ocidata_basename = ['oraociei10', 'oraociicus10']
      @@ld_envs = %w[PATH]
      so_ext = 'dll'
    when /i.86-linux/
      check_proc = Proc.new do |file|
        File.read("|file #{file}").include? "80386"
      end
    when /ia64-linux/
      check_proc = Proc.new do |file|
        File.read("|file #{file}").include? "IA-64"
      end
    when /x86_64-linux/
      check_proc = Proc.new do |file|
        File.read("|file #{file}").include? "x86-64"
      end
    when /solaris/
      if [0].pack('l!').length == 8
        @@ld_envs = %w[LD_LIBRARY_PATH_64 LD_LIBRARY_PATH]
      else
        @@ld_envs = %w[LD_LIBRARY_PATH_32 LD_LIBRARY_PATH]
      end
    when /aix/
      oci_glob_postfix = ''
      @@ld_envs = %w[LIBPATH]
      so_ext = 'a'
    when /hppa.*-hpux/
      if [0].pack('l!').length == 4
        @@ld_envs = %w[SHLIB_PATH]
      end
      so_ext = 'sl'
    when /darwin/
      @@ld_envs = %w[DYLD_LIBRARY_PATH]
      so_ext = 'dylib'
    end

    glob_name = "#{oci_basename}.#{so_ext}#{oci_glob_postfix}"
    ld_path = nil
    file = nil
    @@ld_envs.collect do |env|
      print "(#{env})... "
      STDOUT.flush
      ENV[env] && ENV[env].split(File::PATH_SEPARATOR)
    end.flatten.each do |path|
      next if path.nil? or path == ''
      path.gsub!(/\\/, '/') if /mswin32|cygwin|mingw32|bccwin32/ =~ RUBY_PLATFORM
      files = Dir.glob(File.join(path, glob_name))
      next if files.empty?
      STDOUT.flush
      next if (check_proc && !check_proc.call(files[0]))
      file = files[0]
      ld_path = path
      break
    end

    if ld_path.nil? and RUBY_PLATFORM =~ /linux/
      open("|/sbin/ldconfig -p") do |f|
        print "(ld.so.conf)... "
        STDOUT.flush
        while line = f.gets
          if line =~ /libclntsh\.so\..* => (\/.*)\/libclntsh\.so\.(.*)/
            file = "#$1/libclntsh.so.#$2"
            path = $1
            next if (check_proc && !check_proc.call(file))
            ld_path = path
            break
          end
        end
      end
    end

    if ld_path
      ocidata_basename.each do |basename|
        if File.exists?(File.join(ld_path, "#{basename}.#{so_ext}"))
          puts "#{file} looks like an instant client."
          return ld_path
        end
      end
      puts "#{file} looks like a full client."
    else
      puts "not found"
    end
    nil
  end

  def init
    check_cc()
    @cc_is_gcc = check_cc_is_gcc()
    @lp64 = check_lp64()
    check_ruby_header()
  end

  def check_cc
    print "checking for cc... "
    STDOUT.flush
    if try_run("int main() { return 0; }")
      puts "ok"
    else
      puts "ng"
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

  def check_ruby_header
    print "checking for ruby header... "
    STDOUT.flush
    unless File.exists?("#{Config::CONFIG['archdir']}/ruby.h")
      puts "ng"
      if RUBY_PLATFORM =~ /darwin/ and File.exists?("#{Config::CONFIG['archdir']}/../universal-darwin8.0/ruby.h")
        raise <<EOS
#{Config::CONFIG['archdir']}/ruby.h doesn't exist.
Run the following commands to fix the problem.

  cd #{Config::CONFIG['archdir']}
  sudo ln -s ../universal-darwin8.0/* ./
EOS
      else
        raise <<EOS
#{Config::CONFIG['archdir']}/ruby.h doesn't exist.
Install the ruby development library.
EOS
      end
    end
    if RUBY_PLATFORM =~ /linux/ and not File.exist?("/usr/include/sys/types.h")
      raise <<EOS
Do you install glibc-devel(redhat) or libc6-dev(debian)?
You need /usr/include/sys/types.h to compile ruby-oci8.
EOS
    end
    puts "ok"
  end # check_ruby_header

  def try_link_oci
    original_libs = $libs
    begin
      $libs += " -L#{CONFIG['libdir']} " + @libs
      have_func("OCIInitialize", "oci.h")
    ensure
      $libs = original_libs
    end
  end

  if RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/ # when Windows

    def get_libs(base_dir = oci_base_dir)
      case RUBY_PLATFORM
      when /cygwin/
        open("OCI.def", "w") do |f|
          f.puts("EXPORTS")
          open("|nm #{base_dir}/LIB/MSVC/OCI.LIB") do |r|
            while line = r.gets
              f.puts($') if line =~ / T _/
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
        lib = "#{base_dir}/LIB/BORLAND/OCI.LIB"
        return lib.tr('/', '\\') if File.exist?(lib)
        raise <<EOS
#{lib} does not exist.

Your Oracle may not support Borland C++.
If you want to run this module, run the following command at your own risk.
  cd #{base_dir.tr('/', '\\')}\\LIB
  mkdir Borland
  cd Borland
  coff2omf ..\\MSVC\\OCI.LIB OCI.LIB
EOS
        exit 1
      else
        "\"#{base_dir}/LIB/MSVC/OCI.LIB\""
      end
    end

  end
end

# OraConf for Full Client
class OraConfFC < OraConf
  def initialize
    init

    @oracle_home = get_home()
    @version = get_version()
    @cflags = get_cflags()
    $CFLAGS += @cflags

    if !@lp64 && File.exist?("#{@oracle_home}/lib32")
      # ruby - 32bit
      # oracle - 64bit
      use_lib32 = true
    else
      use_lib32 = false
    end

    # default
    if @version.to_i >= 900
      if use_lib32
        lib_dir = "#{@oracle_home}/lib32"
      else
        lib_dir = "#{@oracle_home}/lib"
      end
      case RUBY_PLATFORM
      when /solaris/
        @libs = " -L#{lib_dir} -R#{lib_dir} -lclntsh"
      when /linux/
        @libs = " -L#{lib_dir} -Wl,-rpath,#{lib_dir} -lclntsh"
      else
        @libs = " -L#{lib_dir} -lclntsh"
      end
      return if try_link_oci()
    end

    # get from demo_rdbms.mk
    if use_lib32
      if File.exist?("#{@oracle_home}/rdbms/demo/demo_rdbms32.mk")
        @libs = get_libs('32', '')
      else
        @libs = get_libs('', '32')
      end
    else
      @libs = get_libs()
    end
    return if try_link_oci()

    raise 'cannot compile OCI'
  end

  private

  def get_version
    print("Get the version of Oracle from SQL*Plus... ")
    STDOUT.flush
    version = nil
    dev_null = RUBY_PLATFORM =~ /mswin32|mingw32|bccwin32/ ? "nul" : "/dev/null"
    if File.exists?("#{@oracle_home}/bin/plus80.exe")
      sqlplus = "plus80.exe"
    else
      sqlplus = "sqlplus"
    end
    Logging::open do
      open("|#{@oracle_home}/bin/#{sqlplus} < #{dev_null}") do |f|
        while line = f.gets
          if line =~ /(8|9|10)\.([012])\.([0-9])/
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

  if RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/ # when Windows

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
        struct = Struct.new("OracleHome", :name, :path)
        oracle_homes = []
        begin
          last_home = get_local_registry("SOFTWARE\\ORACLE\\ALL_HOMES", 'LAST_HOME')
          0.upto last_home.to_i do |id|
             oracle_homes << "HOME#{id}"
          end
        rescue MiniRegistryError
        end
        oracle_homes << "KEY_XE"
        oracle_homes << "KEY_XEClient"
        oracle_homes.collect! do |home|
          begin
            name = get_local_registry("SOFTWARE\\ORACLE\\#{home}", 'ORACLE_HOME_NAME')
            path = get_local_registry("SOFTWARE\\ORACLE\\#{home}", 'ORACLE_HOME')
            path.chomp!("\\")
            struct.new(name, path) if is_valid_home?(path)
          rescue MiniRegistryError
            nil
          end
        end
        oracle_homes.compact!
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
            if File.exists?("#{path}/OCI.DLL")
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

    def oci_base_dir
      case @version
      when /80./
        "#{@oracle_home}/OCI80"
      else
        "#{@oracle_home}/OCI"
      end
    end

    def get_cflags
      unless File.exist?("#{oci_base_dir}/INCLUDE/OCI.H")
        raise "'#{oci_base_dir}/INCLUDE/OCI.H' does not exists. Please install 'Oracle Call Interface'."
      end
      if RUBY_PLATFORM =~ /cygwin/
        " \"-I#{oci_base_dir}/INCLUDE\" -D_int64=\"long long\""
      else
        " \"-I#{oci_base_dir}/INCLUDE\""
      end
    end

  else # when UNIX

    def get_home
      oracle_home = ENV['ORACLE_HOME']
      if oracle_home.nil?
        raise <<EOS
Set the environment variable ORACLE_HOME if Oracle Full Client.
Append the path of Oracle client libraries to #{OraConf.ld_envs[0]} if Oracle Instant Client.
EOS
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

    def get_libs(postfix1 = '', postfix2 = "")
      print("Running make for $ORACLE_HOME/rdbms/demo/demo_rdbms#{postfix1}.mk (build#{postfix2}) ...")
      STDOUT.flush

      make_opt = "CC='echo MARKER' EXE=/dev/null ECHODO=echo ECHO=echo GENCLNTSH='echo genclntsh'"
      if @cc_is_gcc && /solaris/ =~ RUBY_PLATFORM
        # suggested by Brian Candler.
        make_opt += " KPIC_OPTION= NOKPIC_CCFLAGS#{postfix2}="
      end

      command = "|make -f #{@oracle_home}/rdbms/demo/demo_rdbms#{postfix1}.mk build#{postfix2} #{make_opt}"
      marker = /^\s*MARKER/
      echo = /^\s*echo/
      libs = nil
      Logging::open do
        puts command
        open(command, "r") do |f|
          while line = f.gets
            puts line
            line.chomp!
            line = $' while line =~ echo
            if line =~ marker
              # found a line where a compiler runs.
              libs = $'
              libs.gsub!(/-o\s+\/dev\/null/, "")
              libs.gsub!(/-o\s+build/, "")
            end
          end
        end
      end
      raise 'Cannot get proper libs.' if libs.nil?
      print("OK\n")

      case RUBY_PLATFORM
      when /hpux/
        if @cc_is_gcc
          # strip +DA2.0W, +DS2.0, -Wl,+s, -Wl,+n
          libs.gsub!(/\+DA\S+(\s)*/, "")
          libs.gsub!(/\+DS\S+(\s)*/, "")
          libs.gsub!(/-Wl,\+[sn](\s)*/, "")
        end
        libs.gsub!(/ -Wl,/, " ")
      end

      # remove object files from libs.
      objs = []
      libs.gsub!(/\S+\.o\b/) do |obj|
        objs << obj
        ""
      end
      # change object files to an archive file to work around.
      if objs.length > 0
        Logging::open do
          puts "change object files to an archive file."
          command = Config::CONFIG["AR"] + " cru oracle_objs.a " + objs.join(" ")
          puts command
          system(command)
          libs = "oracle_objs.a " + libs
        end
      end
      libs
    end # get_libs
  end
end

# OraConf for Instant Client
class OraConfIC < OraConf
  def initialize(ic_dir)
    init

    if ic_dir.is_a? String
      if ic_dir =~ /^\/usr\/lib\/oracle\/(\d+\.\d+\.\d+\.\d+)\/client\/lib/
        # rpm package
        lib_dir = ic_dir
        inc_dir = "/usr/include/oracle/#{$1}/client"
      else
        # zip package
        lib_dir = ic_dir
        inc_dir = "#{ic_dir}/sdk/include"
      end
    else
      # rpm package
      lib_dirs = Dir.glob("/usr/lib/oracle/*/client/lib")
      if lib_dirs.empty?
        raise 'Oracle Instant Client not found at /usr/lib/oracle/*/client/lib'
      end
      lib_dir = lib_dirs.sort[-1]
      inc_dir = lib_dir.gsub(%r{^/usr/lib/oracle/(.*)/client/lib}, "/usr/include/oracle/\\1/client")
    end

    @version = "1010"
    if RUBY_PLATFORM =~ /mswin32|cygwin|mingw32|bccwin32/ # when Windows
      unless File.exist?("#{ic_dir}/sdk/lib/msvc/oci.lib")
        raise <<EOS
Could not compile with Oracle instant client.
#{ic_dir}/sdk/lib/msvc/oci.lib could not be found.
EOS
        raise 'failed'
      end
      @cflags = " \"-I#{inc_dir}\""
      @cflags += " -D_int64=\"long long\"" if RUBY_PLATFORM =~ /cygwin/
      @libs = get_libs("#{ic_dir}/sdk")
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
      unless File.exists?("#{lib_dir}/libclntsh.#{so_ext}")
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
      @libs = " -L#{lib_dir} -lclntsh "
    end
    unless File.exists?("#{inc_dir}/oci.h")
          raise <<EOS
'#{inc_dir}/oci.h' does not exist.
Install 'Instant Client SDK'.
EOS
    end
    $CFLAGS += @cflags
    return if try_link_oci()

    if RUBY_PLATFORM =~ /i.*-darwin/
      open('mkmf.log', 'r') do |f|
        while line = f.gets
          if line.include? 'cputype (18, architecture ppc) does not match cputype (7)'
            raise <<EOS
Oracle doesn't support intel mac.
  http://blade.nagaokaut.ac.jp/cgi-bin/scat.rb/ruby/ruby-talk/223854

There are three solutions:
1. Compile ruby as ppc binary.
2. Wait until Oracle releases mac intel binary.
3. Use a third-party ODBC driver and ruby-odbc instead.
     http://www.actualtechnologies.com/
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
