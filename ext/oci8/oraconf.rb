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
        raise Win32APIException.new("Win32::RegOpenKeyExA",code)
      end
      hkey = result.unpack('L')[0]
      begin
        lpcbData = [0].pack('L')
        code = RegQueryValueExA.call(hkey, name, nil, nil, nil, lpcbData)
        if code == ERROR_FILE_NOT_FOUND
          return nil
        elsif code != ERROR_SUCCESS
          raise Win32APIException.new("Win32::RegQueryValueExA",code)
        end
        len = lpcbData.unpack('L')[0]
        lpType = "\0\0\0\0"
        lpData = "\0"*len
        lpcbData = [len].pack('L')
        code = RegQueryValueExA.call(hkey, name, nil, lpType, lpData, lpcbData)
        if code != ERROR_SUCCESS
          raise Win32APIException.new("Win32::RegQueryValueExA",code)
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

  def initialize(oracle_home = nil)
    original_CFLAGS = $CFLAGS
    original_defs = $defs
    original_libs = $libs
    begin
      @cc_is_gcc = get_cc_is_gcc_or_not()
      @oracle_home = get_home(oracle_home)
      @version = get_version()
      @cflags = get_cflags()
      @libs = get_libs()

      $CFLAGS += ' ' + @cflags
      $libs += " -L#{CONFIG['libdir']} " + @libs
      unless have_func("OCIInitialize", "oci.h")
        ok = false
        case RUBY_PLATFORM
        when /solaris/, /hpux/
          case @version
          when /^9..$/
            print("retry with postfix 32.\n")
            $libs = original_libs
            @libs = get_libs('', '32')
            $libs += " -L#{CONFIG['libdir']} " + @libs
            ok = true if have_func("OCIInitialize", "oci.h")
          when /^10..$/
            print("retry with postfix 32.\n")
            $libs = original_libs
            @libs = get_libs('32', '')
            $libs += " -L#{CONFIG['libdir']} " + @libs
            ok = true if have_func("OCIInitialize", "oci.h")
          end
        end
        raise 'cannot compile OCI' unless ok
      end
    ensure
      $CFLAGS = original_CFLAGS
      $defs = original_defs
      $libs = original_libs
    end
  end

  private

  def get_cc_is_gcc_or_not
    # bcc defines __GNUC__. why??
    return false if RUBY_PLATFORM =~ /bccwin32/

    printf "checking for gcc... "
    STDOUT.flush
    if macro_defined?("__GNUC__", "")
      print "yes\n"
      return true
    else
      print "no\n"
      return false
    end
  end # cc_is_gcc

  def get_version
    print("Get the version of Oracle from SQL*Plus... ")
    STDOUT.flush
    version = nil
    sqlplus = get_local_registry('SOFTWARE\\ORACLE', 'EXECUTE_SQL') # Oracle 8 on Windows.
    sqlplus = 'sqlplus' if sqlplus.nil?                        # default
    dev_null = RUBY_PLATFORM =~ /mswin32|mingw32|bccwin32/ ? "nul" : "/dev/null"
    if RUBY_PLATFORM =~ /cygwin/
        oracle_home = @oracle_home.sub(%r{/cygdrive/([a-zA-Z])/}, "\\1:")
    else
        oracle_home = @oracle_home
    end
    Logging::open do
      open("|#{oracle_home}/bin/#{sqlplus} < #{dev_null}") do |f|
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

    def check_home_key(key)
      oracle_home = get_local_registry("SOFTWARE\\ORACLE#{key}", 'ORACLE_HOME')
      return nil if oracle_home.nil?
      sqlplus = "#{oracle_home}/bin/sqlplus.exe"
      print("checking for ORACLE_HOME(#{oracle_home})... ")
      STDOUT.flush
      if File.exist?(sqlplus)
        puts("yes")
        oracle_home
      else
        puts("no")
        nil
      end
    end

    def get_home(oracle_home)
      if oracle_home.nil?
        oracle_home = ENV['ORACLE_HOME']
      end
      if oracle_home.nil?
        oracle_home = check_home_key('')
        if oracle_home.nil?
          0.upto 9 do |num|
            oracle_home = check_home_key("\\HOME#{num.to_i}")
            break if oracle_home
          end
          raise 'Cannot get ORACLE_HOME. Please set the environment valiable ORACLE_HOME.' if oracle_home.nil?
        end
      end
      if RUBY_PLATFORM =~ /cygwin/
         oracle_home.sub!(/^([a-zA-Z]):/, "/cygdrive/\\1")
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
      if RUBY_PLATFORM =~ /cygwin/
        " -I#{oci_base_dir}/INCLUDE -D_int64=\"long long\""
      else
        " -I#{oci_base_dir}/INCLUDE"
      end
    end

    def get_libs
      case RUBY_PLATFORM
      when /cygwin/
        open("OCI.def", "w") do |f|
          f.puts("EXPORTS")
          open("|nm #{oci_base_dir}/LIB/MSVC/OCI.LIB") do |r|
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
        lib = "#{oci_base_dir}/LIB/BORLAND/OCI.LIB"
        return lib.tr('/', '\\') if File.exist?(lib)
        print <<EOS

#{lib} does not exist.

Your Oracle may not support Borland C++.
If you want to run this module, run the following command at your own risk.
  cd #{oci_base_dir.tr('/', '\\')}\\LIB
  mkdir Borland
  cd Borland
  coff2omf ..\\MSVC\\OCI.LIB OCI.LIB

EOS
        exit 1
      else
        "#{oci_base_dir}/LIB/MSVC/OCI.LIB"
      end
    end

  else # when UNIX

    def get_home(oracle_home)
      oracle_home ||= ENV['ORACLE_HOME']
      raise 'Cannot get ORACLE_HOME. Please set the environment valiable ORACLE_HOME.' if oracle_home.nil?
      oracle_home
    end

    def get_cflags
      cflags = ''
      ok = false
      original_CFLAGS = $CFLAGS
      begin
        for i in ["rdbms/demo", "rdbms/public", "network/public", "plsql/public"]
          cflags += " -I#{@oracle_home}/#{i}"
          $CFLAGS += " -I#{@oracle_home}/#{i}"
          print("try #{cflags}\n");
          if have_header("oci.h")
            ok = true
            break
          end
        end
        raise 'Cannot get proper cflags.' unless ok
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
