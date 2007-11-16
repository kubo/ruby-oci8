require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestMetadata < RUNIT::TestCase

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
  end

  def teardown
    @conn.logoff
  end

  def test_metadata
    # data_size factor for nchar charset_form.
    cursor = @conn.exec("select CAST('1' AS NCHAR(1)) from dual")
    cfrm = cursor.column_metadata[0].data_size
    if $oracle_version >=  900
      # data_size factor for char semantics.
      cursor = @conn.exec("select CAST('1' AS CHAR(1 char)) from dual")
      csem = cursor.column_metadata[0].data_size
    end

    coldef =
      [
       # oracle_version, definition,    data_type,  csfrm,    null?,csem?,csize, data_size,prec,scale,fsprec,lfprec
       [800, "CHAR(10) NOT NULL",       :char,      :implicit, false, false, 10,        10,   0,    0,    0,    0],
       [900, "CHAR(10 CHAR)",           :char,      :implicit, true,  true,  10, 10 * csem,   0,    0,    0,    0],
       [800, "NCHAR(10)",               :char,      :nchar,    true,  true,  10, 10 * cfrm,   0,    0,    0,    0],
       [800, "VARCHAR2(10)",            :varchar2,  :implicit, true,  false, 10,        10,   0,    0,    0,    0],
       [900, "VARCHAR2(10 CHAR)",       :varchar2,  :implicit, true,  true,  10, 10 * csem,   0,    0,    0,    0],
       [800, "NVARCHAR2(10)",           :varchar2,  :nchar,    true,  true,  10, 10 * cfrm,   0,    0,    0,    0],
       [800, "CLOB",                    :clob,      :implicit, true,  false,  0,      4000,   0,    0,    0,    0],
       [800, "NCLOB",                   :clob,      :nchar,    true,  false,  0,      4000,   0,    0,    0,    0],
       [800, "RAW(10)",                 :raw,       nil,       true,  false,  0,        10,   0,    0,    0,    0],
       [800, "BLOB",                    :blob,      nil,       true,  false,  0,      4000,   0,    0,    0,    0],
       [800, "BFILE",                   :bfile,     nil,       true,  false,  0,       530,   0,    0,    0,    0],
       [800, "NUMBER",                  :number,    nil,       true,  false,  0,        22,   0, -127,  129,    0],
       [800, "NUMBER(10)",              :number,    nil,       true,  false,  0,        22,  10,    0,    0,   10],
       [800, "NUMBER(10,2)",            :number,    nil,       true,  false,  0,        22,  10,    2,    2,   10],
       [800, "FLOAT",                   :number,    nil,       true,  false,  0,        22, 126, -127,  129,  126],
       [800, "FLOAT(10)",               :number,    nil,       true,  false,  0,        22,  10, -127,  129,   10],
       [1000,"BINARY_FLOAT",            :binary_float,  nil,   true,  false,  0,         4,   0,    0,    0,    0],
       [1000,"BINARY_DOUBLE",           :binary_double, nil,   true,  false,  0,         8,   0,    0,    0,    0],
       [800, "DATE",                    :date,      nil,       true,  false,  0,         7,   0,    0,    0,    0],
       [900, "TIMESTAMP",                         :timestamp,     nil, true, false, 0,  11,   0,    6,    6,    0],
       [900, "TIMESTAMP(9)",                      :timestamp,     nil, true, false, 0,  11,   0,    9,    9,    0],
       [900, "TIMESTAMP WITH TIME ZONE",          :timestamp_tz,  nil, true, false, 0,  13,   0,    6,    6,    0],
       [900, "TIMESTAMP(9) WITH TIME ZONE",       :timestamp_tz,  nil, true, false, 0,  13,   0,    9,    9,    0],
       [900, "TIMESTAMP WITH LOCAL TIME ZONE",    :timestamp_ltz, nil, true, false, 0,  11,   0,    6,    6,    0],
       [900, "TIMESTAMP(9) WITH LOCAL TIME ZONE", :timestamp_ltz, nil, true, false, 0,  11,   0,    9,    9,    0],
       [900, "INTERVAL YEAR TO MONTH",      :interval_ym, nil, true,  false,  0,         5,   2,    0,    0,    2],
       [900, "INTERVAL YEAR(4) TO MONTH",   :interval_ym, nil, true,  false,  0,         5,   4,    0,    0,    4],
       [900, "INTERVAL DAY TO SECOND",      :interval_ds, nil, true,  false,  0,        11,   2,    6,    6,    2],
       [900, "INTERVAL DAY(4) TO SECOND(9)",:interval_ds, nil, true,  false,  0,        11,   4,    9,    9,    4],
      ]

    coldef.reject! do |c| c[0] > $oracle_version end

    drop_table('test_table')
    sql = <<-EOS
CREATE TABLE test_table (#{i = 0; coldef.collect do |c| i += 1; "C#{i} " + c[1]; end.join(',')})
STORAGE (
   INITIAL 100k
   NEXT 100k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)

    # temporarily change OCI8::BindType::Mapping.
    saved_mapping = {}
    [OCI8::SQLT_TIMESTAMP_TZ,
     OCI8::SQLT_TIMESTAMP_LTZ,
     OCI8::SQLT_INTERVAL_YM,
     OCI8::SQLT_INTERVAL_DS].each do |sqlt_type|
      saved_mapping[sqlt_type] = OCI8::BindType::Mapping[sqlt_type]
      OCI8::BindType::Mapping[sqlt_type] = OCI8::BindType::String
    end
    begin
      cursor = @conn.exec("SELECT * FROM test_table")
    ensure
      saved_mapping.each do |key, val|
        OCI8::BindType::Mapping[key] = val
      end
    end
    cursor.column_metadata.each_with_index do |md, i|
      # common
      assert_equal("C#{i + 1}", md.name, "'#{coldef[i][1]}': name")
      assert_equal(coldef[i][1], md.to_s, "'#{coldef[i][1]}': to_s")
      assert_equal(coldef[i][2], md.data_type, "'#{coldef[i][1]}': data_type")
      assert_equal(coldef[i][3], md.charset_form, "'#{coldef[i][1]}': charset_form")
      assert_equal(coldef[i][4], md.is_null?, "'#{coldef[i][1]}': is_null? ")
      # string type
      if $oracle_version >=  900
        assert_equal(coldef[i][5], md.char_used?, "'#{coldef[i][1]}': char_used? ")
        assert_equal(coldef[i][6], md.char_size, "'#{coldef[i][1]}': char_size")
      end
      assert_equal(coldef[i][7], md.data_size, "'#{coldef[i][1]}': data_size")
      # number, timestamp and interval type
      assert_equal(coldef[i][8], md.precision, "'#{coldef[i][1]}': precision")
      assert_equal(coldef[i][9], md.scale, "'#{coldef[i][1]}': scale")
      assert_equal(coldef[i][10], md.fsprecision, "'#{coldef[i][1]}': fsprecision")
      assert_equal(coldef[i][11], md.lfprecision, "'#{coldef[i][1]}': lfprecision")
    end
    drop_table('test_table')
  end
end # TestMetadata

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestMetadata.suite())
end
