require 'runit/testsuite'
require 'runit/cui/testrunner'

srcdir = File.dirname(__FILE__)

# Low-level API
require "#{srcdir}/test_oradate"
require "#{srcdir}/test_oranumber"
require "#{srcdir}/test_describe"
require "#{srcdir}/test_bind_time"
require "#{srcdir}/test_bind_raw"
if $test_clob
  require "#{srcdir}/test_clob"
end

# High-level API
require "#{srcdir}/test_break"
require "#{srcdir}/test_oci8"

# Ruby/DBI
begin
  require 'dbi'
  is_dbi_loaded = true
rescue LoadError
  is_dbi_loaded = false
end
if is_dbi_loaded
  require "#{srcdir}/test_dbi"
  if $test_clob
    require "#{srcdir}/test_dbi_clob"
  end
end

suite = RUNIT::TestSuite.new
ObjectSpace.each_object(Class) { |klass|
  if klass.ancestors.include?(RUNIT::TestCase)
    suite.add_test(klass.suite)
  end
}
#RUNIT::CUI::TestRunner.quiet_mode = true
RUNIT::CUI::TestRunner.run(suite)
