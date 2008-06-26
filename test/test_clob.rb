# Low-level API
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require File.dirname(__FILE__) + '/config'

class TestCLob < RUNIT::TestCase

  def setup
    @env, @svc, @stmt = setup_lowapi()
  end

  def test_insert
    filename = File.basename($lobfile)
    @stmt.prepare("DELETE FROM test_clob WHERE filename = :1")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.execute(@svc)

    @stmt.prepare("INSERT INTO test_clob(filename, content) VALUES (:1, EMPTY_CLOB())")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.execute(@svc)

    lob = @env.alloc(OCILobLocator)
    @stmt.prepare("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.defineByPos(1, OCI_TYPECODE_CLOB, lob)
    @stmt.execute(@svc, 1)
    offset = 1 # count by charactor, not byte.
    open($lobfile) do |f|
      while f.gets()
	num_of_chars = lob.write(@svc, offset, $_)
	offset += num_of_chars
      end
    end
    lob.free()
  end

  def test_insert_with_open_close
    filename = File.basename($lobfile)
    @stmt.prepare("DELETE FROM test_clob WHERE filename = :1")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.execute(@svc)

    @stmt.prepare("INSERT INTO test_clob(filename, content) VALUES (:1, EMPTY_CLOB())")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.execute(@svc)

    lob = @env.alloc(OCILobLocator)
    @stmt.prepare("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.defineByPos(1, OCI_TYPECODE_CLOB, lob)
    @stmt.execute(@svc, 1)
    lob.open(@svc)
    begin
      offset = 1 # count by charactor, not byte.
      open($lobfile) do |f|
	while f.gets()
	  offset += lob.write(@svc, offset, $_)
	end
      end
    ensure
      lob.close(@svc)
    end
    lob.free()
  end

  def test_read
    filename = File.basename($lobfile)
    test_insert() # first insert data.
    lob = @env.alloc(OCILobLocator)
    @stmt.prepare("SELECT content FROM test_clob WHERE filename = :1 FOR UPDATE")
    @stmt.bindByPos(1, String, filename.size).set(filename)
    @stmt.defineByPos(1, OCI_TYPECODE_CLOB, lob)
    @stmt.execute(@svc, 1)

    open($lobfile) do |f|
      offset = 1
      while buf = lob.read(@svc, offset, $lobreadnum)
	fbuf = f.read(buf.size)
	assert_equal(fbuf, buf)
	offset += $lobreadnum
	# offset += buf.size will not work fine,
	# Though buf.size counts in byte,
	# offset and $lobreadnum count in character.
      end
      assert_equal(nil, buf)
      assert_equal(true, f.eof?)
    end
    lob.free()
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestCLob.suite())
end
