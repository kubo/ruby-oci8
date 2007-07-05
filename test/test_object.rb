require 'oci8'
require 'test/unit'
require './config'

class RbTestObj < OCI8::Object::Base
end

class TestObj1 < Test::Unit::TestCase
  @@delta = 0.00001

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
    RbTestObj.default_connection = @conn
  end

  def teardown
    @conn.logoff
  end

  class ExpectedVal

    attr_reader :n
    attr_reader :integer_val
    attr_reader :float_val
    attr_reader :string_val
    attr_reader :str_array_val

    def initialize
      @n = 0.0
    end

    def next
      @n += 1.2
      @integer_val = @n.round
      @float_val = @n
      @string_val = @n.to_s
      @string_val = $` if /.0$/ =~ @string_val
      if @integer_val == 1
        @str_array_val = nil
      else
        @str_array_val = []
        0.upto(2) do |i|
          val = (@n + i).to_s
          val = $` if /.0$/ =~ val
          @str_array_val[i] = val
        end
      end
      @n <= 20
    end
  end

  def assert_rb_test_obj(exp, obj)
    assert_instance_of(RbTestObj, obj)
    assert_equal(exp.integer_val, obj.integer_val)
    assert_in_delta(exp.float_val, obj.float_val, @@delta)
    assert_equal(exp.string_val, obj.string_val)
    assert_equal(exp.str_array_val, obj.str_array_val && obj.str_array_val.to_ary)
  end

  def test_select1
    expected_val = ExpectedVal.new
    @conn.exec("select * from rb_test_obj_tab1 order by n") do |row|
      assert(expected_val.next)

      assert_in_delta(expected_val.n, row[0], @@delta)
      assert_rb_test_obj(expected_val, row[1])
    end
    assert(!expected_val.next)
  end

  def test_select2
    expected_val = ExpectedVal.new
    @conn.exec("select * from rb_test_obj_tab2 order by integer_val") do |row|
      assert(expected_val.next)

      assert_equal(expected_val.integer_val, row[0])
      assert_in_delta(expected_val.float_val, row[1], @@delta)
      assert_equal(expected_val.string_val, row[2])
    end
    assert(!expected_val.next)
  end

  def test_select3
    expected_val = ExpectedVal.new
    @conn.exec("select value(p) from rb_test_obj_tab2 p order by integer_val") do |row|
      assert(expected_val.next)

      assert_rb_test_obj(expected_val, row[0])
    end
    assert(!expected_val.next)
  end

  def _test_select4 # TODO
    expected_val = ExpectedVal.new
    @conn.exec("select ref(p) from rb_test_obj_tab2 p order by integer_val") do |row|
      assert(expected_val.next)

      assert_rb_test_obj(expected_val, row[0])
    end
    assert(!expected_val.next)
  end

  def test_explicit_constructor
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.n)
      assert_rb_test_obj(expected_val, obj)
      assert_nothing_raised do
        obj.inspect
      end
    end
  end

  def _test_implicit_constructor # TODO
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.integer_val, expected_val.float_val, expected_val.string_val, expected_val.str_array_val)
      assert_rb_test_obj(expected_val, obj)
    end
  end

  def _test_implicit_constructor2 # TODO
    obj = nil
    assert_nothing_raised do
      obj = RbTestObj.new(nil, nil, nil)
    end
    assert_nil(obj.integer_val)
    assert_nil(obj.float_val)
    assert_nil(obj.string_val)
  end

  def test_class_func
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.class_func(expected_val.n)
      assert_rb_test_obj(expected_val, obj)
    end
  end

  def test_class_proc1
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(0)
      RbTestObj.class_proc1(obj, expected_val.n)
      assert_rb_test_obj(expected_val, obj)
    end
  end

  def test_class_proc2
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new
      obj.integer_val = expected_val.integer_val - 1
      obj.float_val = expected_val.float_val
      obj.string_val = expected_val.string_val
      obj.str_array_val = expected_val.str_array_val
      RbTestObj.class_proc2(obj)
      assert_rb_test_obj(expected_val, obj)
    end
  end

  def test_member_func
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.n)
      assert_equal(expected_val.integer_val, obj.member_func)
    end
  end

  def test_plsql_member_func
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.n)
      rv = @conn.exec(<<EOS, obj, [nil, Integer])
declare
  obj rb_test_obj := :obj;
begin
  :rv := obj.member_func;
end;
EOS
      assert_equal(expected_val.integer_val, rv[1])
    end
  end

  def test_member_proc
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new
      obj.member_proc(expected_val.integer_val)
      assert_equal(expected_val.integer_val, obj.integer_val)
    end
  end
end
