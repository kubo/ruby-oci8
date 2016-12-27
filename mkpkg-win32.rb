$ruby_oci8_version = open('lib/oci8/version.rb').readlines.collect do |line|
  (line =~ /VERSION = "([^"]*)"/) && $1
end.compact[0]

case ARGV[0]
when '32'
  $platform = 'x86'
  $ruby_base_dirs = []

  $ruby_base_dirs =
    [
     'c:\ruby\ruby-1.9.1-p430-i386-mingw32',
     'c:\ruby\ruby-2.0.0-p0-i386-mingw32',
     'c:\ruby\ruby-2.1.3-i386-mingw32',
     'c:\ruby\ruby-2.2.1-i386-mingw32',
     'c:\ruby\ruby-2.3.0-i386-mingw32',
     'c:\ruby\ruby-2.4.0-i386-mingw32',
    ]

  $oracle_path = 'c:\oracle\instantclient_12_1-win32'
  $devkit_tdm_path = ['c:\ruby\devkit-tdm-32\bin', 'c:\ruby\devkit-tdm-32\mingw\bin']
  $devkit_mingw64_path = ['c:\ruby\devkit-mingw64-32\bin', 'c:\ruby\devkit-mingw64-32\mingw\bin']

  $build_ruby_dirs =
    [
     ['c:\ruby\ruby-1.9.1-p430-i386-mingw32', $devkit_tdm_path],
     ['c:\ruby\ruby-2.0.0-p0-i386-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.1.3-i386-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.2.1-i386-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.3.0-i386-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.4.0-i386-mingw32', $devkit_mingw64_path],
    ]

when '64'
  $platform = 'x64'
  $ruby_base_dirs =
    [
     # RubyInstaller <URL:http://rubyinstaller.org>
     'c:\ruby\ruby-2.0.0-p0-x64-mingw32',
     'c:\ruby\ruby-2.1.3-x64-mingw32',
     'c:\ruby\ruby-2.2.1-x64-mingw32',
     'c:\ruby\ruby-2.3.0-x64-mingw32',
     'c:\ruby\ruby-2.4.0-x64-mingw32',
    ]

  $oracle_path = 'c:\oracle\instantclient_12_1-win64'
  $devkit_mingw64_path = ['c:\ruby\devkit-mingw64-64\bin', 'c:\ruby\devkit-mingw64-64\mingw\bin']
  $build_ruby_dirs =
    [
     ['c:\ruby\ruby-2.0.0-p0-x64-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.1.3-x64-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.2.1-x64-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.3.0-x64-mingw32', $devkit_mingw64_path],
     ['c:\ruby\ruby-2.4.0-x64-mingw32', $devkit_mingw64_path],
    ]
else
  puts "#{ARGV[0]} (32|64)"
  exit 0
end

$gem_package = "ruby-oci8-#{$ruby_oci8_version}-#{$platform}-mingw32.gem"
ENV['PATH'] = $oracle_path + ';' + ENV['PATH']

def prepend_path(basedir, others = [])
  orig_env = ENV['PATH']
  begin
    ENV['PATH'] = basedir + '\bin;' + others.collect{|p| p + ';'}.join('') + orig_env
    yield
  ensure
    ENV['PATH'] = orig_env
  end
end

def ruby_oci8_gem_is_installed?
  open("|gem list -d ruby-oci8", 'r') do |f|
    while line = f.gets
      if line =~ /ruby-oci8/
        return true
      end
    end
  end
  false
end

def run_cmd(cmd, raise_on_error = true)
  system(cmd)
  raise $?.to_s if raise_on_error && $?.to_i != 0
end

def make_gem
  Dir.glob('ext/oci8/oci8lib_*.so') do |file|
    File.delete file
  end

  $build_ruby_dirs.each do |ruby, devkit|
    prepend_path(ruby, devkit) do
      run_cmd("ruby setup.rb config -- --with-runtime-check")
      run_cmd("ruby setup.rb setup")
    end
  end

  ruby_for_gem_build = $build_ruby_dirs[0][0]
  prepend_path(ruby_for_gem_build) do
    run_cmd("gem build ruby-oci8.gemspec -- current")
  end
end

def install_and_test
  $ruby_base_dirs.each do |base_dir|
    prepend_path(base_dir) do
      puts "=================================================="
      puts "       #{base_dir}"
      puts "=================================================="
      STDOUT.flush
      if ruby_oci8_gem_is_installed?
        run_cmd("gem uninstall ruby-oci8 --all")
        if ruby_oci8_gem_is_installed?
          raise "ruby-oci8 gem in #{base_dir} could not be uninstalled."
        end
      end
      run_cmd("gem install ./#{$gem_package} --no-rdoc --no-ri --local")
      run_cmd("ruby -rubygems -I. test/test_all.rb", false)
    end
  end
end

make_gem
ENV['TNS_ADMIN'] = 'c:\oraclexe\app\oracle\producet\11.2.0\server\network\admin'
ENV['LOCAL'] = 'XE'
install_and_test
