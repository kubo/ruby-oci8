require 'oci8'
require 'test/unit'
require File.dirname(__FILE__) + '/config'

class TestMetadata < Test::Unit::TestCase

  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  def drop_type(name, drop_body = false)
    if drop_body
      begin
        @conn.exec("DROP TYPE BODY #{name}")
      rescue OCIError
        raise if $!.code != 4043
      end
    end
    begin
      @conn.exec("DROP TYPE #{name}")
    rescue OCIError
      raise if $!.code != 4043
    end
  end

  class DatatypeData
    @@attributes =
      [:data_type_string,
       :data_type,
       :charset_form,
       :nullable?,
       :data_size,
       :precision,
       :scale,
      ]
    @@attributes +=
      [
       :char_used?,
       :char_size,
       :fsprecision,
       :lfprecision,
      ] if $oracle_version >= OCI8::ORAVER_9_0

    @@attributes.each do |attr|
      define_method attr do
        @table[attr]
      end
    end

    def self.attributes
      @@attributes
    end

    def initialize(hash = {})
      @table = hash
    end

    def available?(conn)
      return false if $oracle_version < @table[:oraver]
      if /^(\w+)\.(\w+)$/ =~ @table[:data_type_string]
        if conn.select_one('select 1 from all_objects where owner = :1 and object_name = :2', $1, $2)
          true
        else
          warn "skip a test for unsupported datatype: #{@table[:data_type_string]}."
          false
        end
      else
        true
      end
    end
  end

  # Get data_size of NCHAR(1) and that of CHAR(1 CHAR).
  # They depend on the database character set and the
  # client character set.
  conn = OCI8.new($dbuser, $dbpass, $dbname)
  begin
    cursor = conn.exec("select N'1' from dual")
    # cfrm: data_size of NCHAR(1).
    cfrm = cursor.column_metadata[0].data_size
    if $oracle_version >=  OCI8::ORAVER_9_0
      # csem: data_size of CHAR(1 CHAR).
      cursor = conn.exec("select CAST('1' AS CHAR(1 char)) from dual")
      csem = cursor.column_metadata[0].data_size
    else
      csem = 1
    end
  ensure
    conn.logoff
  end

  ora80 = OCI8::ORAVER_8_0
  ora81 = OCI8::ORAVER_8_1
  ora90 = OCI8::ORAVER_9_0
  ora101 = OCI8::ORAVER_10_1

  @@column_test_data =
    [
     DatatypeData.new(:data_type_string => "CHAR(10) NOT NULL",
                      :oraver => ora80,
                      :data_type => :char,
                      :charset_form => :implicit,
                      :nullable? => false,
                      :char_used? => false,
                      :char_size => 10,
                      :data_size => 10,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "CHAR(10 CHAR)",
                      :oraver => ora90,
                      :data_type => :char,
                      :charset_form => :implicit,
                      :nullable? => true,
                      :char_used? => true,
                      :char_size => 10,
                      :data_size => 10 * csem,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "NCHAR(10)",
                      :oraver => ora80,
                      :data_type => :char,
                      :charset_form => :nchar,
                      :nullable? => true,
                      :char_used? => true,
                      :char_size => 10,
                      :data_size => 10 * cfrm,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "VARCHAR2(10)",
                      :oraver => ora80,
                      :data_type => :varchar2,
                      :charset_form => :implicit,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 10,
                      :data_size => 10,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "VARCHAR2(10 CHAR)",
                      :oraver => ora90,
                      :data_type => :varchar2,
                      :charset_form => :implicit,
                      :nullable? => true,
                      :char_used? => true,
                      :char_size => 10,
                      :data_size => 10 * csem,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "NVARCHAR2(10)",
                      :oraver => ora80,
                      :data_type => :varchar2,
                      :charset_form => :nchar,
                      :nullable? => true,
                      :char_used? => true,
                      :char_size => 10,
                      :data_size => 10 * cfrm,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "RAW(10)",
                      :oraver => ora80,
                      :data_type => :raw,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 10,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),

     # Skip tests for data_size of CLOB, NCLOB and BLOB
     # because their values depend on how they are described.
     #
     #  Oracle 10g XE 10.2.0.1.0 on Linux:
     #   +----------------+-----------+
     #   |                | data_size |
     #   +----------------+-----------+
     #   | implicitly(*1) |   4000    |
     #   | explicitly(*2) |     86    |
     #   +----------------+-----------+
     #
     # *1 explicitly described by column definition.
     # *2 implicitly described by select list.
     DatatypeData.new(:data_type_string => "CLOB",
                      :oraver => ora81,
                      :data_type => :clob,
                      :charset_form => :implicit,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => :skip,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "NCLOB",
                      :oraver => ora81,
                      :data_type => :clob,
                      :charset_form => :nchar,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => :skip,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "BLOB",
                      :oraver => ora80,
                      :data_type => :blob,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => :skip,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "BFILE",
                      :oraver => ora80,
                      :data_type => :bfile,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 530,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),

     # Skip tests for fsprecision and lfprecision for NUMBER and FLOAT
     # because their values depend on how they are described.
     #
     #  Oracle 10g XE 10.2.0.1.0 on Linux:
     #   +-----------------------------+-------------+-------------+
     #   |                             | fsprecision | lfprecision |
     #   +----------------+------------+-------------+-------------+
     #   | NUMBER         | implicitly |     129     |      0      |
     #   |                | explicitly |       0     |    129      |
     #   +----------------+------------+-------------+-------------+
     #   | NUMBER(10)     | implicitly |       0     |     10      |
     #   |                | explicitly |      10     |      0      |
     #   +----------------+------------+-------------+-------------+
     #   | NUMBER(10,2)   | implicitly |       2     |     10      |
     #   |                | explicitly |      10     |      2      |
     #   +----------------+------------+-------------+-------------+
     #   | FLOAT          | implicitly |     129     |    126      |
     #   |                | explicitly |     126     |    129      |
     #   +----------------+------------+-------------+-------------+
     #   | FLOAT(10)      | implicitly |     129     |     10      |
     #   |                | explicitly |      10     |    129      |
     #   +----------------+------------+-------------+-------------+
     DatatypeData.new(:data_type_string => "NUMBER",
                      :oraver => ora80,
                      :data_type => :number,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 22,
                      :precision => 0,
                      :scale => $oracle_version > ora90 ? -127 : 0,
                      :fsprecision => :skip,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "NUMBER(10)",
                      :oraver => ora80,
                      :data_type => :number,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 22,
                      :precision => 10,
                      :scale => 0,
                      :fsprecision => :skip,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "NUMBER(10,2)",
                      :oraver => ora80,
                      :data_type => :number,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 22,
                      :precision => 10,
                      :scale => 2,
                      :fsprecision => :skip,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "FLOAT",
                      :oraver => ora80,
                      :data_type => :number,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 22,
                      :precision => 126,
                      :scale => -127,
                      :fsprecision => :skip,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "FLOAT(10)",
                      :oraver => ora80,
                      :data_type => :number,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 22,
                      :precision => 10,
                      :scale => -127,
                      :fsprecision => :skip,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "BINARY_FLOAT",
                      :oraver => ora101,
                      :data_type => :binary_float,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 4,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "BINARY_DOUBLE",
                      :oraver => ora101,
                      :data_type => :binary_double,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 8,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
     DatatypeData.new(:data_type_string => "DATE",
                      :oraver => ora80,
                      :data_type => :date,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 7,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),

     # Skip tests for precision and lfprecision for TIMESTAMP
     # because their values depend on how they are described.
     #
     #  Oracle 10g XE 10.2.0.1.0 on Linux:
     #   +------------------------------------------------+-----------+-------------+
     #   |                                                | precision | lfprecision |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP                         | implicitly |     0     |      0      |
     #   |                                   | explicitly |     6     |      6      |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP(9)                      | implicitly |     0     |      0      |
     #   |                                   | explicitly |     9     |      9      |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP WITH TIME ZONE          | implicitly |     0     |      0      |
     #   |                                   | explicitly |     6     |      6      |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP(9) WITH TIME ZONE       | implicitly |     0     |      0      |
     #   |                                   | explicitly |     9     |      9      |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP WITH LOCAL TIME ZONE    | implicitly |     0     |      0      |
     #   |                                   | explicitly |     6     |      6      |
     #   +-----------------------------------+------------+-----------+-------------+
     #   | TIMESTAMP(9) WITH LOCAL TIME ZONE | implicitly |     0     |      0      |
     #   |                                   | explicitly |     9     |      9      |
     #   +-----------------------------------+------------+-----------+-------------+
     DatatypeData.new(:data_type_string => "TIMESTAMP",
                      :oraver => ora90,
                      :data_type => :timestamp,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => 6,
                      :fsprecision => 6,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "TIMESTAMP(9)",
                      :oraver => ora90,
                      :data_type => :timestamp,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => 9,
                      :fsprecision => 9,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "TIMESTAMP WITH TIME ZONE",
                      :oraver => ora90,
                      :data_type => :timestamp_tz,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 13,
                      :precision => :skip,
                      :scale => 6,
                      :fsprecision => 6,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "TIMESTAMP(9) WITH TIME ZONE",
                      :oraver => ora90,
                      :data_type => :timestamp_tz,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 13,
                      :precision => :skip,
                      :scale => 9,
                      :fsprecision => 9,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "TIMESTAMP WITH LOCAL TIME ZONE",
                      :oraver => ora90,
                      :data_type => :timestamp_ltz,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => 6,
                      :fsprecision => 6,
                      :lfprecision => :skip
                      ),
     DatatypeData.new(:data_type_string => "TIMESTAMP(9) WITH LOCAL TIME ZONE",
                      :oraver => ora90,
                      :data_type => :timestamp_ltz,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => 9,
                      :fsprecision => 9,
                      :lfprecision => :skip
                      ),

     # Skip tsets for scale and fsprecision for INTERVAL YEAR TO MONTH
     # because their values depend on how they are described.
     #
     #  Oracle 10g XE 10.2.0.1.0 on Linux:
     #   +-------------------------------------------+-----------+-------------+
     #   |                                           |   scale   | fsprecision |
     #   +------------------------------+------------+-----------+-------------+
     #   | INTERVAL YEAR TO MONTH       | implicitly |     0     |      0      |
     #   |                              | explicitly |     2     |      2      |
     #   +------------------------------+------------+-----------+-------------+
     #   | INTERVAL YEAR(4) TO MONTH    | implicitly |     0     |      0      |
     #   |                              | explicitly |     4     |      4      |
     #   +------------------------------+------------+-----------+-------------+
     DatatypeData.new(:data_type_string => "INTERVAL YEAR TO MONTH",
                      :oraver => ora90,
                      :data_type => :interval_ym,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 5,
                      :precision => 2,
                      :scale => :skip,
                      :fsprecision => :skip,
                      :lfprecision => 2
                      ),
     DatatypeData.new(:data_type_string => "INTERVAL YEAR(4) TO MONTH",
                      :oraver => ora90,
                      :data_type => :interval_ym,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 5,
                      :precision => 4,
                      :scale => :skip,
                      :fsprecision => :skip,
                      :lfprecision => 4
                      ),
     # Skip tests for precision and scale for INTERVAL DAY TO SECOND
     # because their values depend on how they are described.
     #
     #  Oracle 10g XE 10.2.0.1.0 on Linux:
     #   +-------------------------------------------+-----------+-----------+
     #   |                                           | precision |   scale   |
     #   +------------------------------+------------+-----------+-----------+
     #   | INTERVAL DAY TO SECOND       | implicitly |     2     |     6     |
     #   |                              | explicitly |     6     |     2     |
     #   +------------------------------+------------+-----------+-----------+
     #   | INTERVAL DAY(4) TO SECOND(9) | implicitly |     4     |     9     |
     #   |                              | explicitly |     9     |     4     |
     #   +------------------------------+------------+-----------+-----------+
     DatatypeData.new(:data_type_string => "INTERVAL DAY TO SECOND",
                      :oraver => ora90,
                      :data_type => :interval_ds,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => :skip,
                      :fsprecision => 6,
                      :lfprecision => 2
                      ),
     DatatypeData.new(:data_type_string => "INTERVAL DAY(4) TO SECOND(9)",
                      :oraver => ora90,
                      :data_type => :interval_ds,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => 11,
                      :precision => :skip,
                      :scale => :skip,
                      :fsprecision => 9,
                      :lfprecision => 4
                      ),
     # Object Types
     DatatypeData.new(:data_type_string => "MDSYS.SDO_GEOMETRY",
                      :oraver => ora101,
                      :data_type => :named_type,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => :skip, # 1 when explicitly, 2000 when implicitly.
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0
                      ),
=begin # uncomment after ref is supported.
     DatatypeData.new(:data_type_string => "REF MDSYS.SDO_GEOMETRY",
                      :oraver => ora101,
                      :data_type => :ref,
                      :charset_form => nil,
                      :nullable? => true,
                      :char_used? => false,
                      :char_size => 0,
                      :data_size => :skip,
                      :precision => 0,
                      :scale => 0,
                      :fsprecision => 0,
                      :lfprecision => 0,
                      ),
=end
    ]

  def test_error_describe_table
    drop_table('test_table')
    begin
      @conn.describe_table('test_table')
      flunk("expects ORA-4043 but no error")
    rescue OCIError
      flunk("expects ORA-4043 but ORA-#{$!.code}") if $!.code != 4043
    end
    @conn.exec('create sequence test_table')
    begin
      begin
        @conn.describe_table('test_table')
        flunk('expects ORA-4043 but no error')
      rescue OCIError
        flunk("expects ORA-4043 but ORA-#{$!.code}") if $!.code != 4043
      end
    ensure
      @conn.exec('drop sequence test_table')
    end
  end

  def assert_object_id(object_name, object_id, owner_name = nil)
    owner_name ||= @conn.username
    expected_val = @conn.select_one('select object_id from all_objects where owner = :1 and object_name = :2', owner_name, object_name)[0]
    assert_equal(expected_val, object_id, "ID of #{object_name}")
  end

  def test_table_metadata
    drop_table('test_table')

    # Relational table
    @conn.exec(<<-EOS)
CREATE TABLE test_table (col1 number(38,0), col2 varchar2(60))
STORAGE (
   INITIAL 100k
   NEXT 100k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      assert_object_id('TEST_TABLE', desc.obj_id)
      assert_equal('TEST_TABLE', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(2, desc.num_cols)
      assert_nil(desc.type_metadata)
      assert_equal(false, desc.is_temporary?)
      assert_equal(false, desc.is_typed?)
      assert_nil(desc.duration)
      assert_not_nil(desc.dba)
      assert_not_nil(desc.tablespace)
      assert_equal(false, desc.clustered?)
      assert_equal(false, desc.partitioned?)
      assert_equal(false, desc.index_only?)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    drop_table('test_table')

    # Transaction-specific temporary table
    @conn.exec(<<-EOS)
CREATE GLOBAL TEMPORARY TABLE test_table (col1 number(38,0), col2 varchar2(60))
EOS
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      assert_object_id('TEST_TABLE', desc.obj_id)
      assert_equal('TEST_TABLE', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(2, desc.num_cols)
      assert_nil(desc.type_metadata)
      assert_equal(true, desc.is_temporary?)
      assert_equal(false, desc.is_typed?)
      assert_equal(:transaction, desc.duration)
      assert_not_nil(desc.dba)
      assert_not_nil(desc.tablespace)
      assert_equal(false, desc.clustered?)
      assert_equal(false, desc.partitioned?)
      assert_equal(false, desc.index_only?)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    drop_table('test_table')

    # Session-specific temporary table
    @conn.exec(<<-EOS)
CREATE GLOBAL TEMPORARY TABLE test_table (col1 number(38,0), col2 varchar2(60))
ON COMMIT PRESERVE ROWS
EOS
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      assert_object_id('TEST_TABLE', desc.obj_id)
      assert_equal('TEST_TABLE', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(2, desc.num_cols)
      assert_nil(desc.type_metadata)
      assert_equal(true, desc.is_temporary?)
      assert_equal(false, desc.is_typed?)
      assert_equal(:session, desc.duration)
      assert_not_nil(desc.dba)
      assert_not_nil(desc.tablespace)
      assert_equal(false, desc.clustered?)
      assert_equal(false, desc.partitioned?)
      assert_equal(false, desc.index_only?)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    drop_table('test_table')

    # Object table
    @conn.exec(<<-EOS)
CREATE OR REPLACE TYPE test_type AS OBJECT (col1 number(38,0), col2 varchar2(60))
EOS
    @conn.exec(<<-EOS)
CREATE TABLE test_table OF test_type
EOS
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      assert_object_id('TEST_TABLE', desc.obj_id)
      assert_equal('TEST_TABLE', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(2, desc.num_cols)
      assert_instance_of(OCI8::Metadata::Type, desc.type_metadata)
      assert_equal(false, desc.is_temporary?)
      assert_equal(true, desc.is_typed?)
      assert_equal(nil, desc.duration)
      assert_not_nil(desc.dba)
      assert_not_nil(desc.tablespace)
      assert_equal(false, desc.clustered?)
      assert_equal(false, desc.partitioned?)
      assert_equal(false, desc.index_only?)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    drop_table('test_table')
    @conn.exec('DROP TYPE TEST_TYPE')

    # Index-organized table
    @conn.exec(<<-EOS)
CREATE TABLE test_table (col1 number(38,0) PRIMARY KEY, col2 varchar2(60))
ORGANIZATION INDEX 
EOS
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      assert_object_id('TEST_TABLE', desc.obj_id)
      assert_equal('TEST_TABLE', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(2, desc.num_cols)
      assert_nil(desc.type_metadata)
      assert_equal(false, desc.is_temporary?)
      assert_equal(false, desc.is_typed?)
      assert_equal(nil, desc.duration)
      assert_not_nil(desc.dba)
      assert_not_nil(desc.tablespace)
      assert_equal(false, desc.clustered?)
      assert_equal(false, desc.partitioned?)
      assert_equal(true, desc.index_only?)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    drop_table('test_table')
  end # test_table_metadata

  def test_view_metadata
    @conn.exec('CREATE OR REPLACE VIEW test_view as SELECT * FROM tab')
    [
     @conn.describe_any('test_view'),
     @conn.describe_view('test_view'),
     @conn.describe_table('test_view'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_VIEW'
     end
    ].each do |desc|
      assert_object_id('TEST_VIEW', desc.obj_id)
      assert_equal('TEST_VIEW', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(3, desc.num_cols)
      assert_instance_of(Array, desc.columns)
      assert_instance_of(OCI8::Metadata::Column, desc.columns[0])
    end
    @conn.exec('DROP VIEW test_view')
  end # test_view_metadata

  def test_procedure_metadata
    @conn.exec(<<-EOS)
CREATE OR REPLACE PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2) IS
BEGIN
  NULL;
END;
EOS
    [
     @conn.describe_any('test_proc'),
     @conn.describe_procedure('test_proc'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PROC'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Procedure, desc)
      assert_object_id('TEST_PROC', desc.obj_id)
      assert_equal('TEST_PROC', desc.obj_name)
      assert_equal('TEST_PROC', desc.name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(false, desc.is_invoker_rights?)
      assert_equal(nil, desc.overload_id)
      assert_instance_of(Array, desc.arguments)
      assert_equal(2, desc.arguments.length)
      assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2)
  AUTHID CURRENT_USER
IS
BEGIN
  NULL;
END;
EOS
    [
     @conn.describe_any('test_proc'),
     @conn.describe_procedure('test_proc'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PROC'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Procedure, desc)
      assert_object_id('TEST_PROC', desc.obj_id)
      assert_equal('TEST_PROC', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(true, desc.is_invoker_rights?)
      assert_equal(nil, desc.overload_id)
      assert_instance_of(Array, desc.arguments)
      assert_equal(2, desc.arguments.length)
      assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])
    end

    @conn.exec('DROP PROCEDURE test_proc');

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG IS
  PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2);
END;
EOS
    desc = @conn.describe_package('test_pkg').subprograms[0]
    assert_instance_of(OCI8::Metadata::Procedure, desc)
    assert_equal(nil, desc.obj_id)
    assert_equal('TEST_PROC', desc.obj_name)
    assert_equal(nil, desc.obj_schema)
    assert_equal(false, desc.is_invoker_rights?)
    assert_equal(0, desc.overload_id)
    assert_instance_of(Array, desc.arguments)
    assert_equal(2, desc.arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG AUTHID CURRENT_USER
IS
  PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2);
  PROCEDURE test_proc(arg1 IN INTEGER);
END;
EOS
    desc = @conn.describe_package('test_pkg').subprograms
    assert_instance_of(OCI8::Metadata::Procedure, desc[0])
    assert_equal(nil, desc[0].obj_id)
    assert_equal('TEST_PROC', desc[0].obj_name)
    assert_equal(nil, desc[0].obj_schema)
    assert_equal(true, desc[0].is_invoker_rights?)
    assert_equal(2, desc[0].overload_id)
    assert_instance_of(Array, desc[0].arguments)
    assert_equal(2, desc[0].arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc[0].arguments[0])

    descs = @conn.describe_package('test_pkg').subprograms
    assert_instance_of(OCI8::Metadata::Procedure, desc[1])
    assert_equal(nil, desc[1].obj_id)
    assert_equal('TEST_PROC', desc[1].obj_name)
    assert_equal(nil, desc[1].obj_schema)
    assert_equal(true, desc[1].is_invoker_rights?)
    assert_equal(1, desc[1].overload_id)
    assert_instance_of(Array, desc[1].arguments)
    assert_equal(1, desc[1].arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc[1].arguments[0])
  end # test_procedure_metadata

  def test_function_metadata
    @conn.exec(<<-EOS)
CREATE OR REPLACE FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER IS
BEGIN
  RETURN arg1;
END;
EOS
    [
     @conn.describe_any('test_func'),
     @conn.describe_function('test_func'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_FUNC'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Function, desc)
      assert_object_id('TEST_FUNC', desc.obj_id)
      assert_equal('TEST_FUNC', desc.obj_name)
      assert_equal('TEST_FUNC', desc.name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(false, desc.is_invoker_rights?)
      assert_equal(nil, desc.overload_id)
      assert_instance_of(Array, desc.arguments)
      assert_equal(3, desc.arguments.length)
      assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER
  AUTHID CURRENT_USER
IS
BEGIN
  RETURN arg1;
END;
EOS
    [
     @conn.describe_any('test_func'),
     @conn.describe_function('test_func'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_FUNC'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Function, desc)
      assert_object_id('TEST_FUNC', desc.obj_id)
      assert_equal('TEST_FUNC', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(true, desc.is_invoker_rights?)
      assert_equal(nil, desc.overload_id)
      assert_instance_of(Array, desc.arguments)
      assert_equal(3, desc.arguments.length)
      assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])
    end

    @conn.exec('DROP FUNCTION test_func');

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG IS
  FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER;
END;
EOS
    desc = @conn.describe_package('test_pkg').subprograms[0]
    assert_instance_of(OCI8::Metadata::Function, desc)
    assert_equal(nil, desc.obj_id)
    assert_equal('TEST_FUNC', desc.obj_name)
    assert_equal(nil, desc.obj_schema)
    assert_equal(false, desc.is_invoker_rights?)
    assert_equal(0, desc.overload_id)
    assert_instance_of(Array, desc.arguments)
    assert_equal(3, desc.arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc.arguments[0])

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG AUTHID CURRENT_USER
IS
  FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER;
  FUNCTION test_func(arg1 IN INTEGER) RETURN NUMBER;
END;
EOS
    desc = @conn.describe_package('test_pkg').subprograms
    assert_instance_of(OCI8::Metadata::Function, desc[0])
    assert_equal(nil, desc[0].obj_id)
    assert_equal('TEST_FUNC', desc[0].obj_name)
    assert_equal(nil, desc[0].obj_schema)
    assert_equal(true, desc[0].is_invoker_rights?)
    assert_equal(2, desc[0].overload_id)
    assert_instance_of(Array, desc[0].arguments)
    assert_equal(3, desc[0].arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc[0].arguments[0])

    descs = @conn.describe_package('test_pkg').subprograms
    assert_instance_of(OCI8::Metadata::Function, desc[1])
    assert_equal(nil, desc[1].obj_id)
    assert_equal('TEST_FUNC', desc[1].obj_name)
    assert_equal(nil, desc[1].obj_schema)
    assert_equal(true, desc[1].is_invoker_rights?)
    assert_equal(1, desc[1].overload_id)
    assert_instance_of(Array, desc[1].arguments)
    assert_equal(2, desc[1].arguments.length)
    assert_instance_of(OCI8::Metadata::Argument, desc[1].arguments[0])
  end # test_function_metadata

  def test_package_metadata
    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG IS
  FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER;
END;
EOS
    [
     @conn.describe_any('test_pkg'),
     @conn.describe_package('test_pkg'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PKG'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Package, desc)
      assert_object_id('TEST_PKG', desc.obj_id)
      assert_equal('TEST_PKG', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(false, desc.is_invoker_rights?)
      assert_instance_of(Array, desc.subprograms)
      assert_equal(1, desc.subprograms.length)
      assert_instance_of(OCI8::Metadata::Function, desc.subprograms[0])
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG AUTHID CURRENT_USER IS
  PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2);
END;
EOS
    [
     @conn.describe_any('test_pkg'),
     @conn.describe_package('test_pkg'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PKG'
     end
    ].each do |desc|
      assert_instance_of(OCI8::Metadata::Package, desc)
      assert_object_id('TEST_PKG', desc.obj_id)
      assert_equal('TEST_PKG', desc.obj_name)
      assert_equal(@conn.username, desc.obj_schema)
      assert_equal(true, desc.is_invoker_rights?)
      assert_instance_of(Array, desc.subprograms)
      assert_equal(1, desc.subprograms.length)
      assert_instance_of(OCI8::Metadata::Procedure, desc.subprograms[0])
    end
  end # test_package_metadata

  def test_type_metadata
    drop_type('TEST_TYPE_ORDER_METHOD')
    drop_type('TEST_TYPE_MAP_METHOD')
    drop_type('TEST_TYPE_HAS_BFILE')
    drop_type('TEST_TYPE_HAS_BLOB')
    drop_type('TEST_TYPE_HAS_NCLOB')
    drop_type('TEST_TYPE_HAS_CLOB')
    drop_type('TEST_TYPE_INCOMPLETE')
    drop_type('TEST_TYPE_GRANDCHILD')
    drop_type('TEST_TYPE_VARRAY')
    drop_type('TEST_TYPE_NESTEAD_TABLE')
    drop_type('TEST_TYPE_CHILD')
    drop_type('TEST_TYPE_PARENT')
    expected_values = []

    @conn.exec(<<-EOS)
CREATE TYPE test_type_parent AS OBJECT (
  col1 number(38,0),
  col2 varchar2(60)
)
NOT INSTANTIABLE
NOT FINAL
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_PARENT',
      :typecode => :named_type,
      :collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => nil,
      :num_type_attrs => 2,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => false,
      :is_instantiable_type? => false,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :type_attrs => [:array, 2, OCI8::Metadata::TypeAttr],
      :type_methods => [:array, 0],
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_child UNDER test_type_parent (
  lob BLOB
)
NOT FINAL
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_CHILD',
      :typecode => :named_type,
      :collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => true,
      :has_file? => false,
      :collection_element => nil,
      :num_type_attrs => 3,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => false,
      :is_instantiable_type? => true,
      :is_subtype? => true,
      :supertype_schema_name => @conn.username,
      :supertype_name => 'TEST_TYPE_PARENT',
      :type_attrs => [:array, 3, OCI8::Metadata::TypeAttr],
      :type_methods => [:array, 0],
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_nestead_table AS TABLE OF test_type_child
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_NESTEAD_TABLE',
      :typecode => :named_collection,
      :collection_typecode => :table,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => true,
      :has_file? => false,
      :collection_element => [:type, OCI8::Metadata::Collection],
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :type_attrs => [:array, 0],
      :type_methods => [:array, 0],
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_varray AS VARRAY(10) OF test_type_child
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_VARRAY',
      :typecode => :named_collection,
      :collection_typecode => :varray,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => true,
      :has_file? => false,
      :collection_element => [:type, OCI8::Metadata::Collection],
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :type_attrs => [:array, 0],
      :type_methods => [:array, 0],
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_grandchild UNDER test_type_child (
  table_column test_type_nestead_table,
  file_column BFILE
)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_GRANDCHILD',
      :typecode => :named_type,
      :collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => true,
      :has_file? => true,
      :collection_element => nil,
      :num_type_attrs => 5,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => true,
      :supertype_schema_name => @conn.username,
      :supertype_name => 'TEST_TYPE_CHILD',
      :type_attrs => [:array, 5, OCI8::Metadata::TypeAttr],
      :type_methods => [:array, 0],
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_incomplete
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_INCOMPLETE',
      :typecode => :named_type,
      :collection_typecode => nil,
      :is_incomplete_type? => true,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => nil,
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :type_attrs => [:array, 0],
      :type_methods => [:array, 0],
    }

    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_clob AS OBJECT (lob CLOB)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_HAS_CLOB',
      :has_lob? => true,
      :has_file? => false,
    }
    if $oracle_version >= OCI8::ORAVER_9_2
      @conn.exec(<<-EOS)
CREATE TYPE test_type_has_nclob AS OBJECT (lob NCLOB)
EOS
      expected_values << {
        :obj_name => 'TEST_TYPE_HAS_NCLOB',
        :has_lob? => true,
        :has_file? => false,
      }
    end
    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_blob AS OBJECT (lob BLOB)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_HAS_BLOB',
      :has_lob? => true,
      :has_file? => false,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_bfile AS OBJECT (lob BFILE)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_HAS_BFILE',
      :has_lob? => false,
      :has_file? => true,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_map_method AS OBJECT (
  x integer,
  y integer,
  MAP MEMBER FUNCTION area RETURN NUMBER
)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_MAP_METHOD',
      :map_method => [:type, OCI8::Metadata::TypeMethod],
      :order_method => nil,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_order_method AS OBJECT (
  x integer,
  y integer,
  ORDER MEMBER FUNCTION match(l test_type_order_method) RETURN INTEGER
)
EOS
    expected_values << {
      :obj_name => 'TEST_TYPE_ORDER_METHOD',
      :map_method => nil,
      :order_method => [:type, OCI8::Metadata::TypeMethod],
    }

    expected_values.each do |elem|
      [
       @conn.describe_any(elem[:obj_name]),
       @conn.describe_type(elem[:obj_name]),
       @conn.describe_schema(@conn.username).objects.detect do |obj|
         obj.obj_name == elem[:obj_name]
       end,
      ].each do |desc|
        assert_object_id(elem[:obj_name], desc.obj_id)
        assert_equal(@conn.username, desc.obj_schema)
        assert_equal(elem[:obj_name], desc.name)
        assert_equal(@conn.username, desc.schema_name)

        elem.each do |key, val|
          msg = elem[:obj_name] + '.' + key.to_s
          if val.is_a? Array
            case val[0]
            when :array
              assert_instance_of(Array, desc.send(key), msg)
              assert_equal(val[1], desc.send(key).length)
              assert_instance_of(val[2], desc.send(key)[0]) if val[1] > 0
            when :type
              assert_instance_of(val[1], desc.send(key), msg)
            else
              raise "Invalid test case: #{elem[:obj_name]}.#{key} : #{val[0]}"
            end
          else
            assert_equal(val, desc.send(key), msg)
          end
        end
      end
    end

    drop_type('TEST_TYPE_ORDER_METHOD')
    drop_type('TEST_TYPE_MAP_METHOD')
    drop_type('TEST_TYPE_HAS_BFILE')
    drop_type('TEST_TYPE_HAS_BLOB')
    drop_type('TEST_TYPE_HAS_NCLOB')
    drop_type('TEST_TYPE_HAS_CLOB')
    drop_type('TEST_TYPE_INCOMPLETE')
    drop_type('TEST_TYPE_GRANDCHILD')
    drop_type('TEST_TYPE_VARRAY')
    drop_type('TEST_TYPE_NESTEAD_TABLE')
    drop_type('TEST_TYPE_CHILD')
    drop_type('TEST_TYPE_PARENT')
  end # test_type_metadata

  def test_column_metadata
    if $oracle_version < OCI8::ORAVER_8_1
      begin
        @conn.describe_table('tab').columns
      rescue RuntimeError
        assert_equal("This feature is unavailable on Oracle 8.0", $!.to_s)
      end
      return
    end

    coldef = @@column_test_data.find_all do |c|
      c.available?(@conn)
    end

    drop_table('test_table')
    sql = <<-EOS
CREATE TABLE test_table (#{idx = 0; coldef.collect do |c| idx += 1; "C#{idx} " + c.data_type_string; end.join(',')})
STORAGE (
   INITIAL 100k
   NEXT 100k
   MINEXTENTS 1
   MAXEXTENTS UNLIMITED
   PCTINCREASE 0)
EOS
    @conn.exec(sql)

    [
     @conn.describe_any('test_table').columns,
     @conn.describe_table('test_table').columns,
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end.columns,
     @conn.exec('select * from test_table').column_metadata,
    ].each do |columns|
        columns.each_with_index do |column, i|
        assert_equal("C#{i + 1}", column.name, "'#{coldef[i].data_type_string}': name")
        DatatypeData.attributes.each do |attr|
          expected_val = coldef[i].send(attr)
          if expected_val != :skip
            assert_equal(expected_val, column.send(attr), "'#{coldef[i].data_type_string}': #{attr})")
          end
        end
      end
    end
    drop_table('test_table')
  end # test_column_metadata

  def assert_sequence(sequence_name, desc)
    # defined in OCI8::Metadata::Base
    assert_object_id(sequence_name, desc.obj_id)
    assert_equal(sequence_name, desc.obj_name, 'obj_name')
    assert_equal(@conn.username, desc.obj_schema, 'obj_schema')
    # defined in OCI8::Metadata::Sequence
    assert_object_id(sequence_name, desc.objid)
    min, max, incr, cache, order = @conn.select_one(<<EOS, sequence_name)
select min_value, max_value, increment_by, cache_size, order_flag
  from user_sequences
 where sequence_name = :1
EOS
    min = min.to_i
    max = max.to_i
    incr = incr.to_i
    cache = cache.to_i
    order = order == 'Y'
    currval = @conn.select_one("select cast(#{sequence_name}.currval as integer) from dual")[0]

    assert_equal(min, desc.min, 'min')
    assert_equal(max, desc.max, 'max')
    assert_equal(incr, desc.incr, 'incr')
    assert_equal(cache, desc.cache, 'cache')
    assert_equal(order, desc.order?, 'order?')
    assert_operator(currval, :<=, desc.hw_mark, 'hw_mark')
  end

  def test_sequence
    begin
      @conn.exec("DROP SEQUENCE TEST_SEQ_OCI8")
    rescue OCIError
      raise if $!.code != 2289 # ORA-02289: sequence does not exist
    end
    @conn.exec(<<-EOS)
CREATE SEQUENCE TEST_SEQ_OCI8
    INCREMENT BY 2
    START WITH 998
    MAXVALUE 1000
    MINVALUE 10
    CYCLE
    CACHE 5
    ORDER
EOS
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_any('test_seq_oci8'))
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_sequence('test_seq_oci8'))
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_schema(@conn.username).objects.detect do |obj|
                      obj.obj_name == 'TEST_SEQ_OCI8'
                    end)

    @conn.exec("DROP SEQUENCE TEST_SEQ_OCI8")
    @conn.exec(<<-EOS)
CREATE SEQUENCE TEST_SEQ_OCI8
    INCREMENT BY -1
    NOMAXVALUE
    NOMINVALUE
    NOCYCLE
    NOCACHE
    NOORDER
EOS
    # @conn.describe_any('test_seq_oci8').min is:
    #   -99999999999999999999999999 on Oracle 10gR2
    #   -999999999999999999999999999 on Oracle 11gR2

    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_any('test_seq_oci8'))
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_sequence('test_seq_oci8'))
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_schema(@conn.username).objects.detect do |obj|
                      obj.obj_name == 'TEST_SEQ_OCI8'
                    end)

    @conn.exec("DROP SEQUENCE TEST_SEQ_OCI8")
  end

end # TestMetadata
