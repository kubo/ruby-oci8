require 'oci8'

## See 'Describing Schema Metadata' in Oracle Call Interface Programmer's Guide.

class OCI8

  module Metadata
    class Base
      ## Table 6-1 Attributes Belonging to All Parameters
      def num_params;   __ub2(121); end # don't use.
      def obj_id;       __ub4(136); end
      def obj_name;     __text(134); end
      def obj_schema;   __text(135); end
      # def ptype;        __ub1(123); end
      def timestamp;    __oradate(119); end # don't use.

      def inspect
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}.#{obj_name}>"
      end
      private

      def __boolean(idx)
        __ub1(idx) == 0 ? false : true
      end
      alias __typecode __ub2
      alias __duration __ub2
      alias __word __sb4
      def __ref(idx); raise NotImplementedError; end
      def __anydata(idx); raise NotImplementedError; end
      def __type_encap(idx); raise NotImplementedError; end
      def __type_param_mode(idx); raise NotImplementedError; end

      def __type_string
        type = case data_type
               when  1 # SQLT_CHR
                 "VARCHAR2(#{data_size})"
               when 2 # SQLT_NUM
                 case scale
                 when -127
                   case precision
                   when 0
                     "NUMBER"
                   else
                     "FLOAT(#{precision})"
                   end
                 when 0
                   case precision
                   when 0
                     "NUMBER"
                   else
                     "NUMBER(#{precision})"
                   end
                 else
                   "NUMBER(#{precision},#{scale})"
                 end
               when  8 # SQLT_LNG
                 "LONG"
               when 12 # SQLT_DAT
                 "DATE"
               when 23 # SQLT_BIN
                 "RAW(#{data_size})"
               when 24 # SQLT_LBI
                 "LONG RAW"
               when 96 # SQLT_AFC
                 "CHAR(#{data_size})"
               when 104 # SQLT_RDD
                 "ROWID"
               when 112 # SQLT_CLOB
                 "CLOB"
               when 113 # SQLT_BLOB
                 "BLOB"
               when 187 # SQLT_TIMESTAMP
                 "TIMESTAMP"
               when 188 # SQLT_TIMESTAMP_TZ
                 "TIMESTAMP WITH TIME ZONE"
               when 189 # SQLT_INTERVAL_YM
                 "INTERVAL YEAR TO MONTH"
               when 190 # SQLT_INTERVAL_DS
                 "INTERVAL DAY TO SECOND"
               when 232 # SQLT_TIMESTAMP_LTZ
                 "TIMESTAMP WITH LOCAL TIME ZONE"
               when 108 # SQLT_NTY
                 "#{schema_name}.#{type_name}"
               else
                 "unknown(#{data_type})"
               end
        if respond_to?(:is_null?) && !is_null?
          name + ' ' + type + " NOT NULL"
        else
          name + ' ' + type
        end
      end
    end

    class Unknown < Base
      register_ptype 0 # OCI_PTYPE_UNK
    end

    class Table < Base
      register_ptype  1 # OCI_PTYPE_TABLE

      ## Table 6-2 Attributes Belonging to Tables or Views
      def objid;        __ub4(122); end
      def num_cols;     __ub2(102); end
      def list_columns; __param(103); end; private :list_columns
      def ref_tdo;      __ref(110); end # not implemented
      def is_temporary?; __boolean(130); end
      def is_typed?;    __boolean(131); end
      def duration;     __duration(132); end # don't use. return value may change in future.

      # Table 6-3 Attributes Specific to Tables
      def dba;          __ub4(104); end # don't use.
      def tablespace;   __word(126); end # don't use.
      def clustered?;   __boolean(105); end
      def partitioned?; __boolean(106); end
      def index_only?;  __boolean(107); end

      # shortcut method
      def columns
        @columns ||= list_columns.to_a
      end
    end

    class View < Base
      register_ptype  2 # OCI_PTYPE_VIEW

      ## Table 6-2 Attributes Belonging to Tables or Views
      def objid;        __ub4(122); end
      def num_cols;     __ub2(102); end
      def list_columns; __param(103); end; private :list_columns
      def ref_tdo;      __ref(110); end # not implemented
      def is_temporary?; __boolean(130); end
      def is_typed?;    __boolean(131); end
      def duration;     __duration(132); end # don't use. return value may change in future.

      # shortcut method 
      def columns
        @columns ||= list_columns.to_a
      end
    end

    class ProcBase < Base # don't use.
      # Table 6-4 Attribute Belonging to Procedures or Functions
      def list_arguments; __param(108); end # don't use.
      def is_invoker_rights?; __boolean(133); end # don't use.

      # Table 6-5 Attributes Specific to Package Subprograms
      def name;         __text(4); end # don't use.
      def overload_id;  __ub2(125); end # don't use.
    end
    class Procedure < ProcBase # don't use.
      register_ptype 3 # OCI_PTYPE_PROC
    end
    class Function < ProcBase # don't use.
      register_ptype 4 # OCI_PTYPE_FUNC
    end

    class Package < Base # don't use.
      register_ptype 5 # OCI_PTYPE_PKG

      # Table 6-6 Attributes Belonging to Packages
      def list_subprograms; __param(109); end # don't use.
      def is_invoker_rights?; __boolean(133); end # don't use.
    end

    class Type < Base
      register_ptype 6 # OCI_PTYPE_TYPE

      # Table 6-7 Attributes Belonging to Types
      def ref_tdo;      __ref(110); end # not implemented
      def typecode;     __typecode(216); end # don't use.
      def collection_typecode; __typecode(217); end # don't use.
      def is_incomplete_type?; __boolean(219); end # don't use.
      def is_system_type?; __boolean(220); end # don't use.
      def is_predefined_type?; __boolean(221); end # don't use.
      def is_transient_type?; __boolean(222); end # don't use.
      def is_system_generated_type?; __boolean(223); end # don't use.
      def has_nested_table?; __boolean(224); end # don't use.
      def has_lob?;     __boolean(225); end # don't use.
      def has_file?;    __boolean(226); end # don't use.
      def collection_element; __param(227); end # don't use.
      def num_type_attrs; __ub2(228); end
      def list_type_attrs; __param(229); end; private :list_type_attrs
      def num_type_methods; __ub2(230); end # don't use.
      def list_type_methods; __param(231); end # don't use.
      def map_method;    __param(232); end # don't use.
      def order_method;  __param(233); end # don't use.
      def is_invoker_rights?; __boolean(133); end # don't use.
      def name;         __text(4); end
      def schema_name;  __text(9); end
      def is_final_type?; __boolean(279); end # don't use.
      def is_instantiable_type?; __boolean(280); end # don't use.
      def is_subtype?;  __boolean(258); end # don't use.
      def supertype_schema_name; __text(259); end # don't use.
      def supertype_name; __text(260); end # don't use.

      def type_attrs
        @type_attrs ||= list_type_attrs.to_a
      end
    end

    class TypeAttr < Base # don't use.
      register_ptype 12 # OCI_PTYPE_TYPE_ATTR

      # Table 6-8 Attributes Belonging to Type Attributes
      def data_size;    __ub2_nc(1); end
      def typecode;     __typecode(216); end # don't use.
      def data_type;    __ub2(2); end
      def name;         __text(4); end
      def precision;    @is_implicit ? __sb2(5) : __ub1(5); end
      def scale;        __sb1(6); end
      def type_name;    __text(8); end
      def schema_name;  __text(9); end
      def ref_tdo;      __ref(110); end # not implemented
      def charset_id;   __ub2(31); end # don't use.
      def charset_form; __ub1(32); end # don't use.
      def fsprecision;  __ub1(16); end # don't use.
      def lfprecision;  __ub1(17); end # don't use.

      def inspect
        "#<#{self.class.name}: #{__type_string}>"
      end
    end

    class TypeMethod < Base # don't use.
      register_ptype 14 # OCI_PTYPE_TYPE_METHOD

      # Table 6-9 Attributes Belonging to Type Methods
      def name;         __text(4); end # don't use.
      def encapsulation; __type_encap(235); end # not implemented
      def list_arguments; __param(108); end # don't use.
      def is_constructor?; __boolean(241); end # don't use.
      def is_destructor?; __boolean(242); end # don't use.
      def is_operator?; __boolean(243); end # don't use.
      def is_selfish?;  __boolean(236); end # don't use.
      def is_map?;      __boolean(244); end # don't use.
      def is_order?;    __boolean(245); end # don't use.
      def is_rnds?;     __boolean(246); end # don't use.
      def is_rnps?;     __boolean(247); end # don't use.
      def is_wnds?;     __boolean(248); end # don't use.
      def is_wnps?;     __boolean(249); end # don't use.
      def is_final_method?; __boolean(281); end # don't use.
      def is_instantiable_method?; __boolean(282); end # don't use.
      def is_overriding_method?; __boolean(283); end # don't use.
    end

    class Collection < Base # don't use.
      register_ptype 13 # OCI_PTYPE_TYPE_COLL

      # Table 6-10 Attributes Belonging to Collection Types 
      def data_size;    __ub2_nc(1); end # don't use.
      def typecode;     __typecode(216); end # don't use.
      def data_type;    __ub2(2); end # don't use.
      def num_elems;    __ub4(234); end # don't use.
      def name;         __text(4); end # don't use.
      def precision;    @is_implicit ? __sb2(5) : __ub1(5); end # don't use.
      def scale;        __sb1(6); end # don't use.
      def type_name;    __text(8); end # don't use.
      def schema_name;  __text(9); end # don't use.
      def ref_tdo;      __ref(110); end # not implemented
      def charset_id;   __ub2(31); end # don't use.
      def charset_form; __ub1(32); end # don't use.
    end

    class Synonym < Base
      register_ptype 7 # OCI_PTYPE_SYN

      # Table 6-11 Attributes Belonging to Synonyms
      def objid;        __ub4(122); end
      def schema_name;  __text(9); end
      def name;         __text(4); end
      def link;         __text(111); end

      def object_name
        if link.size == 0
          schema_name + '.' + name
        else
          schema_name + '.' + name + '@' + link
        end
      end
      def inspect
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}.#{obj_name} to #{object_name}>"
      end
    end

    class Sequence < Base
      register_ptype 8 # OCI_PTYPE_SEQ

      # Table 6-12 Attributes Belonging to Sequences
      def objid;        __ub4(122); end
      def min;          __oraint(112); end
      def max;          __oraint(113); end
      def incr;         __oraint(114); end
      def cache;        __oraint(115); end
      def order?;       __boolean(116); end
      def hw_mark;      __oraint(117); end
    end

    class Column < Base
      register_ptype 9 # OCI_PTYPE_COL

      # Table 6-13 Attributes Belonging to Columns of Tables or Views
      def char_used;    __ub4(285); end # don't use.
      def char_size;    __ub2(286); end # don't use.
      def data_size;    __ub2_nc(1); end
      def data_type;    __ub2(2); end
      def name;         __text(4); end
      def precision;    @is_implicit ? __sb2(5) : __ub1(5); end
      def scale;        __sb1(6); end
      def is_null?;     __boolean(7); end
      def type_name;    __text(8); end
      def schema_name;  __text(9); end
      def ref_tdo;      __ref(110); end # not implemented
      def charset_id;   __ub2(31); end # don't use.
      def charset_form; __ub1(32); end # don't use.

      def inspect
        "#<#{self.class.name}: #{__type_string}>"
      end
    end

    class ArgBase < Base # don't use.
      # Table 6-14 Attributes Belonging to Arguments/Results
      def name;         __text(4); end # don't use.
      def position;     __ub2(11); end # don't use.
      def typecode;     __typecode(216); end # don't use.
      def data_type;    __ub2(2); end # don't use.
      def data_size;    __ub2_nc(1); end # don't use.
      def precision;    @is_implicit ? __sb2(5) : __ub1(5); end # don't use.
      def scale;        __sb1(6); end # don't use.
      def level;        __ub2(211); end # don't use.
      def has_default;  __ub1(212); end # don't use.
      def list_arguments; __param(108); end # don't use.
      def iomode;       __type_param_mode(213); end # not implemented
      def radix;        __ub1(214); end # don't use.
      def is_null?;     __boolean(7); end # don't use.
      def type_name;    __text(8); end # don't use.
      def schema_name;  __text(9); end # don't use.
      def sub_name;     __text(10); end # don't use.
      def link;         __text(111); end # don't use.
      def ref_tdo;      __ref(110); end # not implemented
      def charset_id;   __ub2(31); end # don't use.
      def charset_form; __ub1(32); end # don't use.
    end
    class Argument < ArgBase # don't use.
      register_ptype 10 # OCI_PTYPE_ARG
    end
    class TypeArgument < ArgBase # don't use.
      register_ptype 15 # OCI_PTYPE_TYPE_ARG
    end
    class TypeResult < ArgBase # don't use.
      register_ptype 16 # OCI_PTYPE_TYPE_RESULT
    end

    class List < Base # don't use.
      register_ptype 11 # OCI_PTYPE_LIST

      # Table 6-15 List Attributes
      def ltype;        __ub2(128); end

      # convert to array
      def to_a
        # Table 6-15 List Attributes
        case ltype
        when  1; offset = 1 # OCI_LTYPE_COLUMN;
        when  2; offset = 1 # OCI_LTYPE_ARG_PROC
        when  3; offset = 0 # OCI_LTYPE_ARG_FUNC
        when  4; offset = 0 # OCI_LTYPE_SUBPRG
        when  5; offset = 1 # OCI_LTYPE_TYPE_ATTR
        when  6; offset = 1 # OCI_LTYPE_TYPE_METHOD
        when  7; offset = 0 # OCI_LTYPE_TYPE_ARG_PROC
        when  8; offset = 1 # OCI_LTYPE_TYPE_ARG_FUNC      
        when  9; offset = 0 # OCI_LTYPE_SCH_OBJ
        when 10; offset = 0 # OCI_LTYPE_DB_SCH
        #when 11; offset = ? # OCI_LTYPE_TYPE_SUBTYPE
        #when 12; offset = ? # OCI_LTYPE_TABLE_ALIAS
        #when 13; offset = ? # OCI_LTYPE_VARIABLE_TYPE
        #when 14; offset = ? # OCI_LTYPE_NAME_VALUE
        else
          raise NotImplementedError, "unsupported list type #{list.ltype}"
        end
        ary = []
        0.upto(num_params - 1) do |i|
          ary << __param_at(i + offset);
        end
        ary
      end
    end

    class Schema < Base
      register_ptype 17 # OCI_PTYPE_SCHEMA

      # Table 6-16 Attributes Specific to Schemas
      def list_objects; __param(261); end

      # shortcut method
      def objects
        @objects ||= begin
                       list_objects
                     rescue OCIError => exc
                       if exc.code != -1
                         raise
                       end
                       # describe again.
                       @con.describe_schema(obj_schema).list_objects
                     end.to_a
      end
      def inspect
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}>"
      end
    end

    class Database < Base
      register_ptype 18 # OCI_PTYPE_DATABASE

      # Table 6-17 Attributes Specific to Databases
      def version;      __text(218); end
      def charset_id;   __ub2(31); end # don't use.
      def ncharset_id;  __ub2(262); end # don't use.
      def list_schemas; __param(263); end; private :list_schemas
      def max_proc_len; __ub4(264); end
      def max_column_len; __ub4(265); end
      def cursor_commit_behavior
        case __ub1(266)
        when 0; :cusror_open
        when 1; :cursor_closed
        end
      end
      def max_catalog_namelen; __ub1(267); end
      def catalog_location
        case __ub1(268)
        when 0; :cl_start
        when 1; :cl_end
        end
      end
      def savepoint_support
        case __ub1(269)
        when 0; :sp_supported
        when 1; :sp_unsupported
        end
      end
      def nowait_support
        case __ub1(270)
        when 0; :nw_supported
        when 1; :nw_unsupported
        end
      end
      def autocommit_ddl
        case __ub1(271)
        when 0; :ac_ddl
        when 1; :no_ac_ddl
        end
      end
      def locking_mode
        case __ub1(272)
        when 0; :lock_immediate
        when 1; :lock_delayed
        end
      end

      # shortcut method
      def schemas
        @schemas ||= list_schemas.to_a
      end
      def inspect
        "#<#{self.class.name}:(#{obj_id}) #{obj_name} #{version}>"
      end
    end

    class Rule < Base # don't use.
      register_ptype 19 # OCI_PTYPE_RULE

      # Table 6-18 Attributes Specific to Rules
      def condition;    __text(342); end # don't use.
      def eval_context_owner; __text(345); end # don't use.
      def eval_context_name; __text(346); end # don't use.
      def comment;      __text(343); end # don't use.
      # def list_action_context; __param(???); end
    end

    class RuleSet < Base  # don't use.
      register_ptype 22 # OCI_PTYPE_RULE_SET

      # Table 6-19 Attributes Specific to Rule Sets
      def eval_context_owner; __text(345); end # don't use.
      def eval_context_name; __text(346); end # don't use.
      def commen;       __text(343); end # don't use.
      # def list_rules;   __param(???); end
    end

    class EvaluationContext < Base  # don't use.
      register_ptype 21 # OCI_PTYPE_EVALUATION_CONTEXT

      # Table 6-20 Attributes Specific to Evaluation Contexts
      def evaluation_function; __text(347); end # don't use.
      def comment;      __text(343); end # don't use.
      def list_table_aliases; __param(352); end # don't use.
      def list_variable_types; __param(353); end # don't use.
    end

    class TableAlias < Base  # don't use.
      register_ptype 22 # OCI_PTYPE_TABLE_ALIAS

      # Table 6-21 Attributes Specific to Table Aliases
      def name;         __text(4); end # don't use.
      def table_name;   __text(356); end # don't use.
    end

    class VariableType < Base  # don't use.
      register_ptype 23 # OCI_PTYPE_VARIABLE_TYPE

      # Table 6-22 Attributes Specific to Variable Types
      def name;         __text(4); end # don't use.
      def var_type;     __text(348); end # don't use.
      def var_value_function; __text(349); end # don't use.
      def var_method_function; __text(350); end # don't use.
    end

    class NameValue < Base  # don't use.
      register_ptype 24 # OCI_PTYPE_NAME_VALUE

      # Table 6-23 Attributes Specific to Name Value Pair
      def name;         __text(4); end # don't use.
      def value;        __anydata(344); end # not implemented
    end
  end # OCI8::Metadata

  def describe_any(name)
    __describe(name, OCI8::Metadata::Unknown, true)
  end
  def describe_table(name, check_selectable_objects = false)
    if check_selectable_objects
      # check tables, views, synonyms and public synonyms.
      metadata = __describe(name, OCI8::Metadata::Unknown, true)
      case metadata
      when OCI8::Metadata::Table, OCI8::Metadata::View
        metadata
      when OCI8::Metadata::Synonym
        describe_table(metadata.object_name, true);
      else
        nil
      end
    else
      # check my own tables only.
      __describe(name, OCI8::Metadata::Table, false)
    end
  end
  def describe_view(name)
    __describe(name, OCI8::Metadata::View, false)
  end
  def describe_procedure(name)
    __describe(name, OCI8::Metadata::Procedure, false)
  end
  def describe_function(name)
    __describe(name, OCI8::Metadata::Function, false)
  end
  def describe_package(name)
    __describe(name, OCI8::Metadata::Package, false)
  end
  def describe_type(name)
    __describe(name, OCI8::Metadata::Type, false)
  end
  def describe_synonym(name, check_public_also = false)
    __describe(name, OCI8::Metadata::Synonym, check_public_also)
  end
  def describe_sequence(name)
    __describe(name, OCI8::Metadata::Sequence, false)
  end
  def describe_schema(name)
    __describe(name, OCI8::Metadata::Schema, false)
  end
  def describe_database(name)
    __describe(name, OCI8::Metadata::Database, false)
  end
end # OCI8
