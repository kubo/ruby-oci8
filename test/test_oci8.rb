require 'oci8'
require File.dirname(__FILE__) + '/config'
require 'bigdecimal'
require 'rational'

class TestOCI8 < Minitest::Test

  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  def test_rename
    drop_table('test_table')
    drop_table('test_rename_table')
    sql = <<-EOS
CREATE TABLE test_rename_table
  (C CHAR(10) NOT NULL)
EOS
    @conn.exec(sql)
    @conn.exec("RENAME test_rename_table TO test_table")
    drop_table('test_rename_table')
  end

  def test_long_type
    clob_bind_type = OCI8::BindType::Mapping[:clob]
    blob_bind_type = OCI8::BindType::Mapping[:blob]
    initial_cunk_size = OCI8::BindType::Base.initial_chunk_size
    begin
      OCI8::BindType::Base.initial_chunk_size = 5
      @conn.prefetch_rows = LONG_TEST_DATA.size / 3
      drop_table('test_table')
      ascii_enc = Encoding.find('US-ASCII')
      0.upto(1) do |i|
        if i == 0
          @conn.exec("CREATE TABLE test_table (id number(38), long_column long, clob_column clob)")
          cursor = @conn.parse('insert into test_table values (:1, :2, :3)')
          cursor.bind_param(1, nil, Integer)
          cursor.bind_param(2, nil, :long)
          cursor.bind_param(3, nil, :clob)
          lob = OCI8::CLOB.new(@conn, '')
          enc = Encoding.default_internal || OCI8.encoding
        else
          @conn.exec("CREATE TABLE test_table (id number(38), long_raw_column long raw, blob_column blob)")
          cursor = @conn.parse('insert into test_table values (:1, :2, :3)')
          cursor.bind_param(1, nil, Integer)
          cursor.bind_param(2, nil, :long_raw)
          cursor.bind_param(3, nil, :blob)
          lob = OCI8::BLOB.new(@conn, '')
          enc = Encoding.find('ASCII-8BIT')
        end

        LONG_TEST_DATA.each_with_index do |data, index|
          cursor[1] = index
          cursor[2] = data
          if data.nil?
            cursor[3] = nil
          else
            lob.rewind
            lob.write(data)
            lob.size = data.size
            cursor[3] = lob
          end
          cursor.exec
        end
        cursor.close

        cursor = @conn.parse('SELECT * from test_table order by id')
        cursor.exec
        LONG_TEST_DATA.each_with_index do |data, index|
          row = cursor.fetch
          assert_equal(index, row[0])
          if data.nil?
            assert_nil(row[1])
            assert_nil(row[2])
          elsif data.empty?
            # '' is inserted to the long or long raw column as null.
            assert_nil(row[1])
            # '' is inserted to the clob or blob column as an empty clob.
            # It is fetched as '' when the data is read using a LOB locator.
            assert_equal(data, clob_data = row[2].read)
            assert_equal(ascii_enc, clob_data.encoding)
          else
            assert_equal(data, row[1])
            assert_equal(data, clob_data = row[2].read)
            assert_equal(enc, row[1].encoding)
            assert_equal(enc, clob_data.encoding)
          end
        end
        assert_nil(cursor.fetch)
        cursor.close

        begin
          OCI8::BindType::Mapping[:clob] = OCI8::BindType::Long
          OCI8::BindType::Mapping[:blob] = OCI8::BindType::LongRaw
          cursor = @conn.parse('SELECT * from test_table order by id')
          cursor.exec
          LONG_TEST_DATA.each_with_index do |data, index|
            row = cursor.fetch
            assert_equal(index, row[0])
            if data.nil?
              assert_nil(row[1])
              assert_nil(row[2])
            elsif data.empty?
              # '' is inserted to the long or long raw column as null.
              assert_nil(row[1])
              # '' is inserted to the clob or blob column as an empty clob.
              # However it is fetched as nil.
              assert_nil(row[2])
            else
              assert_equal(data, row[1])
              assert_equal(data, row[2])
              assert_equal(enc, row[1].encoding)
              assert_equal(enc, row[2].encoding)
            end
          end
          assert_nil(cursor.fetch)
          cursor.close
        ensure
          OCI8::BindType::Mapping[:clob] = clob_bind_type
          OCI8::BindType::Mapping[:blob] = blob_bind_type
        end
        drop_table('test_table')
      end
    ensure
      OCI8::BindType::Base.initial_chunk_size = initial_cunk_size
    end
    drop_table('test_table')
  end

  def test_bind_long_data
    initial_cunk_size = OCI8::BindType::Base.initial_chunk_size
    begin
      OCI8::BindType::Base.initial_chunk_size = 5
      cursor = @conn.parse("begin :1 := '<' || :2 || '>'; end;")
      cursor.bind_param(1, nil, :long)
      cursor.bind_param(2, nil, :long)
      (LONG_TEST_DATA + ['z' * 4000]).each do |data|
        cursor[2] = data
        cursor.exec
        assert_equal("<#{data}>", cursor[1])
      end
    ensure
      OCI8::BindType::Base.initial_chunk_size = initial_cunk_size
    end
  end

  def test_select
    drop_table('test_table')
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
        dt = OraDate.new(2000 + i, i % 2 == 0 ? 7 : 1, 3, 23, 59, 59)
      end
      cursor.exec(format("%10d", i * 10), i.to_s, i, dt, dt, dt, dt, i * 11111111111, i * 10000000000)
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
        month = i % 2 == 0 ? 7 : 1
        tm = Time.local(2000 + i, month, 3, 23, 59, 59)
        dt = Date.civil(2000 + i, month, 3)
        dttm = DateTime.civil(2000 + i, month, 3, 23, 59, 59, tm.utc_offset.to_r/86400)
	assert_equal(tm, rv[3])
	assert_equal(tm, rv[4])
	assert_equal(dt, rv[5])
	assert_equal(dttm, rv[6])
      end
      assert_equal(i * 11111111111, rv[7])
      assert_equal(i * 10000000000, rv[8])
    end
    assert_nil(cursor.fetch)

    # fetch_hash with block
    cursor.exec
    i = 1
    cursor.fetch_hash do |row|
      assert_equal(format("%10d", i * 10), row['C'])
      assert_equal(i.to_s, row['V'])
      assert_equal(i, row['N'])
      if i == 1
	assert_nil(row['D1'])
	assert_nil(row['D2'])
	assert_nil(row['D3'])
	assert_nil(row['D4'])
      else
        month = i % 2 == 0 ? 7 : 1
        tm = Time.local(2000 + i, month, 3, 23, 59, 59)
        dt = Date.civil(2000 + i, month, 3)
        dttm = DateTime.civil(2000 + i, month, 3, 23, 59, 59, tm.utc_offset.to_r/86400)
	assert_equal(tm, row['D1'])
	assert_equal(tm, row['D2'])
	assert_equal(dt, row['D3'])
	assert_equal(dttm, row['D4'])
      end
      assert_equal(i * 11111111111, row['INT'])
      assert_equal(i * 10000000000, row['BIGNUM'])
      i += 1
    end
    assert_equal(11, i)

    cursor.close
    drop_table('test_table')
  end

  def test_bind_cursor
    # FIXME: check again after upgrading Oracle 9.2 to 9.2.0.4.
    return if $oracle_version < OCI8::ORAVER_10_1

    drop_table('test_table')
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
    plsql.bind_param(':cursor', nil, OCI8::Cursor)
    plsql.exec
    cursor = plsql[':cursor']
    cursor.define(5, DateTime) # define 5th column as DateTime
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
        tm = Time.local(2000 + i, 8, 3, 23, 59, 59)
        dttm = DateTime.civil(2000 + i, 8, 3, 23, 59, 59, tm.utc_offset.to_r/86400)
	dt = Date.civil(2000 + i, 8, 3)
	assert_equal(tm, rv[3])
	assert_equal(dttm, rv[4])
	assert_equal(dt, rv[5])
      end
      assert_equal(i, rv[6])
      assert_equal(i, rv[7])
    end
    assert_nil(cursor.fetch)
    cursor.close
    drop_table('test_table')
  end

  def test_cursor_in_result_set
    drop_table('test_table')
    sql = <<-EOS
CREATE TABLE test_table (N NUMBER(10, 2))
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)
    cursor = @conn.parse("INSERT INTO test_table VALUES (:1)")
    1.upto(10) do |i|
      cursor.exec(i)
    end
    cursor.close
    cursor = @conn.exec(<<EOS)
select a.n, cursor (select a.n + b.n
                      from test_table b
                     order by n)
  from test_table a
 order by n
EOS
    1.upto(10) do |i|
      row = cursor.fetch
      assert_equal(i, row[0])
      cursor_in_result_set = row[1]
      1.upto(10) do |j|
        row2 = cursor_in_result_set.fetch
        assert_equal(i + j, row2[0])
      end
      assert_nil(cursor_in_result_set.fetch) # check end of row data
      cursor_in_result_set.close
    end
    assert_nil(cursor.fetch) # check end of row data
    drop_table('test_table')
  end

  def test_binary_float
    return if $oracle_version < OCI8::ORAVER_10_1

    # Oracle 10g or upper
    cursor = @conn.parse("select CAST(:1 AS BINARY_FLOAT), CAST(:2 AS BINARY_DOUBLE) from dual")
    bind_val = -1.0
    cursor.bind_param(1, 10.0, :binary_double)
    cursor.bind_param(2, nil, :binary_double)
    while bind_val < 10.0
      cursor[2] = bind_val
      cursor.exec
      rv = cursor.fetch
      assert_equal(10.0, rv[0])
      assert_equal(bind_val, rv[1])
      bind_val += 1.234
    end
    [-1.0/0.0, # -Infinite
     +1.0/0.0, # +Infinite
     0.0/0.0   # NaN
    ].each do |num|
      cursor[1] = num
      cursor[2] = num
      cursor.exec
      rv = cursor.fetch
      if num.nan?
        assert(rv[0].nan?)
        assert(rv[1].nan?)
      else
        assert_equal(num, rv[0])
        assert_equal(num, rv[1])
      end
    end
    cursor.close
  end

  def test_clob_nclob_and_blob
    return if OCI8::oracle_client_version < OCI8::ORAVER_8_1

    drop_table('test_table')
    sql = <<-EOS
CREATE TABLE test_table (id number(5), C CLOB, NC NCLOB, B BLOB)
STORAGE (
   INITIAL 100k
   NEXT 100k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)
    cursor = @conn.parse("INSERT INTO test_table VALUES (:1, :2, :3, :4)")
    0.upto(9) do |i|
      val = case i
            when 5; '' # empty string
            else format('%d', i) * 4096
            end
      cursor.exec(i, OCI8::CLOB.new(@conn, val), OCI8::NCLOB.new(@conn, val), OCI8::BLOB.new(@conn, val))
    end
    cursor.close
    cursor = @conn.exec("select * from test_table order by id")
    0.upto(9) do |i|
      rv = cursor.fetch
      val = case i
            when 5; '' # empty string
            else format('%d', i) * 4096
            end
      assert_equal(i, rv[0])
      assert_instance_of(OCI8::CLOB, rv[1])
      assert_instance_of(OCI8::NCLOB, rv[2])
      assert_instance_of(OCI8::BLOB, rv[3])
      assert_equal(val, rv[1].read)
      assert_equal(val.length, rv[2].size)
      assert_equal(val, rv[2].read)
      assert_equal(val, rv[3].read)
    end
    assert_nil(cursor.fetch)
    cursor.close
    drop_table('test_table')
  end

  def test_select_number
    drop_table('test_table')
    @conn.exec(<<EOS)
CREATE TABLE test_table (n NUMBER, n20 NUMBER(20), n14_2 NUMBER(14,2), n15_2 NUMBER(15,2), flt FLOAT)
STORAGE (
   INITIAL 100k
   NEXT 100k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(<<EOS)
INSERT INTO test_table values(12345678901234, 12345678901234567890, 123456789012.34, 1234567890123.45, 1234.5)
EOS
    @conn.exec("select * from test_table") do |row|
      assert_equal(row[0], 12345678901234)
      assert_equal(row[1], 12345678901234567890)
      assert_equal(row[2], 123456789012.34)
      assert_equal(row[3], BigDecimal("1234567890123.45"))
      assert_equal(row[4], 1234.5)
      assert_instance_of(BigDecimal, row[0])
      assert_instance_of(Bignum, row[1])
      assert_instance_of(Float, row[2])
      assert_instance_of(BigDecimal, row[3])
      assert_instance_of(Float, row[4])
    end
    drop_table('test_table')
  end

  def test_bind_number_with_implicit_conversions
    src = [1, 1.2, BigDecimal("1.2"), Rational(12, 10)]
    int = [1, 1, 1, 1]
    flt = [1, 1.2, 1.2, 1.2]
    dec = [BigDecimal("1"), BigDecimal("1.2"), BigDecimal("1.2"), BigDecimal("1.2")]
    rat = [Rational(1), Rational(12, 10), Rational(12, 10), Rational(12, 10)]

    cursor = @conn.parse("begin :1 := :2; end;")

    # Float
    cursor.bind_param(1, nil, Float)
    cursor.bind_param(2, nil, Float)
    src.each_with_index do |s, idx|
      cursor[2] = s
      cursor.exec
      assert_equal(cursor[1], flt[idx])
      assert_kind_of(Float, cursor[1])
    end

    # Fixnum
    cursor.bind_param(1, nil, Fixnum)
    cursor.bind_param(2, nil, Fixnum)
    src.each_with_index do |s, idx|
      cursor[2] = s
      cursor.exec
      assert_equal(cursor[1], int[idx])
      assert_kind_of(Fixnum, cursor[1])
    end

    # Integer
    cursor.bind_param(1, nil, Integer)
    cursor.bind_param(2, nil, Integer)
    src.each_with_index do |s, idx|
      cursor[2] = s
      cursor.exec
      assert_equal(cursor[1], int[idx])
      assert_kind_of(Integer, cursor[1])
    end

    # BigDecimal
    cursor.bind_param(1, nil, BigDecimal)
    cursor.bind_param(2, nil, BigDecimal)
    src.each_with_index do |s, idx|
      cursor[2] = s
      cursor.exec
      assert_equal(cursor[1], dec[idx])
      assert_kind_of(BigDecimal, cursor[1])
    end

    # Rational
    cursor.bind_param(1, nil, Rational)
    cursor.bind_param(2, nil, Rational)
    src.each_with_index do |s, idx|
      cursor[2] = s
      cursor.exec
      assert_equal(cursor[1], rat[idx])
      assert_kind_of(Rational, cursor[1])
    end
  end

  def test_parse_sets_query_on_cursor
    statement = "select * from country where country_code = 'ja'"
    cursor = @conn.parse(statement)
    assert_equal(statement, cursor.statement)
  end

  def test_last_error
    # OCI8#parse and OCI8#exec reset OCI8#last_error
    @conn.last_error = 'dummy'
    @conn.exec('begin null; end;')
    assert_nil(@conn.last_error)
    @conn.last_error = 'dummy'
    cursor = @conn.parse('select col1, max(col2) from (select 1 as col1, null as col2 from dual) group by col1')
    cursor.prefetch_rows = 1
    assert_nil(@conn.last_error)

    # When an OCI function returns OCI_SUCCESS_WITH_INFO, OCI8#last_error is set.
    @conn.last_error = 'dummy'
    cursor.exec
    assert_equal('dummy', @conn.last_error)
    cursor.fetch
    assert_kind_of(OCISuccessWithInfo, @conn.last_error)
    assert_equal(24347, @conn.last_error.code)
  end

  def test_environment_handle
    # OCI_ATTR_HEAPALLOC
    assert_operator(OCI8.send(:class_variable_get, :@@environment_handle).send(:attr_get_ub4, 30), :>,  0)
    # OCI_ATTR_OBJECT
    assert(OCI8.send(:class_variable_get, :@@environment_handle).send(:attr_get_boolean, 2))
    # OCI_ATTR_SHARED_HEAPALLOC
    assert_equal(0, OCI8.send(:class_variable_get, :@@environment_handle).send(:attr_get_ub4, 84))
  end

  def test_process_handle
    # OCI_ATTR_MEMPOOL_APPNAME
    assert_equal('', OCI8.send(:class_variable_get, :@@process_handle).send(:attr_get_string, 90))
    # OCI_ATTR_MEMPOOL_SIZE
    assert_equal(0, OCI8.send(:class_variable_get, :@@process_handle).send(:attr_get_ub4, 88))
  end

  def test_client_driver_name
    if OCI8.oracle_client_version >= OCI8::ORAVER_11_1 and @conn.oracle_server_version >= OCI8::ORAVER_11_1
      sid = @conn.select_one("select userenv('sid') from dual")[0].to_i
      cursor = @conn.parse('select client_driver from v$session_connect_info where sid = :1')
      cursor.exec(sid)
      column_size = cursor.column_metadata[0].data_size
      expected_value = case column_size
                       when 30 # Oracle version >= 12.1.0.2
                         "ruby-oci8 : #{OCI8::VERSION}"
                       when 9  # Oracle version <  12.1.0.2
                         'ruby-oci' # only the first 8 characters
                       else
                         raise "Unknown column size #{column_size}"
                       end
      driver_name = cursor.fetch[0].strip
      cursor.close
      assert_equal(expected_value, driver_name)
    end
  end

  def test_server_version
    cursor = @conn.exec("select * from product_component_version where product like 'Oracle Database %'")
    row = cursor.fetch_hash
    cursor.close
    ver = if OCI8::oracle_client_version >= OCI8::ORAVER_18
            row['VERSION_FULL'] || row['VERSION']
          else
            # OCI8#oracle_server_version could not get infomation corresponding
            # to VERSION_FULL when the Oracle client version is below 18.1.
            row['VERSION']
          end
    assert_equal(ver, @conn.oracle_server_version.to_s)
  end

  def test_array_fetch
    drop_table('test_table')
    @conn.exec("CREATE TABLE test_table (id number, val clob)")
    cursor = @conn.parse("INSERT INTO test_table VALUES (:1, :2)")
    1.upto(10) do |i|
      cursor.exec(i, ('a'.ord + i).chr * i)
    end
    cursor.close
    cursor = @conn.parse("select * from test_table where id <= :1 order by id")
    cursor.prefetch_rows = 4
    [1, 6, 2, 7, 3, 8, 4, 9, 5, 10].each do |i|
      cursor.exec(i)
      1.upto(i) do |j|
        row = cursor.fetch
        assert_equal(j, row[0])
        assert_equal(('a'.ord + j).chr * j, row[1].read)
      end
      assert_nil(cursor.fetch)
    end
  end
end # TestOCI8
