srcdir = File.dirname(__FILE__)

require 'oci8'
require 'test/unit'
require "#{srcdir}/config"

require "#{srcdir}/test_oradate"
require "#{srcdir}/test_oranumber"
require "#{srcdir}/test_bind_time"
require "#{srcdir}/test_bind_raw"
if $test_clob
  require "#{srcdir}/test_clob"
end

require "#{srcdir}/test_break"
require "#{srcdir}/test_oci8"
require "#{srcdir}/test_datetime"
require "#{srcdir}/test_connstr"
require "#{srcdir}/test_metadata"
require "#{srcdir}/test_array_dml"
require "#{srcdir}/test_rowid"

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

#Test::Unit::AutoRunner.run(true, true)
Test::Unit::AutoRunner.run()

