require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestOCI8 < RUNIT::TestCase

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
  end

  def teardown
    @conn.logoff
  end

  def test_select
    begin
      @conn.exec("DROP TABLE test_table")
    rescue OCIError
      raise if $!.code != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL,
   V VARCHAR2(20),
   N NUMBER(10, 2),
   D1 DATE, D2 DATE, D3 DATE,
   INT NUMBER(30), BIGNUM NUMBER(30))
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)
    cursor = @conn.parse("INSERT INTO test_table VALUES (:C, :V, :N, :D1, :D2, :D3, :INT, :BIGNUM)")
    1.upto(10) do |i|
      if i == 1
	dt = [nil, OraDate]
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
      end
      cursor.exec(format("%10d", i * 10), i.to_s, i, dt, dt, dt, i, i)
    end
    cursor.close
    cursor = @conn.parse("SELECT * FROM test_table ORDER BY c")
    cursor.define(5, Time) # define 5th column as Time
    cursor.define(6, Date) # define 6th column as Date
    cursor.define(7, Integer) # define 7th column as Integer
    cursor.define(8, Bignum) # define 8th column as Integer
    cursor.exec
    assert_equal(["C", "V", "N", "D1", "D2", "D3", "INT", "BIGNUM"], cursor.get_col_names)
    1.upto(10) do |i|
      rv = cursor.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
      if i == 1
	assert_nil(rv[3])
	assert_nil(rv[4])
	assert_nil(rv[5])
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
	assert_equal(dt, rv[3])
	assert_equal(dt.to_time, rv[4])
	assert_equal(dt.to_date, rv[5])
      end
      assert_equal(i, rv[6])
      assert_equal(i, rv[7])
    end
    assert_nil(cursor.fetch)
    cursor.close
    @conn.exec("DROP TABLE test_table")
  end

  def test_bind_cursor
    begin
      @conn.exec("DROP TABLE test_table")
    rescue OCIError
      raise if $!.code != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL,
   V VARCHAR2(20),
   N NUMBER(10, 2),
   D1 DATE, D2 DATE, D3 DATE,
   INT NUMBER(30), BIGNUM NUMBER(30))
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)
    cursor = @conn.parse("INSERT INTO test_table VALUES (:C, :V, :N, :D1, :D2, :D3, :INT, :BIGNUM)")
    1.upto(10) do |i|
      if i == 1
	dt = [nil, OraDate]
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
      end
      cursor.exec(format("%10d", i * 10), i.to_s, i, dt, dt, dt, i, i)
    end
    cursor.close
    plsql = @conn.parse("BEGIN OPEN :cursor FOR SELECT * FROM test_table ORDER BY c; END;")
    plsql.bind_param(':cursor', OCI8::Cursor)
    plsql.exec
    cursor = plsql[':cursor']
    cursor.define(5, Time) # define 5th column as Time
    cursor.define(6, Date) # define 6th column as Date
    cursor.define(7, Integer) # define 7th column as Integer
    cursor.define(8, Bignum) # define 8th column as Integer
    assert_equal(["C", "V", "N", "D1", "D2", "D3", "INT", "BIGNUM"], cursor.get_col_names)
    1.upto(10) do |i|
      rv = cursor.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
      if i == 1
	assert_nil(rv[3])
	assert_nil(rv[4])
	assert_nil(rv[5])
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
	assert_equal(dt, rv[3])
	assert_equal(dt.to_time, rv[4])
	assert_equal(dt.to_date, rv[5])
      end
      assert_equal(i, rv[6])
      assert_equal(i, rv[7])
    end
    assert_nil(cursor.fetch)
    cursor.close
    @conn.exec("DROP TABLE test_table")
  end

end # TestOCI8

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestOCI8.suite())
end
