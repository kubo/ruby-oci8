require 'oci8'
require 'test/unit'
require File.dirname(__FILE__) + '/config'

class TestError < Test::Unit::TestCase
  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  attr_reader :code, :sql, :parse_error_offset

  def test_error
    begin
      @conn.exec('select * from') # raises "ORA-00903: invalid table name"
    rescue OCIException
      assert_instance_of(OCIError, $!)
      assert_match(/^ORA-00903: /, $!.to_s)
      assert_equal(903, $!.code)
      assert_equal(13, $!.parse_error_offset)
      assert_equal(13, $!.parseErrorOffset) # barkward compatibility
      assert_equal('select * from', $!.sql)
    end
  end
end
