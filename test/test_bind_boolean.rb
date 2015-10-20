# Low-level API
require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestBindBoolean < Minitest::Test
  def setup
    @conn = get_oci8_connection()
  end

  def test_set_raw
    stmt = <<EOS
DECLARE
  bool_val boolean;
  int_val pls_integer;
BEGIN
  bool_val := :in;
  IF bool_val THEN
    int_val := 1;
  ELSE
    int_val := 0;
  END IF;
  :out := int_val;
END;
EOS
    # explicit bind
    cursor = @conn.parse(stmt)
    cursor.bind_param(:in, nil, TrueClass)
    cursor.bind_param(:out, nil, Integer)

    cursor[:in] = true
    cursor.exec
    assert_equal(1, cursor[:out])

    cursor[:in] = false
    cursor.exec
    assert_equal(0, cursor[:out])

    # implicit bind
    do_block_cnt = 0
    @conn.exec(stmt, true, 0) do |in_val, out_val|
      assert_equal(true, in_val)
      assert_equal(1, out_val)
      do_block_cnt += 1
    end

    @conn.exec(stmt, false, 0) do |in_val, out_val|
      assert_equal(false, in_val)
      assert_equal(0, out_val)
      do_block_cnt += 1
    end
    assert_equal(2, do_block_cnt)
  end

  def test_get_raw
    stmt = <<EOS
DECLARE
  int_val pls_integer;
  bool_val boolean;
BEGIN
  int_val := :in;
  IF int_val <> 0 THEN
    bool_val := TRUE;
  ELSE
    bool_val := FALSE;
  END IF;
  :out := bool_val;
END;
EOS
    cursor = @conn.parse(stmt)
    cursor.bind_param(:in, nil, Integer)
    cursor.bind_param(:out, nil, TrueClass)

    cursor[:in] = 1
    cursor.exec
    assert_equal(true, cursor[:out])

    cursor[:in] = 0
    cursor.exec
    assert_equal(false, cursor[:out])

    do_block_cnt = 0
    @conn.exec(stmt, 1, true) do |in_val, out_val|
      assert_equal(1, in_val)
      assert_equal(true, out_val)
      do_block_cnt += 1
    end

    @conn.exec(stmt, 0, true) do |in_val, out_val|
      assert_equal(0, in_val)
      assert_equal(false, out_val)
      do_block_cnt += 1
    end
    assert_equal(2, do_block_cnt)
  end

  def teardown
    @conn.logoff
  end
end
