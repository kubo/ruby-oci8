# Low-level API
require 'oci8'
require 'test/unit'
require './config'

class TestCLob < Test::Unit::TestCase

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
  end

  def test_insert
    @conn.exec("DELETE FROM test_clob WHERE filename = :1", $lobfile)
    @conn.exec("INSERT INTO test_clob(filename, content) VALUES (:1, EMPTY_CLOB())", $lobfile)
    cursor = @conn.exec("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE", $lobfile)
    lob = cursor.fetch[0]
    open($lobfile) do |f|
      while f.gets()
        lob.write($_)
      end
    end
    lob.close
  end

  def test_insert_with_flush
    @conn.exec("DELETE FROM test_clob WHERE filename = :1", $lobfile)
    @conn.exec("INSERT INTO test_clob(filename, content) VALUES (:1, EMPTY_CLOB())", $lobfile)
    cursor = @conn.exec("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE", $lobfile)
    lob = cursor.fetch[0]
    lob.sync = false
    open($lobfile) do |f|
      while f.gets()
        lob.write($_)
      end
    end
    lob.flush
    lob.close
  end

  def test_read
    test_insert() # first insert data.
    cursor = @conn.exec("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE", $lobfile)
    lob = cursor.fetch[0]

    open($lobfile) do |f|
      while buf = lob.read($lobreadnum)
        fbuf = f.read(buf.size)
        assert_equal(fbuf, buf)
        # offset += buf.size will not work fine,
        # Though buf.size counts in byte,
        # offset and $lobreadnum count in character.
      end
      assert_equal(nil, buf)
      assert(f.eof?)
      assert(lob.eof?)
    end
    lob.close
  end

  def teardown
    @conn.logoff
  end
end
