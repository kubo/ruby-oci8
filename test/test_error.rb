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

  def test_sql_parse_error
    sql = 'select * from'
    begin
      @conn.exec(sql) # raises "ORA-00903: invalid table name"
    rescue OCIException
      assert_instance_of(OCIError, $!)
      assert_match(/^ORA-00903: /, $!.to_s)
      assert_equal(903, $!.code)
      assert_equal(13, $!.parse_error_offset)
      assert_equal(13, $!.parseErrorOffset) # barkward compatibility
      assert_equal(sql, $!.sql)
    end
  end

  def test_plsql_parse_error
    sql = <<EOS
BEGIN
  SELECT dummy INTO l_dummy FROM dual WHERE 1=2;
END;
EOS
    begin
      @conn.exec(sql) # raises "ORA-06550: line 2, column 21"
    rescue OCIException
      assert_instance_of(OCIError, $!)
      assert_match(/^ORA-06550: /, $!.to_s)
      assert_equal(6550, $!.code)
      assert_equal(26, $!.parse_error_offset)
      assert_equal(26, $!.parseErrorOffset) # barkward compatibility
      assert_equal(sql, $!.sql)
    end
  end

  def test_plsql_error_in_execution
    sql = <<EOS
DECLARE
  l_dummy VARCHAR2(50);
BEGIN
  SELECT * INTO l_dummy
    FROM (SELECT DUMMY FROM DUAL
          UNION ALL
          SELECT DUMMY FROM DUAL);
END;
EOS
    begin
      @conn.exec(sql) # raises "ORA-01422: exact fetch returns more than requested number of rows"
    rescue OCIException
      assert_instance_of(OCIError, $!)
      assert_match(/^ORA-01422: /, $!.to_s)
      assert_equal(1422, $!.code)
      assert_equal(0, $!.parse_error_offset)
      assert_equal(0, $!.parseErrorOffset) # barkward compatibility
      assert_equal(sql, $!.sql)
    end
  end

  def test_nodata
    sql = <<EOS
DECLARE
  l_dummy VARCHAR2(50);
BEGIN
  SELECT dummy INTO l_dummy FROM dual WHERE 1=2;
END;
EOS
    begin
      @conn.exec(sql) # raises "ORA-01403: no data found"
    rescue OCIException
      assert_instance_of(OCINoData, $!)
      assert_match(/^ORA-01403: /, $!.to_s)
      assert_equal(1403, $!.code)
      assert_equal(0, $!.parse_error_offset)
      assert_equal(0, $!.parseErrorOffset) # barkward compatibility
      assert_equal(sql, $!.sql)
    end
  end
end
