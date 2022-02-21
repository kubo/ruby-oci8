require 'fileutils'

$ruby_oci8_version = open('lib/oci8/version.rb').readlines.collect do |line|
  (line =~ /VERSION = "([^"]*)"/) && $1
end.compact[0]

$platform = ARGV[0]

def run_cmd(cmd, raise_on_error = true)
  system(cmd)
  raise $?.to_s if raise_on_error && $?.to_i != 0
end

case $platform
when 'x86-mingw32'
  $ruby_base_dirs = []

  $ruby_base_dirs =
    [
     'c:\ruby\rubyinstaller-2.5.0-1-x86',
     'c:\ruby\rubyinstaller-2.6.0-1-x86',
     'c:\ruby\rubyinstaller-2.7.0-1-x86',
     'c:\ruby\rubyinstaller-3.0.0-1-x86',
     'c:\ruby\rubyinstaller-3.1.0-1-x86',
    ]

  $oracle_path = 'c:\oracle\instantclient_19_9-win32'
  $ridk_setup = 'ridk enable'

  $build_ruby_dirs =
    [
     ['c:\ruby\rubyinstaller-2.5.0-1-x86', $ridk_setup, :disable_fortify_source],
     ['c:\ruby\rubyinstaller-2.6.0-1-x86', $ridk_setup],
     ['c:\ruby\rubyinstaller-2.7.0-1-x86', $ridk_setup],
     ['c:\ruby\rubyinstaller-3.0.0-1-x86', $ridk_setup],
     ['c:\ruby\rubyinstaller-3.1.0-1-x86', $ridk_setup],
    ]

when 'x64-mingw32'
  $ruby_base_dirs =
    [
     # RubyInstaller <URL:http://rubyinstaller.org>
     'c:\ruby\rubyinstaller-2.5.0-1-x64',
     'c:\ruby\rubyinstaller-2.6.0-1-x64',
     'c:\ruby\rubyinstaller-2.7.0-1-x64',
     'c:\ruby\rubyinstaller-3.0.0-1-x64',
     'c:\ruby\rubyinstaller-3.1.0-1-x64',
    ]

  $oracle_path = 'c:\oracle\instantclient_19_9-win64'
  $ridk_setup = 'ridk enable'
  $build_ruby_dirs =
    [
     ['c:\ruby\rubyinstaller-2.5.0-1-x64', $ridk_setup, :disable_fortify_source],
     ['c:\ruby\rubyinstaller-2.6.0-1-x64', $ridk_setup],
     ['c:\ruby\rubyinstaller-2.7.0-1-x64', $ridk_setup],
     ['c:\ruby\rubyinstaller-3.0.0-1-x64', $ridk_setup],
     ['c:\ruby\rubyinstaller-3.1.0-1-x64', $ridk_setup],
    ]

when 'x64-mingw-ucrt'
  $ruby_base_dirs =
    [
     # RubyInstaller <URL:http://rubyinstaller.org>
     'c:\ruby\rubyinstaller-3.1.0-1-x64',
    ]

  $oracle_path = 'c:\oracle\instantclient_19_9-win64'
  $ridk_setup = 'ridk enable'
  $build_ruby_dirs =
    [
     ['c:\ruby\rubyinstaller-3.1.0-1-x64', $ridk_setup],
    ]

when 'all'
  files = File.read('dist-files').split("\n")
  ['x86-mingw32', 'x64-mingw32', 'x64-mingw-ucrt'].each do |platform|
    FileUtils.rm_rf platform
    files.each do |file|
      dest = File.join(platform, file)
      FileUtils.mkdir_p File.dirname(dest)
      FileUtils.copy(file, dest)
    end
    Dir.chdir platform do
      run_cmd("#{RbConfig.ruby} ../mkpkg-win32.rb #{platform}")
    end
  end
  exit 0
else
  puts "#{ARGV[0]} (x86-mingw32|x64-mingw32|x64-mingw-ucrt|all)"
  exit 0
end

$gem_package = "ruby-oci8-#{$ruby_oci8_version}-#{$platform}.gem"
ENV['PATH'] = $oracle_path + ';c:\Windows\System32'

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

def make_gem
  Dir.glob('ext/oci8/oci8lib_*.so') do |file|
    File.delete file
  end

  $build_ruby_dirs.each do |ruby, dev_setup, disable_fortify_source|
    prepend_path(ruby) do
      fix_makefile = ''
      if disable_fortify_source
        fix_makefile += ' && sed -i "s/-D_FORTIFY_SOURCE=2/-D_FORTIFY_SOURCE=0/" ext\oci8\Makefile'
      end
      run_cmd("#{dev_setup} && ruby setup.rb config -- --with-runtime-check #{fix_makefile} && ruby setup.rb setup")
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
      begin
        run_cmd("gem install ./#{$gem_package} --local")
        run_cmd('ruby -roci8 -e "OCI8.properties[:tcp_keepalive_time] = 300"')
        run_cmd("ruby -I. test/test_all.rb", false)
      ensure
        run_cmd("gem uninstall ruby-oci8 --platform #{$platform}")
      end
    end
  end
end

make_gem
ENV['LOCAL'] ||= '//172.17.0.2/ORCLPDB1'
ENV['NLS_LANG'] ||= 'American_America.AL32UTF8'
install_and_test
