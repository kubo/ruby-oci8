# -*- ruby -*-
#
# To make a pure ruby gems package:
#   gem build ruby-oci8.gemspec
#
# To make a binary gems package:
#   gem build ruby-oci8.gemspec -- current
#

if ARGV.size > 3
   gem_platform = ARGV[3]
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
  s.autorequire = 'oci8'
  s.has_rdoc = true
  s.required_ruby_version = '~> 1.8.0'
  s.authors = ['KUBO Takehiro']
  s.platform = gem_platform
  files = File.read('dist-files').split("\n")
  if gem_platform == Gem::Platform::RUBY
    s.require_paths = ['lib']
    s.extensions << 'ext/oci8/extconf.rb'
  else
    s.require_paths = ['lib', 'ext/oci8']
    # exclude C source files.
    files.delete_if do |file|
      file[0, 4] == 'ext/'
    end
    # check files created by a make command.
    ['ext/oci8/oci8lib.so', 'lib/oci8.rb'].each do |file|
      raise <<EOS unless File.exist?(file)
#{file} doesn't exist. Run make in advance.
EOS
      #'
      files << file
    end
  end
  s.files = files
  s.test_files = 'test/test_all.rb'
  s.rdoc_options = ['--main', 'README', '--exclude', 'ext/*']
  s.extra_rdoc_files = ['README']
end
