# Low-level API
require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestCLob < Minitest::Test

  def setup
    @conn = get_oci8_connection
  end

  def test_insert
    drop_table('test_table')
    @conn.exec('CREATE TABLE test_table (filename VARCHAR2(40), content CLOB)')
    filename = File.basename($lobfile)
    @conn.exec("DELETE FROM test_table WHERE filename = :1", filename)
    @conn.exec("INSERT INTO test_table(filename, content) VALUES (:1, EMPTY_CLOB())", filename)
    cursor = @conn.exec("SELECT content FROM test_table WHERE filename = :1 FOR UPDATE", filename)
    lob = cursor.fetch[0]
    open($lobfile) do |f|
      while s = f.read(1000)
        lob.write(s)
      end
    end
    lob.close
  end

  def test_insert_symbol
    drop_table('test_table')
    @conn.exec('CREATE TABLE test_table (filename VARCHAR2(40), content CLOB)')
    filename = 'test_symbol'
    value = :foo_bar
    @conn.exec("DELETE FROM test_table WHERE filename = :1", filename)
    @conn.exec("INSERT INTO test_table(filename, content) VALUES (:1, EMPTY_CLOB())", filename)
    cursor = @conn.exec("SELECT content FROM test_table WHERE filename = :1 FOR UPDATE", filename)
    lob = cursor.fetch[0]
    lob.write(value)
    lob.rewind
    assert_equal(value.to_s, lob.read);
    lob.close
  end

  def test_read
    drop_table('test_table')
    @conn.exec('CREATE TABLE test_table (filename VARCHAR2(40), content CLOB)')
    test_insert() # first insert data.
    filename = File.basename($lobfile)
    cursor = @conn.exec("SELECT content FROM test_table WHERE filename = :1 FOR UPDATE", filename)
    lob = cursor.fetch[0]

    open($lobfile) do |f|
      while buf = lob.read($lobreadnum)
        fbuf = f.read(buf.size)
        assert_equal(fbuf, buf)
        # offset += buf.size will not work fine,
        # Though buf.size counts in byte,
        # offset and $lobreadnum count in character.
      end
      assert_nil(buf)
      assert(f.eof?)
      assert(lob.eof?)
    end
    lob.close
  end

  # https://github.com/kubo/ruby-oci8/issues/20
  def _test_github_issue_20
    # Skip this test if FULLTEST isn't set because it takes 4 minutes in my Linux box.
    return if ENV['FULLTEST']

    lob1 = OCI8::CLOB.new(@conn, ' '  * (1024 * 1024))
    lob1.read(1) # to suppress `warning: assigned but unused variable - lob1`
    begin
      lob2 = OCI8::CLOB.new(@conn, ' '  * (128 * 1024 * 1024))
    rescue OCIError
      raise if $!.code != 24817
      # ORA-24817: Unable to allocate the given chunk for current lob operation
      GC.start
      # allocate smaller size
      lob2 = OCI8::CLOB.new(@conn, ' '  * (16 * 1024 * 1024))
    end
    lob1 = nil  # lob1's value will be freed in GC.
    lob2.read   # GC must run here to reproduce the issue.
  end

  def test_string_from_lob
    clob_bind_type = OCI8::BindType::Mapping[:clob]
    blob_bind_type = OCI8::BindType::Mapping[:blob]
    begin
      OCI8::BindType::Mapping[:clob] = OCI8::BindType::StringFromCLOB
      OCI8::BindType::Mapping[:blob] = OCI8::BindType::StringFromBLOB
      OCI8::BindType::Base.initial_chunk_size = 5

      drop_table('test_table')
      @conn.exec('CREATE TABLE test_table (id number(38), clob_col CLOB, nclob_col NCLOB, blob_col BLOB)')

      clob = OCI8::CLOB.new(@conn, '')
      nclob = OCI8::NCLOB.new(@conn, '')
      blob = OCI8::BLOB.new(@conn, '')
      cursor = @conn.parse('insert into test_table values (:1, :2, :3, :4)')
      cursor.bind_param(1, nil, Integer)
      cursor.bind_param(2, clob)
      cursor.bind_param(3, nclob)
      cursor.bind_param(4, blob)
      LONG_TEST_DATA.each_with_index do |data, index|
        cursor[1] = index
        if data.nil?
          cursor[2] = nil
          cursor[3] = nil
          cursor[4] = nil
        else
          clob.rewind
          clob.write(data)
          clob.size = data.size
          cursor[2] = clob
          nclob.rewind
          nclob.write(data)
          nclob.size = data.size
          cursor[3] = nclob
          blob.rewind
          blob.write(data)
          blob.size = data.size
          cursor[4] = blob
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
          assert_nil(row[3])
        else
          assert_equal(data, row[1])
          assert_equal(data, row[2])
          assert_equal(data, row[3])
        end
      end
      assert_nil(cursor.fetch)
      cursor.close

    ensure
      OCI8::BindType::Mapping[:clob] = clob_bind_type
      OCI8::BindType::Mapping[:blob] = blob_bind_type
    end
  end

  def teardown
    drop_table('test_table')
    @conn.logoff
  end
end
