require 'dbi'
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestDbiCLob < RUNIT::TestCase

  def setup
    @dbh = DBI.connect("dbi:OCI8:#{$dbname}", $dbuser, $dbpass, 'AutoCommit' => false)
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
    @dbh.disconnect()
  end
end

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestCLob.suite())
end
