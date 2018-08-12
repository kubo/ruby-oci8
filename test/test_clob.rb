# Low-level API
require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestCLob < Minitest::Test

  def setup
    @conn = get_oci8_connection
    drop_table('test_table')
    @conn.exec('CREATE TABLE test_table (filename VARCHAR2(40), content CLOB)')
  end

  def test_insert
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
  def test_github_issue_20
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

  def teardown
    drop_table('test_table')
    @conn.logoff
  end
end
