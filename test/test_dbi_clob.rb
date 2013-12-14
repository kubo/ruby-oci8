require 'dbi'
require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestDbiCLob < Minitest::Test

  def setup
    @dbh = get_dbi_connection()
    drop_table('test_table')
    @dbh.execute('CREATE TABLE test_table (filename VARCHAR2(40), content CLOB)')
  end

  def test_insert
    filename = File.basename($lobfile)

    # insert an empty clob and get the rowid.
    rowid = @dbh.execute("INSERT INTO test_table(filename, content) VALUES (:1, EMPTY_CLOB())", filename) do |sth|
      sth.func(:rowid)
    end
    lob = @dbh.select_one("SELECT content FROM test_table WHERE filename = :1 FOR UPDATE", filename)[0]
    begin
      open($lobfile) do |f|
        while f.gets()
          lob.write($_)
        end
      end
    ensure
      lob.close()
    end
  end

  def test_read
    filename = File.basename($lobfile)
    test_insert() # first insert data.
    lob = @dbh.select_one("SELECT content FROM test_table WHERE filename = :1 FOR UPDATE", filename)[0]
    begin
      open($lobfile) do |f|
        while buf = lob.read($lobreadnum)
          fbuf = f.read(buf.size)
          assert_equal(fbuf, buf)
        end
        assert_equal(nil, buf)
        assert_equal(true, f.eof?)
      end
    ensure
      lob.close()
    end
  end

  def teardown
    @dbh.disconnect
  end
end
