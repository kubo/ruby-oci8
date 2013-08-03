require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestMetadata < MiniTest::Unit::TestCase

  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff if @conn
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

  def to_obj_id(owner_name, object_name)
    @conn.select_one('select object_id from all_objects where owner = :1 and object_name = :2', owner_name, object_name)[0].to_i
  end

  def check_attributes(msg, obj, attrs)
    attrs.each do |method, expected_value|
      next if expected_value == :skip

      val = method.is_a?(Array) ? obj[method[0]] : obj.send(method)
      case expected_value
      when Hash
        check_attributes("#{msg} > #{method}", val, expected_value)
      when Proc
        assert(expected_value.call(val), "#{msg} > #{method}")
      when Regexp
        assert_match(expected_value, val, "#{msg} > #{method}")
      else
        assert_equal(expected_value, val, "#{msg} > #{method}")
      end
    end
  end

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
    attrs = {
      :class => OCI8::Metadata::Table,
      :obj_id => to_obj_id(@conn.username, 'TEST_TABLE'),
      :obj_name => 'TEST_TABLE',
      :obj_schema => @conn.username,
      :num_cols => 2,
      :type_metadata => nil,
      :is_temporary? => false,
      :is_typed? => false,
      :duration => nil,
      #:dba => 0,
      #:tablespace => 0,
      :clustered? => false,
      :partitioned? => false,
      :index_only? => false,
      :columns => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL1',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL2',
        },
      },
      :inspect => /#<OCI8::Metadata::Table:\(\d+\) #{@conn.username}.TEST_TABLE>/,
    }
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end
    drop_table('test_table')

    # Transaction-specific temporary table
    @conn.exec(<<-EOS)
CREATE GLOBAL TEMPORARY TABLE test_table (col1 number(38,0), col2 varchar2(60))
EOS
    attrs = {
      :class => OCI8::Metadata::Table,
      :obj_id => to_obj_id(@conn.username, 'TEST_TABLE'),
      :obj_name => 'TEST_TABLE',
      :obj_schema => @conn.username,
      :num_cols => 2,
      :type_metadata => nil,
      :is_temporary? => true,
      :is_typed? => false,
      :duration => :transaction,
      #:dba => 0,
      #:tablespace => 0,
      :clustered? => false,
      :partitioned? => false,
      :index_only? => false,
      :columns => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL1',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL2',
        },
      },
      :inspect => /#<OCI8::Metadata::Table:\(\d+\) #{@conn.username}.TEST_TABLE>/,
    }
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end
    drop_table('test_table')

    # Session-specific temporary table
    @conn.exec(<<-EOS)
CREATE GLOBAL TEMPORARY TABLE test_table (col1 number(38,0), col2 varchar2(60))
ON COMMIT PRESERVE ROWS
EOS
    attrs = {
      :class => OCI8::Metadata::Table,
      :obj_id => to_obj_id(@conn.username, 'TEST_TABLE'),
      :obj_name => 'TEST_TABLE',
      :obj_schema => @conn.username,
      :num_cols => 2,
      :type_metadata => nil,
      :is_temporary? => true,
      :is_typed? => false,
      :duration => :session,
      #:dba => 0,
      #:tablespace => 0,
      :clustered? => false,
      :partitioned? => false,
      :index_only? => false,
      :columns => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL1',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL2',
        },
      },
      :inspect => /#<OCI8::Metadata::Table:\(\d+\) #{@conn.username}.TEST_TABLE>/,
    }
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end
    drop_table('test_table')

    # Object table
    @conn.exec(<<-EOS)
CREATE OR REPLACE TYPE test_type AS OBJECT (col1 number(38,0), col2 varchar2(60))
EOS
    @conn.exec(<<-EOS)
CREATE TABLE test_table OF test_type
EOS
    attrs = {
      :class => OCI8::Metadata::Table,
      :obj_id => to_obj_id(@conn.username, 'TEST_TABLE'),
      :obj_name => 'TEST_TABLE',
      :obj_schema => @conn.username,
      :num_cols => 2,
      :type_metadata => {
        :class => OCI8::Metadata::Type,
        :obj_id => 0,
        :obj_name => nil,
        :obj_schema => nil,
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
        :name => 'TEST_TYPE',
        :schema_name => @conn.username,
        :is_final_type? => true,
        :is_instantiable_type? => true,
        :is_subtype? => false,
        :package_name => nil,
        :type_attrs => {
          :class => Array,
          :size => 2,
          [0] => {
            :class => OCI8::Metadata::TypeAttr,
            :obj_id => nil,
            :obj_name => nil,
            :obj_schema => nil,
            :data_size => 22,
            :typecode => :number,
            :data_type => :number,
            :name => 'COL1',
            :precision => 38,
            :scale => 0,
            :type_name => 'NUMBER',
            :schema_name => 'SYS',
            :fsprecision => 0,
            :lfprecision => 38,
            :type_metadata => {
              :class => OCI8::Metadata::Type,
            },
            :inspect => '#<OCI8::Metadata::TypeAttr: COL1 NUMBER(38)>',
          },
          [1] => {
            :class => OCI8::Metadata::TypeAttr,
            :obj_id => nil,
            :obj_name => nil,
            :obj_schema => nil,
            :data_size => 60,
            :typecode => :varchar2,
            :data_type => :varchar2,
            :name => 'COL2',
            :precision => 0,
            :scale => 0,
            :type_name => 'VARCHAR2',
            :schema_name => 'SYS',
            :fsprecision => 0,
            :lfprecision => 60,
            :type_metadata => {
              :class => OCI8::Metadata::Type,
            },
            :inspect => '#<OCI8::Metadata::TypeAttr: COL2 VARCHAR2(60)>',
          },
        },
        :type_methods => [],
        :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TEST_TYPE>",
      },
      :is_temporary? => false,
      :is_typed? => true,
      :duration => nil,
      #:dba => 0,
      #:tablespace => 0,
      :clustered? => false,
      :partitioned? => false,
      :index_only? => false,
      :columns => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL1',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL2',
        },
      },
      :inspect => /#<OCI8::Metadata::Table:\(\d+\) #{@conn.username}.TEST_TABLE>/,
    }
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end
    drop_table('test_table')
    @conn.exec('DROP TYPE TEST_TYPE')

    # Index-organized table
    @conn.exec(<<-EOS)
CREATE TABLE test_table (col1 number(38,0) PRIMARY KEY, col2 varchar2(60))
ORGANIZATION INDEX 
EOS
    attrs = {
      :class => OCI8::Metadata::Table,
      :obj_id => to_obj_id(@conn.username, 'TEST_TABLE'),
      :obj_name => 'TEST_TABLE',
      :obj_schema => @conn.username,
      :num_cols => 2,
      :type_metadata => nil,
      :is_temporary? => false,
      :is_typed? => false,
      :duration => nil,
      #:dba => 0,
      #:tablespace => 0,
      :clustered? => false,
      :partitioned? => false,
      :index_only? => true,
      :columns => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL1',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'COL2',
        },
      },
      :inspect => /#<OCI8::Metadata::Table:\(\d+\) #{@conn.username}.TEST_TABLE>/,
    }
    [
     @conn.describe_any('test_table'),
     @conn.describe_table('test_table'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_TABLE'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end
    drop_table('test_table')
  end # test_table_metadata

  def test_view_metadata
    @conn.exec('CREATE OR REPLACE VIEW test_view as SELECT * FROM tab')
    attrs = {
      :class => OCI8::Metadata::View,
      :obj_id => to_obj_id(@conn.username, 'TEST_VIEW'),
      :obj_name => 'TEST_VIEW',
      :obj_schema => @conn.username,
      :num_cols => 3,
      :columns => {
        :class => Array,
        :size => 3,
        [0] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'TNAME',
        },
        [1] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'TABTYPE',
        },
        [2] => {
          :class => OCI8::Metadata::Column,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => 'CLUSTERID',
        },
      },
      :inspect => /#<OCI8::Metadata::View:\(\d+\) #{@conn.username}.TEST_VIEW>/,
    }
    [
     @conn.describe_any('test_view'),
     @conn.describe_view('test_view'),
     @conn.describe_table('test_view'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_VIEW'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
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
    attrs = {
      :class => OCI8::Metadata::Procedure,
      :obj_id => to_obj_id(@conn.username, 'TEST_PROC'),
      :obj_name => 'TEST_PROC',
      :obj_schema => @conn.username,
      :is_invoker_rights? => false,
      :name => 'TEST_PROC',
      :overload_id => nil,
      :arguments => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG1",
          :position => 1,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 38,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG2",
          :position => 2,
          #:typecode => nil,
          :data_type => :varchar2,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 0,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
        },
      },
      :is_standalone? => true,
      :inspect => '#<OCI8::Metadata::Procedure: TEST_PROC>',
    }
    [
     @conn.describe_any('test_proc'),
     @conn.describe_procedure('test_proc'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PROC'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2)
  AUTHID CURRENT_USER
IS
BEGIN
  NULL;
END;
EOS
    attrs = {
      :class => OCI8::Metadata::Procedure,
      :obj_id => to_obj_id(@conn.username, 'TEST_PROC'),
      :obj_name => 'TEST_PROC',
      :obj_schema => @conn.username,
      :is_invoker_rights? => true,
      :name => 'TEST_PROC',
      :overload_id => nil,
      :arguments => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG1",
          :position => 1,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 38,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG2",
          :position => 2,
          #:typecode => nil,
          :data_type => :varchar2,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 0,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
        },
      },
      :is_standalone? => true,
      :inspect => '#<OCI8::Metadata::Procedure: TEST_PROC>',
    }
    [
     @conn.describe_any('test_proc'),
     @conn.describe_procedure('test_proc'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PROC'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end

    @conn.exec('DROP PROCEDURE test_proc');

  end # test_procedure_metadata

  def test_function_metadata
    @conn.exec(<<-EOS)
CREATE OR REPLACE FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER IS
BEGIN
  RETURN arg1;
END;
EOS
    attrs = {
      :class => OCI8::Metadata::Function,
      :obj_id => to_obj_id(@conn.username, 'TEST_FUNC'),
      :obj_name => 'TEST_FUNC',
      :obj_schema => @conn.username,
      :is_invoker_rights? => false,
      :name => 'TEST_FUNC',
      :overload_id => nil,
      :arguments => {
        :class => Array,
        :size => 3,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "",
          :position => 0,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument:  NUMBER>',
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG1",
          :position => 1,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 38,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
        },
        [2] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG2",
          :position => 2,
          #:typecode => nil,
          :data_type => :varchar2,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 0,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
        },
      },
      :is_standalone? => true,
      :inspect => '#<OCI8::Metadata::Function: TEST_FUNC>',
    }
    [
     @conn.describe_any('test_func'),
     @conn.describe_function('test_func'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_FUNC'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER
  AUTHID CURRENT_USER
IS
BEGIN
  RETURN arg1;
END;
EOS
    attrs = {
      :class => OCI8::Metadata::Function,
      :obj_id => to_obj_id(@conn.username, 'TEST_FUNC'),
      :obj_name => 'TEST_FUNC',
      :obj_schema => @conn.username,
      :is_invoker_rights? => true,
      :name => 'TEST_FUNC',
      :overload_id => nil,
      :arguments => {
        :class => Array,
        :size => 3,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "",
          :position => 0,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument:  NUMBER>',
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG1",
          :position => 1,
          #:typecode => nil,
          :data_type => :number,
          :data_size => 22,
          :precision => 38,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 10,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
        },
        [2] => {
          :class => OCI8::Metadata::Argument,
          :obj_id => nil,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "ARG2",
          :position => 2,
          #:typecode => nil,
          :data_type => :varchar2,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 0,
          :type_name => "",
          :schema_name => "",
          :sub_name => "",
          :link => "",
          #:type_metadata => nil,
          :arguments => [],
          :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
        },
      },
      :is_standalone? => true,
      :inspect => '#<OCI8::Metadata::Function: TEST_FUNC>',
    }
    [
     @conn.describe_any('test_func'),
     @conn.describe_function('test_func'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_FUNC'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end

    @conn.exec('DROP FUNCTION test_func');

  end # test_function_metadata

  def test_package_metadata
    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG IS
  FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER;
  PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2);
END;
EOS
    attrs = {
      :class => OCI8::Metadata::Package,
      :obj_id => to_obj_id(@conn.username, 'TEST_PKG'),
      :obj_name => 'TEST_PKG',
      :obj_schema => @conn.username,
      :is_invoker_rights? => false,
      :subprograms => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Function,
          :obj_id => nil,
          :obj_name => 'TEST_FUNC',
          :obj_schema => nil,
          :is_invoker_rights? => false,
          :name => 'TEST_FUNC',
          :overload_id => 0,
          :arguments => {
            :class => Array,
            :size => 3,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 0,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument:  NUMBER>',
            },
            [1] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
            [2] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG2",
              :position => 2,
              #:typecode => nil,
              :data_type => :varchar2,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 0,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Function: TEST_FUNC>',
        },
        [1] => {
          :class => OCI8::Metadata::Procedure,
          :obj_id => nil,
          :obj_name => 'TEST_PROC',
          :obj_schema => nil,
          :is_invoker_rights? => false,
          :name => 'TEST_PROC',
          :overload_id => 0,
          :arguments => {
            :class => Array,
            :size => 2,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
            [1] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG2",
              :position => 2,
              #:typecode => nil,
              :data_type => :varchar2,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 0,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Procedure: TEST_PROC>',
        },
      },
      :types => ($oracle_version >= OCI8::ORAVER_12_1) ? [] : :skip,
      :inspect => /#<OCI8::Metadata::Package:\(\d+\) #{@conn.username}.TEST_PKG>/,
    }
    [
     @conn.describe_any('test_pkg'),
     @conn.describe_package('test_pkg'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PKG'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
    end

    @conn.exec(<<-EOS)
CREATE OR REPLACE PACKAGE TEST_PKG AUTHID CURRENT_USER IS
  PROCEDURE test_proc(arg1 IN INTEGER, arg2 OUT varchar2);
  PROCEDURE test_proc(arg1 IN INTEGER);
  FUNCTION test_func(arg1 IN INTEGER, arg2 OUT varchar2) RETURN NUMBER;
  FUNCTION test_func(arg1 IN INTEGER) RETURN NUMBER;
END;
EOS
    attrs = {
      :class => OCI8::Metadata::Package,
      :obj_id => to_obj_id(@conn.username, 'TEST_PKG'),
      :obj_name => 'TEST_PKG',
      :obj_schema => @conn.username,
      :is_invoker_rights? => true,
      :subprograms => {
        :class => Array,
        :size => 4,
        [0] => {
          :class => OCI8::Metadata::Function,
          :obj_id => nil,
          :obj_name => 'TEST_FUNC',
          :obj_schema => nil,
          :is_invoker_rights? => true,
          :name => 'TEST_FUNC',
          :overload_id => 2,
          :arguments => {
            :class => Array,
            :size => 3,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 0,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument:  NUMBER>',
            },
            [1] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
            [2] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG2",
              :position => 2,
              #:typecode => nil,
              :data_type => :varchar2,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 0,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Function: TEST_FUNC>',
        },
        [1] => {
          :class => OCI8::Metadata::Function,
          :obj_id => nil,
          :obj_name => 'TEST_FUNC',
          :obj_schema => nil,
          :is_invoker_rights? => true,
          :name => 'TEST_FUNC',
          :overload_id => 1,
          :arguments => {
            :class => Array,
            :size => 2,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 0,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument:  NUMBER>',
            },
            [1] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Function: TEST_FUNC>',
        },
        [2] => {
          :class => OCI8::Metadata::Procedure,
          :obj_id => nil,
          :obj_name => 'TEST_PROC',
          :obj_schema => nil,
          :is_invoker_rights? => true,
          :name => 'TEST_PROC',
          :overload_id => 2,
          :arguments => {
            :class => Array,
            :size => 2,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
            [1] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG2",
              :position => 2,
              #:typecode => nil,
              :data_type => :varchar2,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 0,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG2 VARCHAR2(0)>', # TODO: change to "VARCHAR2"
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Procedure: TEST_PROC>',
        },
        [3] => {
          :class => OCI8::Metadata::Procedure,
          :obj_id => nil,
          :obj_name => 'TEST_PROC',
          :obj_schema => nil,
          :is_invoker_rights? => true,
          :name => 'TEST_PROC',
          :overload_id => 1,
          :arguments => {
            :class => Array,
            :size => 1,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_id => nil,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "ARG1",
              :position => 1,
              #:typecode => nil,
              :data_type => :number,
              :data_size => 22,
              :precision => 38,
              :scale => 0,
              :level => 0,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 10,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument: ARG1 NUMBER(38)>',
            },
          },
          :is_standalone? => false,
          :inspect => '#<OCI8::Metadata::Procedure: TEST_PROC>',
        },
      },
      :types => ($oracle_version >= OCI8::ORAVER_12_1) ? [] : :skip,
      :inspect => /#<OCI8::Metadata::Package:\(\d+\) #{@conn.username}.TEST_PKG>/,
    }
    [
     @conn.describe_any('test_pkg'),
     @conn.describe_package('test_pkg'),
     @conn.describe_schema(@conn.username).objects.detect do |obj|
       obj.obj_name == 'TEST_PKG'
     end
    ].each do |desc|
      check_attributes("line: #{__LINE__}", desc, attrs)
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
    attrs_map = {}

    @conn.exec(<<-EOS)
CREATE TYPE test_type_parent AS OBJECT (
  col1 number(38,0),
  col2 varchar2(60)
)
NOT INSTANTIABLE
NOT FINAL
EOS
    attrs_map['TEST_TYPE_PARENT'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_PARENT'),
      :obj_name => 'TEST_TYPE_PARENT',
      :obj_schema => @conn.username,
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
      :name => 'TEST_TYPE_PARENT',
      :schema_name => @conn.username,
      :is_final_type? => false,
      :is_instantiable_type? => false,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::TypeAttr,
          :inspect => '#<OCI8::Metadata::TypeAttr: COL1 NUMBER(38)>',
        },
        [1] => {
          :class => OCI8::Metadata::TypeAttr,
          :inspect => '#<OCI8::Metadata::TypeAttr: COL2 VARCHAR2(60)>',
        },
      },
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_PARENT>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_child UNDER test_type_parent (
  lob BLOB
)
NOT FINAL
EOS
    attrs_map['TEST_TYPE_CHILD'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_CHILD'),
      :obj_name => 'TEST_TYPE_CHILD',
      :obj_schema => @conn.username,
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
      :name => 'TEST_TYPE_CHILD',
      :schema_name => @conn.username,
      :is_final_type? => false,
      :is_instantiable_type? => true,
      :is_subtype? => true,
      :supertype_schema_name => @conn.username,
      :supertype_name => 'TEST_TYPE_PARENT',
      :package_name => nil,
      :type_attrs => {
        :class => Array,
        :size => 3,
        [0] => attrs_map['TEST_TYPE_PARENT'][:type_attrs][[0]],
        [1] => attrs_map['TEST_TYPE_PARENT'][:type_attrs][[1]],
        [2] => {
          :class => OCI8::Metadata::TypeAttr,
          :inspect => '#<OCI8::Metadata::TypeAttr: LOB BLOB>',
        },
      },
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_CHILD>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_nestead_table AS TABLE OF test_type_child
EOS
    attrs_map['TEST_TYPE_NESTEAD_TABLE'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_NESTEAD_TABLE'),
      :obj_name => 'TEST_TYPE_NESTEAD_TABLE',
      :obj_schema => @conn.username,
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
      :collection_element => {
        :class =>  OCI8::Metadata::Collection,
        :obj_id => nil,
        :obj_name => nil,
        :obj_schema => nil,
        :data_size => 0,
        :typecode => :named_type,
        :data_type => :named_type,
        :num_elems => 0,
        :precision => 0,
        :scale => 0,
        :type_name => 'TEST_TYPE_CHILD',
        :schema_name => @conn.username,
        :type_metadata => {
          :class => OCI8::Metadata::Type,
          :obj_id => 0,
          :obj_name => nil,
          :obj_schema => nil,
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
          :name => 'TEST_TYPE_CHILD',
          :schema_name => @conn.username,
          :is_final_type? => false,
          :is_instantiable_type? => true,
          :is_subtype? => true,
          :supertype_schema_name => @conn.username,
          :supertype_name => 'TEST_TYPE_PARENT',
          :package_name => nil,
          :type_attrs => {
            :class => Array,
            :size => 3,
            [0] => {
              :class => OCI8::Metadata::TypeAttr,
              :inspect => '#<OCI8::Metadata::TypeAttr: COL1 NUMBER(38)>',
            },
            [1] => {
              :class => OCI8::Metadata::TypeAttr,
              :inspect => '#<OCI8::Metadata::TypeAttr: COL2 VARCHAR2(60)>',
            },
            [2] => {
              :class => OCI8::Metadata::TypeAttr,
              :inspect => '#<OCI8::Metadata::TypeAttr: LOB BLOB>',
            },
          },
          :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TEST_TYPE_CHILD>",
        },
        :inspect => "#<OCI8::Metadata::Collection: #{@conn.username}.TEST_TYPE_CHILD>",
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TEST_TYPE_NESTEAD_TABLE',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_NESTEAD_TABLE>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_varray AS VARRAY(10) OF test_type_child
EOS
    attrs_map['TEST_TYPE_VARRAY'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_VARRAY'),
      :obj_name => 'TEST_TYPE_VARRAY',
      :obj_schema => @conn.username,
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
      :collection_element => {
        :class =>  OCI8::Metadata::Collection,
        :obj_id => nil,
        :obj_name => nil,
        :obj_schema => nil,
        :data_size => 0,
        :typecode => :named_type,
        :data_type => :named_type,
        :num_elems => 10,
        :precision => 0,
        :scale => 0,
        :type_name => 'TEST_TYPE_CHILD',
        :schema_name => @conn.username,
        :type_metadata => attrs_map['TEST_TYPE_NESTEAD_TABLE'][:collection_element][:type_metadata],
        :inspect => "#<OCI8::Metadata::Collection: #{@conn.username}.TEST_TYPE_CHILD>",
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TEST_TYPE_VARRAY',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_VARRAY>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_grandchild UNDER test_type_child (
  table_column test_type_nestead_table,
  file_column BFILE
)
EOS
    attrs_map['TEST_TYPE_GRANDCHILD'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_GRANDCHILD'),
      :obj_name => 'TEST_TYPE_GRANDCHILD',
      :obj_schema => @conn.username,
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
      :name => 'TEST_TYPE_GRANDCHILD',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => true,
      :supertype_schema_name => @conn.username,
      :supertype_name => 'TEST_TYPE_CHILD',
      :package_name => nil,
      :type_attrs => {
        :class => Array,
        :size => 5,
        [0] => attrs_map['TEST_TYPE_CHILD'][:type_attrs][[0]],
        [1] => attrs_map['TEST_TYPE_CHILD'][:type_attrs][[1]],
        [2] => attrs_map['TEST_TYPE_CHILD'][:type_attrs][[2]],
        [3] => {
          :class => OCI8::Metadata::TypeAttr,
          :inspect => "#<OCI8::Metadata::TypeAttr: TABLE_COLUMN #{@conn.username}.TEST_TYPE_NESTEAD_TABLE>",
        },
        [4] => {
          :class => OCI8::Metadata::TypeAttr,
          :inspect => '#<OCI8::Metadata::TypeAttr: FILE_COLUMN BFILE>',
        },
      },
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_GRANDCHILD>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_incomplete
EOS
    attrs_map['TEST_TYPE_INCOMPLETE'] = {
      :class => OCI8::Metadata::Type,
      :obj_id => to_obj_id(@conn.username, 'TEST_TYPE_INCOMPLETE'),
      :obj_name => 'TEST_TYPE_INCOMPLETE',
      :obj_schema => @conn.username,
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
      :name => 'TEST_TYPE_INCOMPLETE',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      :type_methods => [],
      :inspect => /#<OCI8::Metadata::Type:\(\d+\) #{@conn.username}.TEST_TYPE_INCOMPLETE>/,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_clob AS OBJECT (lob CLOB)
EOS
    attrs_map['TEST_TYPE_HAS_CLOB'] = {
      :class => OCI8::Metadata::Type,
      :has_lob? => true,
      :has_file? => false,
    }
    if $oracle_version >= OCI8::ORAVER_9_2
      @conn.exec(<<-EOS)
CREATE TYPE test_type_has_nclob AS OBJECT (lob NCLOB)
EOS
      attrs_map['TEST_TYPE_HAS_NCLOB'] = {
        :class => OCI8::Metadata::Type,
        :has_lob? => true,
        :has_file? => false,
      }
    end
    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_blob AS OBJECT (lob BLOB)
EOS
    attrs_map['TEST_TYPE_HAS_BLOB'] = {
      :class => OCI8::Metadata::Type,
      :has_lob? => true,
      :has_file? => false,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_has_bfile AS OBJECT (lob BFILE)
EOS
    attrs_map['TEST_TYPE_HAS_BFILE'] = {
      :class => OCI8::Metadata::Type,
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
    attrs_map['TEST_TYPE_MAP_METHOD'] = {
      :class => OCI8::Metadata::Type,
      :map_method => {
        :class => OCI8::Metadata::TypeMethod,
        :name => 'AREA',
        :encapsulation => :public,
        :has_result? => true,
        :is_constructor? => false,
        :is_destructor? => false,
        :is_operator? => false,
        :is_selfish? => true,
        :is_map? => true,
        :is_order? => false,
        :is_rnds? => true,
        :is_rnps? => true,
        :is_wnds? => true,
        :is_wnps? => true,
        :is_final_method? => false,
        :is_instantiable_method? => true,
        :is_overriding_method? => false,
        :arguments => {
          :class => Array,
          :size => 1,
          [0] => {
            :class => OCI8::Metadata::TypeArgument,
            :obj_id => nil,
            :obj_name => nil,
            :obj_schema => nil,
            :name => "SELF",
            :position => 1,
            #:typecode => nil,
            :data_type => :named_type,
            #:data_size => 0, # ORA-24328: illegal attribute value
            #:precision => 0, # ORA-24328: illegal attribute value
            #:scale => 0, # ORA-24328: illegal attribute value
            :level => 0,
            :has_default => 0,
            :has_default? => false,
            :iomode => :in,
            #:radix => 0, # ORA-24328: illegal attribute value
            :type_name => "TEST_TYPE_MAP_METHOD",
            :schema_name => @conn.username,
            #:sub_name => nil, # ORA-24328: illegal attribute value
            #:link => nil, # ORA-24328: illegal attribute value
            :type_metadata => {
              :class => OCI8::Metadata::Type,
              :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TEST_TYPE_MAP_METHOD>",
            },
            :arguments => [],
            :inspect => "#<OCI8::Metadata::TypeArgument: SELF #{@conn.username}.TEST_TYPE_MAP_METHOD>",
          },
        },
        :inspect => '#<OCI8::Metadata::TypeMethod: AREA>',
      },
      :order_method => nil,
    }
    @conn.exec(<<-EOS)
CREATE TYPE test_type_order_method AS OBJECT (
  x integer,
  y integer,
  ORDER MEMBER FUNCTION match(l test_type_order_method) RETURN INTEGER
)
EOS
    attrs_map['TEST_TYPE_ORDER_METHOD'] = {
      :class => OCI8::Metadata::Type,
      :map_method => nil,
      :order_method => {
        :class => OCI8::Metadata::TypeMethod,
        :name => 'MATCH',
        :encapsulation => :public,
        :has_result? => true,
        :is_constructor? => false,
        :is_destructor? => false,
        :is_operator? => false,
        :is_selfish? => true,
        :is_map? => false,
        :is_order? => true,
        :is_rnds? => true,
        :is_rnps? => true,
        :is_wnds? => true,
        :is_wnps? => true,
        :is_final_method? => false,
        :is_instantiable_method? => true,
        :is_overriding_method? => false,
        :arguments => {
          :class => Array,
          :size => 2,
          [0] => {
            :class => OCI8::Metadata::TypeArgument,
            :obj_id => nil,
            :obj_name => nil,
            :obj_schema => nil,
            :name => "SELF",
            :position => 1,
            :typecode => :named_type,
            :data_type => :named_type,
            #:data_size => 0, # ORA-24328: illegal attribute value
            #:precision => 0, # ORA-24328: illegal attribute value
            #:scale => 0, # ORA-24328: illegal attribute value
            :level => 0,
            :has_default => 0,
            :has_default? => false,
            :iomode => :in,
            #:radix => 0, # ORA-24328: illegal attribute value
            :type_name => "TEST_TYPE_ORDER_METHOD",
            :schema_name => @conn.username,
            #:sub_name => nil, # ORA-24328: illegal attribute value
            #:link => nil, # ORA-24328: illegal attribute value
            :type_metadata => {
              :class => OCI8::Metadata::Type,
              :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TEST_TYPE_ORDER_METHOD>",
            },
            :arguments => [],
            :inspect => "#<OCI8::Metadata::TypeArgument: SELF #{@conn.username}.TEST_TYPE_ORDER_METHOD>",
          },
          [1] => {
            :class => OCI8::Metadata::TypeArgument,
            :obj_id => nil,
            :obj_name => nil,
            :obj_schema => nil,
            :name => "L",
            :position => 2,
            :typecode => :named_type,
            :data_type => :named_type,
            #:data_size => 0, # ORA-24328: illegal attribute value
            #:precision => 0, # ORA-24328: illegal attribute value
            #:scale => 0, # ORA-24328: illegal attribute value
            :level => 0,
            :has_default => 0,
            :has_default? => false,
            :iomode => :in,
            #:radix => 0, # ORA-24328: illegal attribute value
            :type_name => "TEST_TYPE_ORDER_METHOD",
            :schema_name => @conn.username,
            #:sub_name => nil, # ORA-24328: illegal attribute value
            #:link => nil, # ORA-24328: illegal attribute value
            :type_metadata => {
              :class => OCI8::Metadata::Type,
              :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TEST_TYPE_ORDER_METHOD>",
            },
            :arguments => [],
            :inspect => "#<OCI8::Metadata::TypeArgument: L #{@conn.username}.TEST_TYPE_ORDER_METHOD>",
          },
        },
        :inspect => '#<OCI8::Metadata::TypeMethod: MATCH>',
      },
    }

    attrs_map.each do |name, attrs|
      [
       @conn.describe_any(name),
       @conn.describe_type(name),
       @conn.describe_schema(@conn.username).objects.detect do |obj|
         obj.obj_name == name
       end,
      ].each do |desc|
        check_attributes(name, desc, attrs)
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

    # Get data_size of NCHAR(1) and that of CHAR(1 CHAR).
    # They depend on the database character set and the
    # client character set.
    cursor = @conn.exec("select N'1' from dual")
    # cfrm: data_size of NCHAR(1).
    cfrm = cursor.column_metadata[0].data_size
    # csem: data_size of CHAR(1 CHAR).
    cursor = @conn.exec("select CAST('1' AS CHAR(1 char)) from dual")
    csem = cursor.column_metadata[0].data_size

    column_attrs_list = []

    column_attrs_list << {
      :name => 'CHAR_10_NOT_NULL_COL',
      :data_type_string => 'CHAR(10) NOT NULL',
      :data_type => :char,
      :charset_form => :implicit,
      :nullable? => false,
      :char_used? => false,
      :char_size => 10,
      :data_size => 10,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: CHAR_10_NOT_NULL_COL CHAR(10) NOT NULL>',
    }
    column_attrs_list << {
      :name => 'CHAR_10_CHAR_COL',
      :data_type_string => 'CHAR(10 CHAR)',
      :data_type => :char,
      :charset_form => :implicit,
      :nullable? => true,
      :char_used? => true,
      :char_size => 10,
      :data_size => 10 * csem,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: CHAR_10_CHAR_COL CHAR(10 CHAR)>',
    }
    column_attrs_list << {
      :name => 'NCHAR_10_COL',
      :data_type_string => 'NCHAR(10)',
      :data_type => :char,
      :charset_form => :nchar,
      :nullable? => true,
      :char_used? => true,
      :char_size => 10,
      :data_size => 10 * cfrm,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: NCHAR_10_COL NCHAR(10)>',
    }
    column_attrs_list << {
      :name => 'VARCHAR2_10_COL',
      :data_type_string => 'VARCHAR2(10)',
      :data_type => :varchar2,
      :charset_form => :implicit,
      :nullable? => true,
      :char_used? => false,
      :char_size => 10,
      :data_size => 10,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: VARCHAR2_10_COL VARCHAR2(10)>',
    }
    column_attrs_list << {
      :name => 'VARCHAR2_10_CHAR_COL',
      :data_type_string => 'VARCHAR2(10 CHAR)',
      :data_type => :varchar2,
      :charset_form => :implicit,
      :nullable? => true,
      :char_used? => true,
      :char_size => 10,
      :data_size => 10 * csem,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: VARCHAR2_10_CHAR_COL VARCHAR2(10 CHAR)>',
    }
    column_attrs_list << {
      :name => 'NVARCHAR2_10_COL',
      :data_type_string => 'NVARCHAR2(10)',
      :data_type => :varchar2,
      :charset_form => :nchar,
      :nullable? => true,
      :char_used? => true,
      :char_size => 10,
      :data_size => 10 * cfrm,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: NVARCHAR2_10_COL NVARCHAR2(10)>',
    }
    column_attrs_list << {
      :name => 'RAW_10_COL',
      :data_type_string => 'RAW(10)',
      :data_type => :raw,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 10,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: RAW_10_COL RAW(10)>',
    }

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
    column_attrs_list << {
      :name => 'CLOB_COL',
      :data_type_string => 'CLOB',
      :data_type => :clob,
      :charset_form => :implicit,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => :skip,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: CLOB_COL CLOB>',
    }
    column_attrs_list << {
      :name => 'NCLOB_COL',
      :data_type_string => 'NCLOB',
      :data_type => :clob,
      :charset_form => :nchar,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => :skip,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: NCLOB_COL NCLOB>',
    }
    column_attrs_list << {
      :name => 'BLOB_COL',
      :data_type_string => 'BLOB',
      :data_type => :blob,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => :skip,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: BLOB_COL BLOB>',
    }
    column_attrs_list << {
      :name => 'BFILE_COL',
      :data_type_string => 'BFILE',
      :data_type => :bfile,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 530,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: BFILE_COL BFILE>',
    }

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
    column_attrs_list << {
      :name => 'NUMBER_COL',
      :data_type_string => 'NUMBER',
      :data_type => :number,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 22,
      :precision => 0,
      :scale => -127,
      :fsprecision => :skip,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: NUMBER_COL NUMBER>',
    }
    column_attrs_list << {
      :name => 'NUMBER_10_COL',
      :data_type_string => 'NUMBER(10)',
      :data_type => :number,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 22,
      :precision => 10,
      :scale => 0,
      :fsprecision => :skip,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: NUMBER_10_COL NUMBER(10)>',
    }
    column_attrs_list << {
      :name => 'NUMBER_10_2_COL',
      :data_type_string => 'NUMBER(10,2)',
      :data_type => :number,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 22,
      :precision => 10,
      :scale => 2,
      :fsprecision => :skip,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: NUMBER_10_2_COL NUMBER(10,2)>',
    }
    column_attrs_list << {
      :name => 'FLOAT_COL',
      :data_type_string => 'FLOAT',
      :data_type => :number,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 22,
      :precision => 126,
      :scale => -127,
      :fsprecision => :skip,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: FLOAT_COL FLOAT>',
    }
    column_attrs_list << {
      :name => 'FLOAT_10_COL',
      :data_type_string => 'FLOAT(10)',
      :data_type => :number,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 22,
      :precision => 10,
      :scale => -127,
      :fsprecision => :skip,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: FLOAT_10_COL FLOAT(10)>',
    }
    if $oracle_version >= OCI8::ORAVER_10_1
      column_attrs_list << {
        :name => 'BINARY_FLOAT_COL',
        :data_type_string => 'BINARY_FLOAT',
        :data_type => :binary_float,
        :charset_form => nil,
        :nullable? => true,
        :char_used? => false,
        :char_size => 0,
        :data_size => 4,
        :precision => 0,
        :scale => 0,
        :fsprecision => 0,
        :lfprecision => 0,
        :inspect => '#<OCI8::Metadata::Column: BINARY_FLOAT_COL BINARY_FLOAT>',
      }
      column_attrs_list << {
        :name => 'BINARY_DOUBLE_COL',
        :data_type_string => 'BINARY_DOUBLE',
        :data_type => :binary_double,
        :charset_form => nil,
        :nullable? => true,
        :char_used? => false,
        :char_size => 0,
        :data_size => 8,
        :precision => 0,
        :scale => 0,
        :fsprecision => 0,
        :lfprecision => 0,
        :inspect => '#<OCI8::Metadata::Column: BINARY_DOUBLE_COL BINARY_DOUBLE>',
      }
    end
    column_attrs_list << {
      :name => 'DATE_COL',
      :data_type_string => 'DATE',
      :data_type => :date,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 7,
      :precision => 0,
      :scale => 0,
      :fsprecision => 0,
      :lfprecision => 0,
      :inspect => '#<OCI8::Metadata::Column: DATE_COL DATE>',
    }

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
    column_attrs_list << {
      :name => 'TIMESTAMP_COL',
      :data_type_string => 'TIMESTAMP',
      :data_type => :timestamp,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => 6,
      :fsprecision => 6,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_COL TIMESTAMP>',
    }
    column_attrs_list << {
      :name => 'TIMESTAMP_9_COL',
      :data_type_string => 'TIMESTAMP(9)',
      :data_type => :timestamp,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => 9,
      :fsprecision => 9,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_9_COL TIMESTAMP(9)>',
    }
    column_attrs_list << {
      :name => 'TIMESTAMP_TZ_COL',
      :data_type_string => 'TIMESTAMP WITH TIME ZONE',
      :data_type => :timestamp_tz,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 13,
      :precision => :skip,
      :scale => 6,
      :fsprecision => 6,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_TZ_COL TIMESTAMP WITH TIME ZONE>',
    }
    column_attrs_list << {
      :name => 'TIMESTAMP_9_TZ_COL',
      :data_type_string => 'TIMESTAMP(9) WITH TIME ZONE',
      :data_type => :timestamp_tz,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 13,
      :precision => :skip,
      :scale => 9,
      :fsprecision => 9,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_9_TZ_COL TIMESTAMP(9) WITH TIME ZONE>',
    }
    column_attrs_list << {
      :name => 'TIMESTAMP_LTZ_COL',
      :data_type_string => 'TIMESTAMP WITH LOCAL TIME ZONE',
      :data_type => :timestamp_ltz,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => 6,
      :fsprecision => 6,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_LTZ_COL TIMESTAMP WITH LOCAL TIME ZONE>',
    }
    column_attrs_list << {
      :name => 'TIMESTAMP_9_LTZ_COL',
      :data_type_string => 'TIMESTAMP(9) WITH LOCAL TIME ZONE',
      :data_type => :timestamp_ltz,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => 9,
      :fsprecision => 9,
      :lfprecision => :skip,
      :inspect => '#<OCI8::Metadata::Column: TIMESTAMP_9_LTZ_COL TIMESTAMP(9) WITH LOCAL TIME ZONE>',
    }

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
    column_attrs_list << {
      :name => 'INTERVAL_YM_COL',
      :data_type_string => 'INTERVAL YEAR TO MONTH',
      :data_type => :interval_ym,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 5,
      :precision => 2,
      :scale => :skip,
      :fsprecision => :skip,
      :lfprecision => 2,
      :inspect => '#<OCI8::Metadata::Column: INTERVAL_YM_COL INTERVAL YEAR TO MONTH>',
    }
    column_attrs_list << {
      :name => 'INTERVAL_YM_4_COL',
      :data_type_string => 'INTERVAL YEAR(4) TO MONTH',
      :data_type => :interval_ym,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 5,
      :precision => 4,
      :scale => :skip,
      :fsprecision => :skip,
      :lfprecision => 4,
      :inspect => '#<OCI8::Metadata::Column: INTERVAL_YM_4_COL INTERVAL YEAR(4) TO MONTH>',
    }

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
    column_attrs_list << {
      :name => 'INTERVAL_DS_COL',
      :data_type_string => 'INTERVAL DAY TO SECOND',
      :data_type => :interval_ds,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => :skip,
      :fsprecision => 6,
      :lfprecision => 2,
      :inspect => '#<OCI8::Metadata::Column: INTERVAL_DS_COL INTERVAL DAY TO SECOND>',
    }
    column_attrs_list << {
      :name => 'INTERVAL_DS_4_9_COL',
      :data_type_string => 'INTERVAL DAY(4) TO SECOND(9)',
      :data_type => :interval_ds,
      :charset_form => nil,
      :nullable? => true,
      :char_used? => false,
      :char_size => 0,
      :data_size => 11,
      :precision => :skip,
      :scale => :skip,
      :fsprecision => 9,
      :lfprecision => 4,
      :inspect => '#<OCI8::Metadata::Column: INTERVAL_DS_4_9_COL INTERVAL DAY(4) TO SECOND(9)>',
    }

    # Object Types
    if $oracle_version >= OCI8::ORAVER_10_1
      column_attrs_list << {
        :name => 'SDO_GEOMETRY_COL',
        :data_type_string => 'MDSYS.SDO_GEOMETRY',
        :data_type => :named_type,
        :charset_form => nil,
        :nullable? => true,
        :char_used? => false,
        :char_size => 0,
        :data_size => :skip, # 1 when explicitly, 2000 when implicitly.
        :precision => 0,
        :scale => 0,
        :fsprecision => 0,
        :lfprecision => 0,
        :inspect => '#<OCI8::Metadata::Column: SDO_GEOMETRY_COL MDSYS.SDO_GEOMETRY>',
      }
    end

    drop_table('test_table')
    sql = <<-EOS
CREATE TABLE test_table (#{column_attrs_list.collect do |c| c[:name] + " " + c[:data_type_string]; end.join(',')})
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
      column_attrs_list.each_with_index do |attrs, idx|
        check_attributes(attrs[:name], columns[idx], attrs)
      end
    end
    drop_table('test_table')
  end # test_column_metadata

  def assert_sequence(sequence_name, desc, msg)
    min, max, incr, cache, order = @conn.select_one(<<EOS, sequence_name)
select min_value, max_value, increment_by, cache_size, order_flag
  from user_sequences
 where sequence_name = :1
EOS
    currval = @conn.select_one("select cast(#{sequence_name}.currval as integer) from dual")[0]

    attrs = {
      :class => OCI8::Metadata::Sequence,
      :obj_id => to_obj_id(@conn.username, sequence_name),
      :obj_name => sequence_name,
      :obj_schema => @conn.username,
      :objid => to_obj_id(@conn.username, sequence_name),
      :min => min.to_i,
      :max => max.to_i,
      :incr => incr.to_i,
      :cache => cache.to_i,
      :order? => order == 'Y',
      :hw_mark => Proc.new {|v| currval <= v},
      :inspect => /#<OCI8::Metadata::Sequence:\(\d+\) #{@conn.username}.#{sequence_name}/,
    }
    check_attributes(msg, desc, attrs)
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
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_any('test_seq_oci8'), "line: #{__LINE__}")
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_sequence('test_seq_oci8'), "line: #{__LINE__}")
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_schema(@conn.username).objects.detect do |obj|
                      obj.obj_name == 'TEST_SEQ_OCI8'
                    end, "line: #{__LINE__}")

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
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_any('test_seq_oci8'), "line: #{__LINE__}")
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_sequence('test_seq_oci8'), "line: #{__LINE__}")
    @conn.select_one('select test_seq_oci8.nextval from dual')
    assert_sequence('TEST_SEQ_OCI8', @conn.describe_schema(@conn.username).objects.detect do |obj|
                      obj.obj_name == 'TEST_SEQ_OCI8'
                    end, "line: #{__LINE__}")

    @conn.exec("DROP SEQUENCE TEST_SEQ_OCI8")
  end

  def test_synonym_metadata
    begin
      @conn.exec("DROP SYNONYM test_synonym")
    rescue OCIError
      raise if $!.code != 1434 # ORA-01434: private synonym to be dropped does not exist
    end

    # private synonym
    begin
      attrs = {
        :class => OCI8::Metadata::Synonym,
        :obj_id => nil,
        :obj_name => 'TEST_SYNONYM',
        :obj_schema => @conn.username,
        :schema_name => 'FOO',
        :name => 'BAR',
        :link => 'BAZ.QUZ',
        :translated_name => 'FOO.BAR@BAZ.QUZ',
        :inspect => /#<OCI8::Metadata::Synonym:\(\d+\) #{@conn.username}.TEST_SYNONYM FOR FOO.BAR@BAZ.QUZ>/,
      }
      @conn.exec("CREATE SYNONYM test_synonym FOR foo.bar@baz.quz")
      [
       @conn.describe_any('test_synonym'),
       @conn.describe_synonym('Test_Synonym'),
       @conn.describe_any(@conn.username + '.test_synonym'),
       @conn.describe_synonym(@conn.username + '.Test_Synonym'),
      ].each do |desc|
        attrs[:obj_id] = to_obj_id(@conn.username, 'TEST_SYNONYM')
        check_attributes('private synonym', desc, attrs)
      end
      @conn.exec("DROP SYNONYM test_synonym")
    rescue OCIError
      raise "grant create synonym privilege to user #{@conn.username} to pass tests." if $!.code == 1031 # ORA-01031: insufficient privileges
      raise
    end

    # public synonym
    attrs = {
      :class => OCI8::Metadata::Synonym,
      :obj_id => to_obj_id('PUBLIC', 'SDO_GEOMETRY'),
      :obj_name => 'SDO_GEOMETRY',
      :obj_schema => 'PUBLIC',
      :schema_name => 'MDSYS',
      :name => 'SDO_GEOMETRY',
      :link => nil,
      :translated_name => 'MDSYS.SDO_GEOMETRY',
      :inspect => /#<OCI8::Metadata::Synonym:\(\d+\) PUBLIC.SDO_GEOMETRY FOR MDSYS.SDO_GEOMETRY>/,
    }
    [
     @conn.describe_any('sdo_geometry'),
     @conn.describe_synonym('sdo_geometry'),
     @conn.describe_any('public.sdo_geometry'),
     @conn.describe_synonym('PUBLIC.sdo_geometry'),
    ].each do |desc|
      check_attributes('public synonym', desc, attrs)
    end

  end

  def test_access_metadata_after_logoff
    metadata = @conn.describe_any('MDSYS.SDO_GEOMETRY')
    @conn.logoff
    @conn = nil
    metadata.inspect # This should not cause segmentation fault.
  end

end # TestMetadata
