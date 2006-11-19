require 'oci8'
require 'test/unit'
require './config'

class TestOCI8 < Test::Unit::TestCase

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
  end

  def teardown
    @conn.logoff
  end

  def test_rename
    begin
      @conn.exec("DROP TABLE test_table")
      @conn.exec("DROP TABLE test_rename_table")
    rescue OCIError
      raise if $!.code != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_rename_table
  (C CHAR(10) NOT NULL)
EOS
    @conn.exec(sql)
    @conn.exec("RENAME test_rename_table TO test_table")
  end

  def test_long_type
    begin
      @conn.exec("DROP TABLE test_table")
    rescue OCIError
      raise if $!.code != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL)
EOS
    @conn.exec(sql)
    @conn.exec("SELECT column_name, data_default FROM all_tab_columns where owner = '#{$dbuser}' and table_name = 'TEST_TABLE'")
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
   D1 DATE, D2 DATE, D3 DATE, D4 DATE,
   INT NUMBER(30), BIGNUM NUMBER(30))
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)
    cursor = @conn.parse("INSERT INTO test_table VALUES (:C, :V, :N, :D1, :D2, :D3, :D4, :INT, :BIGNUM)")
    1.upto(10) do |i|
      if i == 1
	dt = [nil, OraDate]
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
      end
      cursor.exec(format("%10d", i * 10), i.to_s, i, dt, dt, dt, dt, i, i)
    end
    cursor.close
    cursor = @conn.parse("SELECT * FROM test_table ORDER BY c")
    cursor.define(5, Time) # define 5th column as Time
    cursor.define(6, Date) # define 6th column as Date
    cursor.define(7, DateTime) # define 7th column as DateTime
    cursor.define(8, Integer) # define 8th column as Integer
    cursor.define(9, Bignum) # define 9th column as Bignum
    cursor.exec
    assert_equal(["C", "V", "N", "D1", "D2", "D3", "D4", "INT", "BIGNUM"], cursor.get_col_names)
    1.upto(10) do |i|
      rv = cursor.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
      if i == 1
	assert_nil(rv[3])
	assert_nil(rv[4])
	assert_nil(rv[5])
	assert_nil(rv[6])
      else
        tm = Time.local(2000 + i, 8, 3, 23, 59, 59)
	dt = Date.civil(2000 + i, 8, 3)
	dttm = DateTime.civil(2000 + i, 8, 3, 23, 59, 59, Time.now.utc_offset.to_r/86400)
	assert_equal(dttm, rv[3])
	assert_equal(tm, rv[4])
	assert_equal(dt, rv[5])
	assert_equal(dttm, rv[6])
      end
      assert_equal(i, rv[7])
      assert_equal(i, rv[8])
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
	dttm = DateTime.civil(2000 + i, 8, 3, 23, 59, 59, Time.now.utc_offset.to_r/86400)
        tm = Time.local(2000 + i, 8, 3, 23, 59, 59)
	dt = Date.civil(2000 + i, 8, 3)
	assert_equal(dttm, rv[3])
	assert_equal(tm, rv[4])
	assert_equal(dt, rv[5])
      end
      assert_equal(i, rv[6])
      assert_equal(i, rv[7])
    end
    assert_nil(cursor.fetch)
    cursor.close
    @conn.exec("DROP TABLE test_table")
  end

end # TestOCI8
