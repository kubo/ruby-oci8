require 'oci8'
require File.dirname(__FILE__) + '/config'

if $oracle_version < OCI8::ORAVER_12_1
  raise "Package types are not supported in this Oracle version."
end

conn = OCI8.new($dbuser, $dbpass, $dbname)
test_package_version = nil
begin
  cursor = conn.parse('begin :1 := rb_test_pkg.package_version; end;')
  cursor.bind_param(1, nil, Integer)
  cursor.exec
  test_package_version = cursor[1]
  cursor.close
rescue OCIError
  raise if $!.code != 6550
end
raise <<EOS if test_package_version != 1
You need to execute SQL statements in #{File.dirname(__FILE__)}/setup_test_package.sql as follows:

  $ sqlplus USERNAME/PASSWORD
  SQL> @test/setup_test_package.sql

EOS

class TestPackageType < Minitest::Test
  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  def check_attributes(msg, obj, attrs)
    attrs.each do |method, expected_value|
      val = method.is_a?(Array) ? obj[method[0]] : obj.send(method)
      if expected_value.is_a? Hash
        check_attributes("#{msg} > #{method}", val, expected_value)
      elsif expected_value.nil?
        assert_nil(val, "#{msg} > #{method}")
      else
        assert_equal(expected_value, val, "#{msg} > #{method}")
      end
    end
  end

  def test_describe_package
    if $oracle_server_version >= OCI8::ORAVER_23
      boolean_type_name = 'BOOLEAN'
    else
      boolean_type_name = 'PL/SQL BOOLEAN'
    end

    integer_type_attrs = {
      :class => OCI8::Metadata::Type,
      #:typecode => nil,
      #:collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => true,
      :is_predefined_type? => true,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      #:collection_element => nil,
      :num_type_attrs => 0,
      :num_type_methods => 0,
      #:map_method => nil,
      #:order_method => nil,
      :is_invoker_rights? => false,
      :name => 'INTEGER',
      :schema_name => 'SYS',
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      #:type_methods => [],
      :inspect => '#<OCI8::Metadata::Type:(0) SYS.INTEGER>', # TODO: change to "INTEGER"
    }

    pls_integer_type_attrs = {
      :class => OCI8::Metadata::Type,
      #:typecode => nil,
      #:collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => true,
      :is_predefined_type? => true,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      #:collection_element => nil,
      :num_type_attrs => 0,
      :num_type_methods => 0,
      #:map_method => nil,
      #:order_method => nil,
      :is_invoker_rights? => false,
      :name => 'PL/SQL PLS INTEGER',
      :schema_name => 'SYS',
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      #:type_methods => [],
      :inspect => '#<OCI8::Metadata::Type:(0) SYS.PL/SQL PLS INTEGER>', # TODO: change to "PLS_INTEGER"
    }

    boolean_type_attrs = {
      :class => OCI8::Metadata::Type,
      #:typecode => nil,
      #:collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => true,
      :is_predefined_type? => true,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      #:collection_element => nil,
      :num_type_attrs => 0,
      :num_type_methods => 0,
      #:map_method => nil,
      #:order_method => nil,
      :is_invoker_rights? => false,
      :name => boolean_type_name,
      :schema_name => 'SYS',
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      #:type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) SYS.#{boolean_type_name}>",
    }

    varchar2_type_attrs = {
      :class => OCI8::Metadata::Type,
      #:typecode => nil,
      #:collection_typecode => nil,
      :is_incomplete_type? => false,
      :is_system_type? => true,
      :is_predefined_type? => true,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      #:collection_element => nil,
      :num_type_attrs => 0,
      :num_type_methods => 0,
      #:map_method => nil,
      #:order_method => nil,
      :is_invoker_rights? => false,
      :name => 'VARCHAR2',
      :schema_name => 'SYS',
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => nil,
      :type_attrs => [],
      #:type_methods => #[],
      :inspect => '#<OCI8::Metadata::Type:(0) SYS.VARCHAR2>', # TODO: change to "VARCHAR2"
    }

    array_of_integer_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :varray,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 22,
        :typecode => :integer,
        :data_type => :number,
        :num_elems => 50,
        :precision => 38,
        :scale => 0,
        :type_name => 'INTEGER',
        :schema_name => 'SYS',
        :type_metadata => integer_type_attrs,
        :inspect => '#<OCI8::Metadata::Collection: NUMBER(38)>', # TODO: change to "INTEGER"
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'ARRAY_OF_INTEGER',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.ARRAY_OF_INTEGER>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.ARRAY_OF_INTEGER"
    }

    table_of_pls_integer_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :table,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 0,
        :typecode => :pls_integer,
        :data_type => :pls_integer,
        :num_elems => 0,
        :precision => 0,
        :scale => 0,
        :type_name => 'PL/SQL PLS INTEGER',
        :schema_name => 'SYS',
        :type_metadata => pls_integer_type_attrs,
        :inspect => '#<OCI8::Metadata::Collection: PLS_INTEGER>',
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TABLE_OF_PLS_INTEGER',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TABLE_OF_PLS_INTEGER>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_PLS_INTEGER"
    }

    table_of_boolean_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :table,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 0,
        :typecode => :boolean,
        :data_type => :boolean,
        :num_elems => 0,
        :precision => 0,
        :scale => 0,
        :type_name => boolean_type_name,
        :schema_name => 'SYS',
        :type_metadata => boolean_type_attrs,
        :inspect => '#<OCI8::Metadata::Collection: BOOLEAN>',
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TABLE_OF_BOOLEAN',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TABLE_OF_BOOLEAN>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_BOOLEAN"
    }

    indexed_table_of_varchar2_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :itable,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => false,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 10,
        :typecode => :varchar2,
        :data_type => :varchar2,
        :num_elems => 5,
        :precision => 0,
        :scale => 0,
        :type_name => 'VARCHAR2',
        :schema_name => 'SYS',
        :type_metadata => varchar2_type_attrs,
        :inspect => '#<OCI8::Metadata::Collection: VARCHAR2(10)>',
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'INDEXED_TABLE_OF_VARCHAR2',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.INDEXED_TABLE_OF_VARCHAR2>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.INDEXED_TABLE_OF_VARCHAR2"
    }

    rec1_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :record,
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
      :name => 'REC1',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::TypeAttr,
          :data_size => 0,
          :typecode => :pls_integer,
          :data_type => :pls_integer,
          :name => 'I',
          :precision => 0,
          :scale => 0,
          :type_name => 'PL/SQL PLS INTEGER',
          :schema_name => 'SYS',
          :fsprecision => 0,
          :lfprecision => 0,
          :type_metadata => pls_integer_type_attrs,
          :inspect => '#<OCI8::Metadata::TypeAttr: I PLS_INTEGER>',
        },
        [1] => {
          :class => OCI8::Metadata::TypeAttr,
          :data_size => 22,
          :typecode => :integer,
          :data_type => :number,
          :name => 'J',
          :precision => 38,
          :scale => 0,
          :type_name => 'INTEGER',
          :schema_name => 'SYS',
          :fsprecision => 0,
          :lfprecision => 0,
          :type_metadata => integer_type_attrs,
          :inspect => '#<OCI8::Metadata::TypeAttr: J NUMBER(38)>', # TODO: change to "INTEGER"
        },
      },
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.REC1>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC1"
    }

    rec2_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :record,
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
      :num_type_attrs => 3,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'REC2',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => {
        :class => Array,
        :size => 3,
        [0] => {
          :class => OCI8::Metadata::TypeAttr,
          :data_size => 0,
          :typecode => :boolean,
          :data_type => :boolean,
          :name => 'B',
          :precision => 0,
          :scale => 0,
          :type_name => boolean_type_name,
          :schema_name => 'SYS',
          :fsprecision => 0,
          :lfprecision => 0,
          :type_metadata => boolean_type_attrs,
          :inspect => '#<OCI8::Metadata::TypeAttr: B BOOLEAN>',
        },
        [1] => {
          :class => OCI8::Metadata::TypeAttr,
          :data_size => 0,
          :typecode => :named_collection,
          :data_type => :named_type,
          :name => 'IT',
          :precision => 0,
          :scale => 0,
          :type_name => 'INDEXED_TABLE_OF_VARCHAR2',
          :schema_name => @conn.username,
          :fsprecision => 0,
          :lfprecision => 0,
          :type_metadata => indexed_table_of_varchar2_type_attrs,
          :inspect => "#<OCI8::Metadata::TypeAttr: IT #{@conn.username}.INDEXED_TABLE_OF_VARCHAR2>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.INDEXED_TABLE_OF_VARCHAR2"
        },
        [2] => {
          :class => OCI8::Metadata::TypeAttr,
          :data_size => 0,
          :typecode => :record,
          :data_type => :record,
          :name => 'REC',
          :precision => 0,
          :scale => 0,
          :type_name => 'REC1',
          :schema_name => @conn.username,
          :fsprecision => 0,
          :lfprecision => 0,
          :type_metadata => rec1_type_attrs,
          :inspect => "#<OCI8::Metadata::TypeAttr: REC #{@conn.username}.REC1>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC1"
        },
      },
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.REC2>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC2"
    }

    table_of_rec1_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :table,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 0,
        :typecode => :record,
        :data_type => :record,
        :num_elems => 0,
        :precision => 0,
        :scale => 0,
        :type_name => 'REC1',
        :schema_name => @conn.username,
        :type_metadata => rec1_type_attrs,
        :inspect => "#<OCI8::Metadata::Collection: #{@conn.username}.REC1>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC1"
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TABLE_OF_REC1',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TABLE_OF_REC1>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_REC1"
    }

    table_of_rec2_type_attrs = {
      :class => OCI8::Metadata::Type,
      :typecode => :named_collection,
      :collection_typecode => :table,
      :is_incomplete_type? => false,
      :is_system_type? => false,
      :is_predefined_type? => false,
      :is_transient_type? => false,
      :is_system_generated_type? => false,
      :has_nested_table? => true,
      :has_lob? => false,
      :has_file? => false,
      :collection_element => {
        :class => OCI8::Metadata::Collection,
        :data_size => 0,
        :typecode => :record,
        :data_type => :record,
        :num_elems => 0,
        :precision => 0,
        :scale => 0,
        :type_name => 'REC2',
        :schema_name => @conn.username,
        :type_metadata => rec2_type_attrs,
        :inspect => "#<OCI8::Metadata::Collection: #{@conn.username}.REC2>",
        #:inspect => "#<OCI8::Metadata::Collection: #{@conn.username}.RB_TEST_PKG.REC2>",
      },
      :num_type_attrs => 0,
      :num_type_methods => 0,
      :map_method => nil,
      :order_method => nil,
      :is_invoker_rights? => false,
      :name => 'TABLE_OF_REC2',
      :schema_name => @conn.username,
      :is_final_type? => true,
      :is_instantiable_type? => true,
      :is_subtype? => false,
      :supertype_schema_name => nil,
      :supertype_name => nil,
      :package_name => 'RB_TEST_PKG',
      :type_attrs => [],
      :type_methods => [],
      :inspect => "#<OCI8::Metadata::Type:(0) #{@conn.username}.TABLE_OF_REC2>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_REC2"
    }

    type_metadata_attrs = {
      'ARRAY_OF_INTEGER' => array_of_integer_type_attrs,
      'TABLE_OF_PLS_INTEGER' => table_of_pls_integer_type_attrs,
      'TABLE_OF_BOOLEAN' => table_of_boolean_type_attrs,
      'INDEXED_TABLE_OF_VARCHAR2' => indexed_table_of_varchar2_type_attrs,
      'REC1' => rec1_type_attrs,
      'REC2' => rec2_type_attrs,
      'TABLE_OF_REC1' => table_of_rec1_type_attrs,
      'TABLE_OF_REC2' => table_of_rec2_type_attrs,
    }

    sum_table_of_pls_integer_subprogram_attrs = {
      :class => OCI8::Metadata::Function,
      :obj_name => 'SUM_TABLE_OF_PLS_INTEGER',
      :obj_schema => nil,
      :is_invoker_rights? => false,
      :name => 'SUM_TABLE_OF_PLS_INTEGER',
      :overload_id => 0,
      :arguments => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "",
          :position => 0,
          #:typecode => nil,
          :data_type => 3,
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
          :inspect => '#<OCI8::Metadata::Argument:  unknown(3)>', # TODO: change to "PLS_INTEGER"
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "TBL",
          :position => 1,
          #:typecode => nil,
          :data_type => :named_type,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 0,
          :type_name => "RB_TEST_PKG",
          :schema_name => @conn.username,
          :sub_name => "TABLE_OF_PLS_INTEGER",
          :link => "",
          #:type_metadata => nil,
          :arguments => ($oracle_server_version >= OCI8::ORAVER_18) ?
          {
            :class => Array,
            :size => 0,
          } : {
            :class => Array,
            :size => 1,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 1,
              #:typecode => nil,
              :data_type => 3,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 1,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 0,
              :type_name => "",
              :schema_name => "",
              :sub_name => "",
              :link => "",
              #:type_metadata => nil,
              :arguments => [],
              :inspect => '#<OCI8::Metadata::Argument:  unknown(3)>', # TODO: change to "PLS_INTEGER"
            },
          },
          :inspect => "#<OCI8::Metadata::Argument: TBL #{@conn.username}.RB_TEST_PKG>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_PLS_INTEGER"
        },
      },
      :is_standalone? => false,
      :inspect => '#<OCI8::Metadata::Function: SUM_TABLE_OF_PLS_INTEGER>', # FIXME
    }

    add_rec1_values_subprogram_attrs = {
      :class => OCI8::Metadata::Function,
      :obj_name => 'ADD_REC1_VALUES',
      :obj_schema => nil,
      :is_invoker_rights? => false,
      :name => 'ADD_REC1_VALUES',
      :overload_id => 0,
      :arguments => {
        :class => Array,
        :size => 2,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "",
          :position => 0,
          #:typecode => nil,
          :data_type => 3,
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
          :inspect => '#<OCI8::Metadata::Argument:  unknown(3)>', # TODO: change to "PLS_INTEGER"
        },
        [1] => {
          :class => OCI8::Metadata::Argument,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "TBL",
          :position => 1,
          #:typecode => nil,
          :data_type => :named_type,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :in,
          :radix => 0,
          :type_name => "RB_TEST_PKG",
          :schema_name => @conn.username,
          :sub_name => "TABLE_OF_REC1",
          :link => "",
          #:type_metadata => nil,
          :arguments => ($oracle_server_version >= OCI8::ORAVER_18) ?
          {
            :class => Array,
            :size => 0,
          } : {
            :class => Array,
            :size => 1,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 1,
              #:typecode => nil,
              :data_type => 250,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 1,
              :has_default => 0,
              :has_default? => false,
              :iomode => :in,
              :radix => 0,
              :type_name => "RB_TEST_PKG",
              :schema_name => @conn.username,
              :sub_name => "REC1",
              :link => "",
              #:type_metadata => nil,
              :arguments => {
                :class => Array,
                :size => 2,
                [0] => {
                  :class => OCI8::Metadata::Argument,
                  :obj_name => nil,
                  :obj_schema => nil,
                  :name => "I",
                  :position => 1,
                  #:typecode => nil,
                  :data_type => 3,
                  :data_size => 0,
                  :precision => 0,
                  :scale => 0,
                  :level => 2,
                  :has_default => 0,
                  :has_default? => false,
                  :iomode => :in,
                  :radix => 0,
                  :type_name => "",
                  :schema_name => "",
                  :sub_name => "",
                  :link => "",
                  #:type_metadata => nil,
                  :arguments => [],
                  :inspect => '#<OCI8::Metadata::Argument: I unknown(3)>', # TODO: change to "PLS_INTEGER"
                },
                [1] => {
                  :class => OCI8::Metadata::Argument,
                  :obj_name => nil,
                  :obj_schema => nil,
                  :name => "J",
                  :position => 2,
                  #:typecode => nil,
                  :data_type => :number,
                  :data_size => 22,
                  :precision => 38,
                  :scale => 0,
                  :level => 2,
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
                  :inspect => '#<OCI8::Metadata::Argument: J NUMBER(38)>', # TODO: change to "INTEGER"
                },
              },
              :inspect => '#<OCI8::Metadata::Argument:  unknown(250)>', # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC1"
            },
          },
          :inspect => "#<OCI8::Metadata::Argument: TBL #{@conn.username}.RB_TEST_PKG>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_REC1"
        },
      },
      :is_standalone? => false,
      :inspect => '#<OCI8::Metadata::Function: ADD_REC1_VALUES>', # TODO: change to "#{@conn.username}.RB_TEST_PKG.ADD_REC1_VALUES"
    }

    out_rec1_values_subprogram_attrs = {
      :class => OCI8::Metadata::Procedure,
      :obj_name => 'OUT_REC1_VALUES',
      :obj_schema => nil,
      :is_invoker_rights? => false,
      :name => 'OUT_REC1_VALUES',
      :overload_id => 0,
      :arguments => {
        :class => Array,
        :size => 1,
        [0] => {
          :class => OCI8::Metadata::Argument,
          :obj_name => nil,
          :obj_schema => nil,
          :name => "TBL",
          :position => 1,
          #:typecode => nil,
          :data_type => :named_type,
          :data_size => 0,
          :precision => 0,
          :scale => 0,
          :level => 0,
          :has_default => 0,
          :has_default? => false,
          :iomode => :out,
          :radix => 0,
          :type_name => "RB_TEST_PKG",
          :schema_name => @conn.username,
          :sub_name => "TABLE_OF_REC1",
          :link => "",
          #:type_metadata => nil,
          :arguments => ($oracle_server_version >= OCI8::ORAVER_18) ?
          {
            :class => Array,
            :size => 0,
          } : {
            :class => Array,
            :size => 1,
            [0] => {
              :class => OCI8::Metadata::Argument,
              :obj_name => nil,
              :obj_schema => nil,
              :name => "",
              :position => 1,
              #:typecode => nil,
              :data_type => 250,
              :data_size => 0,
              :precision => 0,
              :scale => 0,
              :level => 1,
              :has_default => 0,
              :has_default? => false,
              :iomode => :out,
              :radix => 0,
              :type_name => "RB_TEST_PKG",
              :schema_name => @conn.username,
              :sub_name => "REC1",
              :link => "",
              #:type_metadata => nil,
              :arguments => {
                :class => Array,
                :size => 2,
                [0] => {
                  :class => OCI8::Metadata::Argument,
                  :obj_name => nil,
                  :obj_schema => nil,
                  :name => "I",
                  :position => 1,
                  #:typecode => nil,
                  :data_type => 3,
                  :data_size => 0,
                  :precision => 0,
                  :scale => 0,
                  :level => 2,
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
                  :inspect => '#<OCI8::Metadata::Argument: I unknown(3)>', # TODO: change to "PLS_INTEGER"
                },
                [1] => {
                  :class => OCI8::Metadata::Argument,
                  :obj_name => nil,
                  :obj_schema => nil,
                  :name => "J",
                  :position => 2,
                  #:typecode => nil,
                  :data_type => :number,
                  :data_size => 22,
                  :precision => 38,
                  :scale => 0,
                  :level => 2,
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
                  :inspect => '#<OCI8::Metadata::Argument: J NUMBER(38)>', # TODO: change to "INTEGER"
                },
              },
              :inspect => '#<OCI8::Metadata::Argument:  unknown(250)>', # TODO: change to "#{@conn.username}.RB_TEST_PKG.REC1"
            },
          },
          :inspect => "#<OCI8::Metadata::Argument: TBL #{@conn.username}.RB_TEST_PKG>", # TODO: change to "#{@conn.username}.RB_TEST_PKG.TABLE_OF_REC1"
        },
      },
      :is_standalone? => false,
      :inspect => '#<OCI8::Metadata::Procedure: OUT_REC1_VALUES>', # TODO: change to "#{@conn.username}.RB_TEST_PKG.OUT_REC1_VALUES"
    }

    subprogram_metadata_attrs = {
      'SUM_TABLE_OF_PLS_INTEGER' => sum_table_of_pls_integer_subprogram_attrs,
      'ADD_REC1_VALUES' => add_rec1_values_subprogram_attrs,
      'OUT_REC1_VALUES' => out_rec1_values_subprogram_attrs,
    }

    pkg_metadata = @conn.describe_package('rb_test_pkg')

    assert_kind_of(OCI8::Metadata::Package, pkg_metadata)
    assert_equal(false, pkg_metadata.is_invoker_rights?)
    type_metadata = pkg_metadata.types
    assert_kind_of(Array, type_metadata)
    assert_equal(8, type_metadata.size)
    type_metadata.each do |md|
      attrs = type_metadata_attrs[md.name]
      if attrs
        check_attributes(md.name, md, attrs)
      else
        raise "unknown type name #{md.name}"
      end
    end

    subprogram_metadata = pkg_metadata.subprograms
    assert_kind_of(Array, subprogram_metadata)
    assert_equal(3, subprogram_metadata.size)
    subprogram_metadata.each do |md|
      attrs = subprogram_metadata_attrs[md.name]
      if attrs
        check_attributes(md.name, md, attrs)
      else
        raise "unknown subprogram name #{md.name}"
      end
    end
  end
end
