# -*- ruby -*-
#
# To make a pure ruby gems package:
#   gem build ruby-oci8.gemspec
#
# To make a binary gems package:
#   gem build ruby-oci8.gemspec -- current
#
require 'fileutils'

build_args = if (defined? Gem::Command and Gem::Command.respond_to? :build_args)
               Gem::Command.build_args
             else
               # for old rubygems
               ARGV.include?("--") ? ARGV[(ARGV.index("--") + 1)...ARGV.size] : []
             end

if build_args.size > 0
  gem_platform = build_args[0]
else
  gem_platform = Gem::Platform::RUBY
end

spec = Gem::Specification.new do |s|
  s.name = 'ruby-oci8'
  s.version = File.read('VERSION').strip
  s.summary = 'Ruby interface for Oracle using OCI8 API'
  s.email = 'kubo@jiubao.org'
  s.homepage = 'http://ruby-oci8.rubyforge.org'
  s.rubyforge_project = 'ruby-oci8'
  s.description = <<EOS
ruby-oci8 is a ruby interface for Oracle using OCI8 API. It is available with Oracle8, Oracle8i, Oracle9i, Oracle10g and Oracle Instant Client.
EOS
  s.has_rdoc = 'yard'
  s.authors = ['KUBO Takehiro']
  s.platform = gem_platform
  files = File.read('dist-files').split("\n")
  if gem_platform == Gem::Platform::RUBY
    s.extensions << 'ext/oci8/extconf.rb'
    s.required_ruby_version = '>= 1.8.0'
  else
    so_files = Dir.glob('ext/oci8/oci8lib_*.so')
    has_1_8 = so_files.include? 'ext/oci8/oci8lib_18.so'
    has_1_9_1 = so_files.include? 'ext/oci8/oci8lib_191.so'
    if has_1_8 and has_1_9_1
      puts 'Binary gem for ruby 1.8 and 1.9.1'
      s.required_ruby_version = '>= 1.8.0'
    elsif has_1_8 and !has_1_9_1
      puts 'Binary gem for ruby 1.8'
      s.required_ruby_version = '~> 1.8.0'
    elsif !has_1_8 and has_1_9_1
      puts 'Binary gem for ruby 1.9.1'
      s.required_ruby_version = '~> 1.9.1'
    else
      raise "No compiled binary are found. Run make in advance."
    end
    # add map files to analyze a core (minidump) file.
    so_files << 'ext/oci8/oci8lib_18.map' if has_1_8 and File.exists? 'ext/oci8/oci8lib_18.map'
    so_files << 'ext/oci8/oci8lib_191.map' if has_1_9_1 and File.exists? 'ext/oci8/oci8lib_191.map'

    FileUtils.copy so_files, 'lib', :preserve => true
    files.reject! do |fname|
      fname =~ /^ext/
    end
    so_files.each do |fname|
      files << 'lib/' + File.basename(fname)
    end
    files << 'lib/oci8.rb'
  end
  s.require_paths = ['lib', 'ext/oci8']
  s.files = files
  s.test_files = 'test/test_all.rb'
  s.extra_rdoc_files = ['README.md']
end
