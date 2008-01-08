require 'dbi'
require 'test/unit'
require './config'

class TestDbiCLob < Test::Unit::TestCase

  def setup
    @dbh = $dbh
  end

  def test_insert
    @dbh.do("DELETE FROM test_clob WHERE filename = :1", $lobfile)

    # insert an empty clob and get the rowid.
    rowid = @dbh.execute("INSERT INTO test_clob(filename, content) VALUES (:1, EMPTY_CLOB())", $lobfile) do |sth|
      sth.func(:rowid)
    end
    lob = @dbh.select_one("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE", $lobfile)[0]
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
    test_insert() # first insert data.
    lob = @dbh.select_one("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE", $lobfile)[0]
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
    @dbh.rollback
  end
end
