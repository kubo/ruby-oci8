require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestBindArray < Minitest::Test

  def test_bind_array_names
    assert_equal(":id_0", OCI8::in_cond(:id, []).names)
    assert_equal(":id_0", OCI8::in_cond(:id, [1]).names)
    assert_equal(":id_0, :id_1", OCI8::in_cond(:id, [1, 2]).names)
    assert_equal(":id_0, :id_1, :id_2", OCI8::in_cond(:id, [1, 2, 3]).names)
  end

  def test_bind_array_values
    assert_equal([[nil, String, nil]], OCI8::in_cond(:id, []).values)
    assert_equal([[1, nil, nil]], OCI8::in_cond(:id, [1]).values)
    assert_equal([[1, nil, nil], [2, nil, nil]], OCI8::in_cond(:id, [1, 2]).values)
    assert_equal([[1, nil, nil], [2, nil, nil], [3, nil, nil]], OCI8::in_cond(:id, [1, 2, 3]).values)
  end

  def test_bind_array_values_containg_nil
    assert_equal([[nil, String]], OCI8::in_cond(:id, [nil]).values)
    assert_equal([[nil, Fixnum], [2, nil, nil], [3, nil, nil]], OCI8::in_cond(:id, [nil, 2, 3]).values)
    assert_equal([[1, nil, nil], [nil, Fixnum], [3, nil, nil]], OCI8::in_cond(:id, [1, nil, 3]).values)
  end

  def test_bind_array_values_with_type
    assert_equal([[nil, Integer, nil]], OCI8::in_cond(:id, [], Integer).values)
    assert_equal([[1, Integer, nil]], OCI8::in_cond(:id, [1], Integer).values)
    assert_equal([[1, Integer, nil], [2, Integer, nil]], OCI8::in_cond(:id, [1, 2], Integer).values)
    assert_equal([[1, Integer, nil], [2, Integer, nil], [3, Integer, nil]], OCI8::in_cond(:id, [1, 2, 3], Integer).values)
    assert_equal([[nil, Integer, nil], [2, Integer, nil], [3, Integer, nil]], OCI8::in_cond(:id, [nil, 2, 3], Integer).values)
    assert_equal([[1, Integer, nil], [nil, Integer, nil], [3, Integer, nil]], OCI8::in_cond(:id, [1, nil, 3], Integer).values)
  end

  def test_select
    @conn = get_oci8_connection
    begin
      drop_table('test_table')
      @conn.exec(<<EOS)
CREATE TABLE test_table (ID NUMBER(38))
EOS
      cursor = @conn.parse('insert into test_table values(:1)')
      cursor.bind_param(1, nil, Integer)
      [1, 3, 5].each do |id|
        cursor.exec(id)
      end
      cursor.close

      [
       [],
       [1],
       [1, 2],
       [1, 2, 3],
       [nil],
       [nil, 2, 3],
       [1, nil, 3],
      ].each do |ids|
        in_cond = OCI8::in_cond(:id, ids)
        cursor = @conn.exec("select * from test_table where id in (#{in_cond.names}) order by id", *in_cond.values)
        ([1, 3, 5] & ids).each do |id|
          assert_equal(id, cursor.fetch[0])
        end
      end

      drop_table('test_table')
    ensure
      @conn.logoff
    end
  end
end
