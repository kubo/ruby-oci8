# -*- ruby -*-
#
# To make a pure ruby gems package:
#   gem build ruby-oci8.gemspec
#
# To make a binary gems package:
#   gem build ruby-oci8.gemspec -- current
#
require 'fileutils'

if ARGV.include?("--") and ARGV[(ARGV.index("--") + 1)] == 'current'
  gem_platform = 'current'
else
  gem_platform = Gem::Platform::RUBY
end

if defined? OCI8
  oci8_version = OCI8::VERSION
else
  # Load oci8/version.rb temporarily and delete OCI8.
  # Some 'bundler' tasks load this file before 'require "oci8"'
  # and fail with 'TypeError: superclass mismatch for class OCI8.'
  Kernel.load File.dirname(__FILE__) + '/lib/oci8/version.rb'
  oci8_version = OCI8::VERSION
  Object.send(:remove_const, :OCI8)
end

spec = Gem::Specification.new do |s|
  s.name = 'ruby-oci8'
  s.version = oci8_version
  s.summary = 'Ruby interface for Oracle using OCI8 API'
  s.email = 'kubo@jiubao.org'
  s.homepage = 'http://www.rubydoc.info/github/kubo/ruby-oci8'
  s.description = <<EOS
ruby-oci8 is a ruby interface for Oracle using OCI8 API. It is available with Oracle 10g or later including Oracle Instant Client.
EOS
  s.authors = ['Kubo Takehiro']
  s.platform = gem_platform
  s.license = 'BSD-2-Clause'
  files = File.read('dist-files').split("\n")
  if gem_platform == Gem::Platform::RUBY
    s.extensions << 'ext/oci8/extconf.rb'
    s.required_ruby_version = '>= 1.9.1'
  else
    so_files = Dir.glob('ext/oci8/oci8lib_*.so')
    so_vers = so_files.collect do |file|
      $1 if /ext\/oci8\/oci8lib_(\S+).so/ =~ file
    end.sort

    # add map files to analyze a core (minidump) file.
    so_vers.each do |ver|
      map_file = 'ext/oci8/oci8lib_#{ver}.map'
      so_files << map_file if File.exist? map_file
    end

    # least version in so_vers
    so_vermin = so_vers.collect do |ver|
      "#$1.#$2.#{$3||'0'}" if /^(?:rbx)?(\d)(\d)(\d)?/ =~ ver
    end.sort.first

    case so_vers.length
    when 0
      raise "No compiled binary are found. Run make in advance."
    when 1
      puts "Binary gem for ruby #{so_vers.first}"
      s.required_ruby_version = "~> #{so_vermin}"
    else
      puts "Binary gem for ruby #{so_vers.join(', ')}"
      s.required_ruby_version = ">= #{so_vermin}"
    end

    FileUtils.copy so_files, 'lib', :preserve => true
    files.reject! do |fname|
      fname =~ /^ext/
    end
    so_files.each do |fname|
      files << 'lib/' + File.basename(fname)
    end
    files << 'lib/oci8.rb'
  end
  s.require_paths = ['lib']
  s.files = files
  s.test_files = 'test/test_all.rb'
  s.extra_rdoc_files = ['README.md']
end
