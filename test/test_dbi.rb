require 'dbi'
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestDBI < RUNIT::TestCase

  def setup
    @dbh = DBI.connect("dbi:OCI8:#{$dbname}", $dbuser, $dbpass, 'AutoCommit' => false)
  end

  def test_select
    begin
      @dbh.do("DROP TABLE test_table")
    rescue DBI::DatabaseError
      raise if $!.err != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL,
   V VARCHAR2(20),
   N NUMBER(10, 2),
   D DATE)
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @dbh.do(sql)
    sth = @dbh.prepare("INSERT INTO test_table VALUES (?, ?, ?, ?)")
    1.upto(10) do |i|
      sth.execute(format("%10d", i * 10), i.to_s, i, nil)
    end
    sth = @dbh.execute("SELECT * FROM test_table ORDER BY c")
    assert_equal(["C", "V", "N", "D"], sth.column_names)
    1.upto(10) do |i|
      rv = sth.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
    end
    assert_nil(sth.fetch)
    assert_equal(10, @dbh.select_one("SELECT COUNT(*) FROM test_table")[0])
    @dbh.rollback()
    assert_equal(0, @dbh.select_one("SELECT COUNT(*) FROM test_table")[0])
    @dbh.do("DROP TABLE test_table")
  end

  def test_ref_cursor
    begin
      @dbh.do("DROP TABLE test_table")
    rescue DBI::DatabaseError
      raise if $!.err != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL,
   V VARCHAR2(20),
   N NUMBER(10, 2),
   D DATE)
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @dbh.do(sql)
    sth = @dbh.prepare("INSERT INTO test_table VALUES (?, ?, ?, ?)")
    1.upto(10) do |i|
      sth.execute(format("%10d", i * 10), i.to_s, i, nil)
    end
    # get a ref cursor
    plsql = @dbh.execute("BEGIN OPEN ? FOR SELECT * FROM test_table ORDER BY c; END;", DBI::StatementHandle)
    sth = plsql.func(:bind_value, 1)
    assert_equal(["C", "V", "N", "D"], sth.column_names)
    1.upto(10) do |i|
      rv = sth.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
    end
    @dbh.rollback()
    @dbh.do("DROP TABLE test_table")
  end

  def test_define
    begin
      @dbh.do("DROP TABLE test_table")
    rescue DBI::DatabaseError
      raise if $!.err != 942 # table or view does not exist
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
    @dbh.do(sql)
    sth = @dbh.prepare("INSERT INTO test_table VALUES (:C, :V, :N, :D1, :D2, :D3, :D4, :INT, :BIGNUM)")
    1.upto(10) do |i|
      if i == 1
	dt = nil
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
      end
      sth.execute(format("%10d", i * 10), i.to_s, i, dt, dt, dt, dt, i, i)
    end
    sth.finish
    sth = @dbh.prepare("SELECT * FROM test_table ORDER BY c")
    sth.func(:define, 5, Time) # define 5th column as Time
    sth.func(:define, 6, Date) # define 6th column as Date
    sth.func(:define, 7, DateTime) if defined? DateTime # define 7th column as DateTime
    sth.func(:define, 8, Integer) # define 8th column as Integer
    sth.func(:define, 9, Bignum) # define 9th column as Bignum
    sth.execute
    assert_equal(["C", "V", "N", "D1", "D2", "D3", "D4", "INT", "BIGNUM"], sth.column_info.collect {|cl| cl.name})
    1.upto(10) do |i|
      rv = sth.fetch
      assert_equal(format("%10d", i * 10), rv[0])
      assert_equal(i.to_s, rv[1])
      assert_equal(i, rv[2])
      if i == 1
	assert_nil(rv[3])
	assert_nil(rv[4])
	assert_nil(rv[5])
	assert_nil(rv[6])
      else
	dt = OraDate.new(2000 + i, 8, 3, 23, 59, 59)
	assert_equal(dt, rv[3])
	assert_equal(dt.to_time, rv[4])
	assert_equal(dt.to_date, rv[5])
	assert_equal(dt.to_datetime, rv[6]) if defined? DateTime
	assert_instance_of(Time, rv[4])
	assert_instance_of(Date, rv[5])
	assert_instance_of(DateTime, rv[6]) if defined? DateTime
      end
      assert_equal(i, rv[7])
      assert_equal(i, rv[8])
    end
    assert_nil(sth.fetch)
    sth.finish
    @dbh.execute("DROP TABLE test_table")
  end

  def test_bind_dbi_data_type
    inval = DBI::Date.new(2004, 3, 20)
    sth = @dbh.execute("BEGIN ? := ?; END;", DBI::Date, inval)
    outval = sth.func(:bind_value, 1)
    assert_instance_of(DBI::Date, outval)
    assert_equal(inval.to_time, outval.to_time)

    inval = DBI::Timestamp.new(2004, 3, 20, 18, 26, 33)
    sth = @dbh.execute("BEGIN ? := ?; END;", DBI::Timestamp, inval)
    outval = sth.func(:bind_value, 1)
    assert_instance_of(DBI::Timestamp, outval)
    assert_equal(inval.to_time, outval.to_time)
  end

end # TestDBI

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestDBI.suite())
end
