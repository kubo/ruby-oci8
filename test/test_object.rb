require 'oci8'
require 'test/unit'
require './config'

class RbTestObj < OCI8::Object::Base
end

class TestObj1 < Test::Unit::TestCase
  Delta = 0.00001

  def setup
    @conn = get_oci8_connection
    RbTestObj.default_connection = @conn
  end

  def teardown
    @conn.logoff
  end

  class ExpectedValObjElem
    def initialize(x, y)
      @attributes = {:x => x, :y => y}
    end
    def ==(val)
      @attributes[:x] == val.x and @attributes[:y] == val.y
    end
  end

  class ExpectedVal
    include Test::Unit::Assertions

    attr_reader :n
    attr_reader :int_val
    attr_reader :flt_val
    attr_reader :num_val
    attr_reader :bdbl_val
    attr_reader :bflt_val
    attr_reader :str_val
    attr_reader :raw_val
    attr_reader :obj_val
    attr_reader :int_array_val
    attr_reader :flt_array_val
    attr_reader :num_array_val
    attr_reader :bdbl_array_val
    attr_reader :bflt_array_val
    attr_reader :str_array_val
    attr_reader :raw_array_val
    attr_reader :obj_array_val
    attr_reader :obj_ary_of_ary_val

    def initialize
      @n = 0.0
    end

    def next
      @n += 1.2
      @int_val = @n.round
      @flt_val = @n
      @num_val = @n
      @bdbl_val = @n
      @bflt_val = @n
      @str_val = @n.to_s
      @str_val = $` if /.0$/ =~ @str_val
      @raw_val = @str_val
      @obj_val = ExpectedValObjElem.new(@int_val, @int_val + 1)
      if @int_val == 1
        @int_array_val = nil
        @flt_array_val = nil
        @num_array_val = nil
        @bdbl_array_val = nil
        @bflt_array_val = nil
        @str_array_val = nil
        @raw_array_val = nil
        @obj_array_val = nil
        @obj_ary_of_ary_val = nil
      else
        @int_array_val = []
        @flt_array_val = []
        @num_array_val = []
        @bdbl_array_val = []
        @bflt_array_val = []
        @str_array_val = []
        @raw_array_val = []
        @obj_array_val = []
        @obj_ary_of_ary_val = []
        0.upto(2) do |i|
          ival = @n.round
          val = (@n + i).to_s
          val = $` if /.0$/ =~ val
          @int_array_val[i] = @int_val + i
          @flt_array_val[i] = @n + i
          @num_array_val[i] = @n + i
          @bdbl_array_val[i] = @n + i
          @bflt_array_val[i] = @n + i
          @str_array_val[i] = val
          @raw_array_val[i] = val
          @obj_array_val[i] = ExpectedValObjElem.new(@int_val + i, @int_val + i + 1)
        end
        @obj_ary_of_ary_val[0] = @obj_array_val
      end
      @n <= 20
    end

    def assert(val)
      if val.is_a? Array
        int_val = val[0]
        flt_val = val[1]
        num_val = val[2]
        bdbl_val = val[3]
        bflt_val = val[4]
        str_val = val[5]
        raw_val = val[6]
        obj_val = val[7]
        int_array_val = val[8]
        flt_array_val = val[9]
        num_array_val = val[10]
        bdbl_array_val = val[11]
        bflt_array_val = val[12]
        str_array_val = val[13]
        raw_array_val = val[14]
        obj_array_val = val[15]
        obj_ary_of_ary_val = val[16]
      else
        assert_instance_of(RbTestObj, val)
        int_val = val.int_val
        flt_val = val.flt_val
        num_val = val.num_val
        bdbl_val = val.bdbl_val
        bflt_val = val.bflt_val
        str_val = val.str_val
        raw_val = val.raw_val
        obj_val = val.obj_val
        int_array_val = val.int_array_val
        flt_array_val = val.flt_array_val
        num_array_val = val.num_array_val
        bdbl_array_val = val.bdbl_array_val
        bflt_array_val = val.bflt_array_val
        str_array_val = val.str_array_val
        raw_array_val = val.raw_array_val
        obj_array_val = val.obj_array_val
        obj_ary_of_ary_val = val.obj_ary_of_ary_val
      end

      assert_equal(@int_val, int_val)
      assert_in_delta(@flt_val, flt_val, Delta)
      assert_in_delta(@num_val, num_val, Delta)
      assert_in_delta(@bdbl_val, bdbl_val, Delta)
      assert_in_delta(@bflt_val, bflt_val, Delta)
      assert_equal(@str_val, str_val)
      assert_equal(@raw_val, raw_val)
      assert_equal(@obj_val, obj_val)
      assert_equal(@int_array_val, int_array_val && int_array_val.to_ary)
      assert_array_in_delta(@flt_array_val, flt_array_val && flt_array_val.to_ary)
      assert_array_in_delta(@num_array_val, num_array_val && num_array_val.to_ary)
      assert_array_in_delta(@bdbl_array_val, bdbl_array_val && bdbl_array_val.to_ary)
      assert_array_in_delta(@bflt_array_val, bflt_array_val && bflt_array_val.to_ary)
      assert_equal(@str_array_val, str_array_val && str_array_val.to_ary)
      assert_equal(@raw_array_val, raw_array_val && raw_array_val.to_ary)
      assert_equal(@obj_array_val, obj_array_val && obj_array_val.to_ary)
      assert_equal(@obj_ary_of_ary_val, obj_ary_of_ary_val && obj_ary_of_ary_val.to_ary.collect { |elem| elem.to_ary })
    end

    def assert_array_in_delta(exp, val)
      if exp && val
        assert_equal(exp.size, val.size)
        exp.each_with_index do |elem, idx|
          assert_in_delta(elem, val[idx], Delta)
        end
      else
        assert_equal(exp, val)
      end
    end
  end

  def test_select1
    expected_val = ExpectedVal.new
    @conn.exec("select * from rb_test_obj_tab1 order by n") do |row|
      assert(expected_val.next)

      assert_in_delta(expected_val.n, row[0], Delta)
      expected_val.assert(row[1])
    end
    assert(!expected_val.next)
  end

  def test_select2
    expected_val = ExpectedVal.new
    @conn.exec("select * from rb_test_obj_tab2 order by int_val") do |row|
      assert(expected_val.next)

      expected_val.assert(row)
    end
    assert(!expected_val.next)
  end

  def test_select3
    expected_val = ExpectedVal.new
    @conn.exec("select value(p) from rb_test_obj_tab2 p order by int_val") do |row|
      assert(expected_val.next)

      expected_val.assert(row[0])
    end
    assert(!expected_val.next)
  end

  def _test_select4 # TODO
    expected_val = ExpectedVal.new
    @conn.exec("select ref(p) from rb_test_obj_tab2 p order by int_val") do |row|
      assert(expected_val.next)

      expected_val.assert(row[0])
    end
    assert(!expected_val.next)
  end

  def test_explicit_constructor
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.n)
      expected_val.assert(obj)
      assert_nothing_raised do
        obj.inspect
      end
    end
  end

  def _test_implicit_constructor # TODO
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.int_val, expected_val.flt_val, expected_val.str_val, expected_val.raw_val, expected_val.str_array_val, expected_val.raw_array_val, expected_val.num_array_val)
      expected_val.assert(obj)
    end
  end

  def _test_implicit_constructor2 # TODO
    obj = nil
    assert_nothing_raised do
      obj = RbTestObj.new(nil, nil, nil)
    end
    assert_nil(obj.int_val)
    assert_nil(obj.flt_val)
    assert_nil(obj.str_val)
  end

  def test_class_func
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.class_func(expected_val.n)
      expected_val.assert(obj)
    end
  end

  def test_class_proc1
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(0)
      RbTestObj.class_proc1(obj, expected_val.n)
      expected_val.assert(obj)
    end
  end

  def test_class_proc2
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new
      obj.int_val = expected_val.int_val - 1
      obj.flt_val = expected_val.flt_val
      obj.num_val = expected_val.num_val
      obj.bdbl_val = expected_val.bdbl_val
      obj.bflt_val = expected_val.bflt_val
      obj.str_val = expected_val.str_val
      obj.raw_val = expected_val.raw_val
      obj.obj_val = expected_val.obj_val
      obj.int_array_val = expected_val.int_array_val
      obj.flt_array_val = expected_val.flt_array_val
      obj.num_array_val = expected_val.num_array_val
      obj.bdbl_array_val = expected_val.bdbl_array_val
      obj.bflt_array_val = expected_val.bflt_array_val
      obj.str_array_val = expected_val.str_array_val
      obj.raw_array_val = expected_val.raw_array_val
      obj.obj_array_val = expected_val.obj_array_val
      obj.obj_ary_of_ary_val = expected_val.obj_ary_of_ary_val
      RbTestObj.class_proc2(obj)
      expected_val.assert(obj)
    end
  end

  def test_member_func
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new(expected_val.n)
      assert_equal(expected_val.int_val, obj.member_func)
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
      assert_equal(expected_val.int_val, rv[1])
    end
  end

  def test_member_proc
    expected_val = ExpectedVal.new
    while expected_val.next
      obj = RbTestObj.new
      obj.member_proc(expected_val.int_val)
      assert_equal(expected_val.int_val, obj.int_val)
    end
  end
end
