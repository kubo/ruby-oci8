# Low-level API
require 'oci8'
require 'test/unit'
require './config'

class TestDescribe < Test::Unit::TestCase

  def setup
    # initialize oracle as object mode.
    # This is workaround to use OCIDescribeAny in Oracle 8.0.5 on Linux.
    # This bug was fixed in 8.0.6 and 8i.
    @env, @svc, @stmt = setup_lowapi()
    @desc = @env.alloc(OCIDescribe)
  end

  def test_describe_sequence
    begin
      @stmt.prepare("DROP SEQUENCE test_sequence").execute(@svc)
    rescue OCIError
      raise if $!.code != 2289 # sequence does not exist
    end
    minvalue = 1
    maxvalue = 1234567890123456789012345678 # bignum
    incr = 777
    sql = <<-EOS
CREATE SEQUENCE test_sequence
    MINVALUE #{minvalue}
    MAXVALUE #{maxvalue}
    INCREMENT BY #{incr}
    ORDER
EOS
    @stmt.prepare(sql).execute(@svc)
    @desc.describeAny(@svc, "test_sequence", OCI_PTYPE_SEQ)
    parm = @desc.attrGet(OCI_ATTR_PARAM)
    # common part
    assert_equal(OCI_PTYPE_SEQ, parm.attrGet(OCI_ATTR_PTYPE))
    # specific part
    assert_instance_of(Fixnum, parm.attrGet(OCI_ATTR_OBJID))
    assert_equal(minvalue, parm.attrGet(OCI_ATTR_MIN))
    assert_equal(maxvalue, parm.attrGet(OCI_ATTR_MAX))
    assert_equal(incr, parm.attrGet(OCI_ATTR_INCR))
    assert_kind_of(Integer, parm.attrGet(OCI_ATTR_CACHE))
    assert_equal(true, parm.attrGet(OCI_ATTR_ORDER))
    assert_kind_of(Integer, parm.attrGet(OCI_ATTR_HW_MARK))
    @stmt.prepare("DROP SEQUENCE test_sequence").execute(@svc)
  end

  def test_describe_synonym
    begin
      @stmt.prepare("DROP SYNONYM test_synonym").execute(@svc)
    rescue OCIError
      raise if $!.code != 1434 # private synonym to be dropped does not exist
    end
    @stmt.prepare("CREATE SYNONYM test_synonym FOR foo.bar@baz").execute(@svc)
    @desc.describeAny(@svc, "test_synonym", OCI_PTYPE_SYN)
    parm = @desc.attrGet(OCI_ATTR_PARAM)
    # common part
    assert_equal(OCI_PTYPE_SYN, parm.attrGet(OCI_ATTR_PTYPE))
    # specific part
    assert_instance_of(Fixnum, parm.attrGet(OCI_ATTR_OBJID))
    assert_equal("FOO", parm.attrGet(OCI_ATTR_SCHEMA_NAME))
    assert_equal("BAR", parm.attrGet(OCI_ATTR_NAME))
    assert_equal("BAZ", parm.attrGet(OCI_ATTR_LINK))
    @stmt.prepare("DROP SYNONYM test_synonym").execute(@svc)
  end

  def test_describe_table_and_columns
    begin
      @stmt.prepare("DROP TABLE test_table").execute(@svc)
    rescue OCIError
      raise if $!.code != 942 # table or view does not exist
    end
    sql = <<-EOS
CREATE TABLE test_table
  (C CHAR(10) NOT NULL,
   V VARCHAR2(20),
   N NUMBER(10, 2),
   D DATE)
STORAGE (
   INITIAL 4k
   NEXT 4k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @stmt.prepare(sql).execute(@svc)
    @desc.describeAny(@svc, "test_table", OCI_PTYPE_TABLE)
    tab_parm = @desc.attrGet(OCI_ATTR_PARAM)
    col_list = tab_parm.attrGet(OCI_ATTR_LIST_COLUMNS)
    num_cols = tab_parm.attrGet(OCI_ATTR_NUM_COLS)
    # common part for table
    assert_equal(OCI_PTYPE_TABLE, tab_parm.attrGet(OCI_ATTR_PTYPE))
    # specific part for table
    assert_instance_of(Fixnum, tab_parm.attrGet(OCI_ATTR_OBJID))
    assert_equal(4, num_cols)
    # common part for column list
    assert_equal(OCI_PTYPE_LIST, col_list.attrGet(OCI_ATTR_PTYPE))

    col = Array.new(num_cols)
    1.upto(num_cols) do |i|
      col[i] = col_list.paramGet(i)
      assert_equal(OCI_PTYPE_COL, col[1].attrGet(OCI_ATTR_PTYPE))
    end
    assert_equal(10, col[1].attrGet(OCI_ATTR_DATA_SIZE))
    assert_equal(20, col[2].attrGet(OCI_ATTR_DATA_SIZE))
    assert_equal(22, col[3].attrGet(OCI_ATTR_DATA_SIZE))
    assert_equal(7, col[4].attrGet(OCI_ATTR_DATA_SIZE))
    assert_equal(OCI_TYPECODE_CHAR, col[1].attrGet(OCI_ATTR_DATA_TYPE))
    assert_equal(OCI_TYPECODE_VARCHAR, col[2].attrGet(OCI_ATTR_DATA_TYPE))
    assert_equal(OCI_TYPECODE_NUMBER, col[3].attrGet(OCI_ATTR_DATA_TYPE))
    assert_equal(OCI_TYPECODE_DATE, col[4].attrGet(OCI_ATTR_DATA_TYPE))
    assert_equal("C", col[1].attrGet(OCI_ATTR_NAME))
    assert_equal("V", col[2].attrGet(OCI_ATTR_NAME))
    assert_equal("N", col[3].attrGet(OCI_ATTR_NAME))
    assert_equal("D", col[4].attrGet(OCI_ATTR_NAME))
    assert_equal(0, col[1].attrGet(OCI_ATTR_PRECISION))
    assert_equal(0, col[2].attrGet(OCI_ATTR_PRECISION))
    assert_equal(10, col[3].attrGet(OCI_ATTR_PRECISION))
    assert_equal(0, col[4].attrGet(OCI_ATTR_PRECISION))
    assert_equal(0, col[1].attrGet(OCI_ATTR_SCALE))
    assert_equal(0, col[2].attrGet(OCI_ATTR_SCALE))
    assert_equal(2, col[3].attrGet(OCI_ATTR_SCALE))
    assert_equal(0, col[4].attrGet(OCI_ATTR_SCALE))
    assert_equal(false, col[1].attrGet(OCI_ATTR_IS_NULL))
    assert_equal(true, col[2].attrGet(OCI_ATTR_IS_NULL))
    assert_equal(true, col[3].attrGet(OCI_ATTR_IS_NULL))
    assert_equal(true, col[4].attrGet(OCI_ATTR_IS_NULL))
    @stmt.prepare("DROP TABLE test_table").execute(@svc)
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end
