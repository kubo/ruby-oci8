# oci8.rb -- implements OCI8::Metadata.
#
# Copyright (C) 2006-2010 KUBO Takehiro <kubo@jiubao.org>
#
# See {'Describing Schema Metadata' in Oracle Call Interface Programmer's Guide}
# [http://download-west.oracle.com/docs/cd/B19306_01/appdev.102/b14250/oci06des.htm]

#
class OCI8

  # OCI8 has methods to obtain information about database objects such
  # as tables, views, procedures, functions ans so on. The obtained
  # data are called metadata and retrived as an instance of
  # OCI8::Metadata::Base's subclass.
  #
  # List of methods which return OCI8::Metadata::Base.
  # * OCI8#describe_any(object_name)
  # * OCI8#describe_table(table_name, table_only = false)
  # * OCI8#describe_view(view_name)
  # * OCI8#describe_procedure(procedure_name)
  # * OCI8#describe_function(function_name)
  # * OCI8#describe_package(package_name)
  # * OCI8#describe_type(type_name)
  # * OCI8#describe_synonym(synonym_name, check_public_also = true)
  # * OCI8#describe_sequence(sequence_name)
  # * OCI8#describe_schema(schema_name)
  # * OCI8#describe_database(database_name)
  # * OCI8::Metadata::Type#map_method
  # * OCI8::Metadata::Type#order_method
  # * OCI8::Metadata::Type#collection_element
  #
  # List of methods which return an array of OCI8::Metadata::Base.
  # * OCI8::Cursor#column_metadata
  # * OCI8::Metadata::Database#schemas
  # * OCI8::Metadata::Schema#all_objects
  # * OCI8::Metadata::Schema#objects
  # * OCI8::Metadata::Table#columns
  # * OCI8::Metadata::Package#subprograms
  # * OCI8::Metadata::Procedure#arguments
  # * OCI8::Metadata::Function#arguments
  # * OCI8::Metadata::Type#type_attrs
  # * OCI8::Metadata::Type#type_methods
  # * OCI8::Metadata::TypeMethod#arguments
  #
  # Example:
  #   conn = OCI8.new('username/passord')
  #   table = conn.describe_table('scott.emp')
  #   table.columns.each do |col|
  #     puts "#{col.name} #{col.data_type_string}"
  #   end
  module Metadata
    # Abstract super class of Metadata classes.
    class Base
      # Table 6-1 Attributes Belonging to All Parameters

      # Returns the number of parameters.
      def num_params # :nodoc:
        attr_get_ub2(OCI_ATTR_NUM_PARAMS)
      end
      private :num_params

      # call-seq:
      #   obj_id -> integer or nil
      #
      # Returns the object ID, which is the same as the value of the
      # OBJECT_ID column from ALL_OBJECTS. It returns +nil+
      # if the database object doesn't have ID.
      def obj_id
        attr_get_ub4(OCI_ATTR_OBJ_ID)
      end

      # call-seq:
      #   obj_name -> string
      #
      # Retruns the object name such as table name, view name,
      # procedure name, and so on.
      def obj_name
        attr_get_string(OCI_ATTR_OBJ_NAME)
      end

      # call-seq:
      #   obj_schema -> string
      #
      # Retruns the schema name. It returns +nil+
      # if the database object is not defined just under a schema.
      def obj_schema
        attr_get_string(OCI_ATTR_OBJ_SCHEMA)
      end

      # The timestamp of the object
      #-
      # As far as I checked, it is current timestamp, not the object's timestamp. Why?
      #+
      #def timestamp
      #  attr_get_oradate(OCI_ATTR_TIMESTAMP)
      #end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}.#{obj_name}>"
      end
      private

      def __boolean(idx)
        attr_get_ub1(idx) == 0 ? false : true
      end
      alias __word attr_get_sb4
      def __anydata(idx); raise NotImplementedError; end

      # SQLT values to name
      DATA_TYPE_MAP = {} # :nodoc:

      # SQLT_CHR
      DATA_TYPE_MAP[1] = [:varchar2,
                          Proc.new do |p|
                            if p.charset_form == :nchar
                              "NVARCHAR2(#{p.char_size})"
                            elsif p.char_used?
                              "VARCHAR2(#{p.char_size} CHAR)"
                            else
                              "VARCHAR2(#{p.data_size})"
                            end
                          end]
      # SQLT_NUM
      DATA_TYPE_MAP[2] = [:number,
                          Proc.new do |p|
                            begin
                              case p.scale
                              when -127
                                case p.precision
                                when 0
                                  "NUMBER"
                                when 126
                                  "FLOAT"
                                else
                                  "FLOAT(#{p.precision})"
                                end
                              when 0
                                case p.precision
                                when 0
                                  "NUMBER"
                                else
                                  "NUMBER(#{p.precision})"
                                end
                              else
                                "NUMBER(#{p.precision},#{p.scale})"
                              end
                            rescue OCIError
                              "NUMBER"
                            end
                          end]
      # SQLT_LNG
      DATA_TYPE_MAP[8] = [:long, "LONG"]
      # SQLT_DAT
      DATA_TYPE_MAP[12] = [:date, "DATE"]
      # SQLT_BIN
      DATA_TYPE_MAP[23] = [:raw,
                           Proc.new do |p|
                             "RAW(#{p.data_size})"
                           end]
      # SQLT_LBI
      DATA_TYPE_MAP[24] = [:long_raw, "LONG RAW"]
      # SQLT_AFC
      DATA_TYPE_MAP[96] = [:char,
                           Proc.new do |p|
                             if p.charset_form == :nchar
                               "NCHAR(#{p.char_size})"
                             elsif p.char_used?
                               "CHAR(#{p.char_size} CHAR)"
                             else
                               "CHAR(#{p.data_size})"
                             end
                           end]
      # SQLT_IBFLOAT
      DATA_TYPE_MAP[100] = [:binary_float, "BINARY_FLOAT"]
      # SQLT_IBDOUBLE
      DATA_TYPE_MAP[101] = [:binary_double, "BINARY_DOUBLE"]
      # SQLT_RDD
      DATA_TYPE_MAP[104] = [:rowid, "ROWID"]
      # SQLT_NTY
      DATA_TYPE_MAP[108] = [:named_type,
                            Proc.new do |p|
                              "#{p.schema_name}.#{p.type_name}"
                            end]
      # SQLT_REF
      DATA_TYPE_MAP[110] = [:ref,
                            Proc.new do |p|
                              "REF #{p.schema_name}.#{p.type_name}"
                            end]
      # SQLT_CLOB
      DATA_TYPE_MAP[112] = [:clob,
                            Proc.new do |p|
                              if p.charset_form == :nchar
                                "NCLOB"
                              else
                                "CLOB"
                              end
                            end]
      # SQLT_BLOB
      DATA_TYPE_MAP[113] = [:blob, "BLOB"]
      # SQLT_BFILE
      DATA_TYPE_MAP[114] = [:bfile, "BFILE"]
      # SQLT_RSET
      DATA_TYPE_MAP[116] = [:cursor, "CURSOR"]
      # SQLT_TIMESTAMP
      DATA_TYPE_MAP[187] = [:timestamp,
                            Proc.new do |p|
                              fsprecision = p.fsprecision
                              if fsprecision == 6
                                "TIMESTAMP"
                              else
                                "TIMESTAMP(#{fsprecision})"
                              end
                            end]
      # SQLT_TIMESTAMP_TZ
      DATA_TYPE_MAP[188] = [:timestamp_tz,
                            Proc.new do |p|
                              fsprecision = p.fsprecision
                              if fsprecision == 6
                                "TIMESTAMP WITH TIME ZONE"
                              else
                                "TIMESTAMP(#{fsprecision}) WITH TIME ZONE"
                              end
                            end]
      # SQLT_INTERVAL_YM
      DATA_TYPE_MAP[189] = [:interval_ym,
                            Proc.new do |p|
                              lfprecision = p.lfprecision
                              if lfprecision == 2
                                "INTERVAL YEAR TO MONTH"
                              else
                                "INTERVAL YEAR(#{lfprecision}) TO MONTH"
                              end
                            end]
      # SQLT_INTERVAL_DS
      DATA_TYPE_MAP[190] = [:interval_ds,
                            Proc.new do |p|
                              lfprecision = p.lfprecision
                              fsprecision = p.fsprecision
                              if lfprecision == 2 && fsprecision == 6
                                "INTERVAL DAY TO SECOND"
                              else
                                "INTERVAL DAY(#{lfprecision}) TO SECOND(#{fsprecision})"
                              end
                            end]
      # SQLT_TIMESTAMP_LTZ
      DATA_TYPE_MAP[232] = [:timestamp_ltz,
                            Proc.new do |p|
                              fsprecision = p.fsprecision
                              if fsprecision == 6
                                "TIMESTAMP WITH LOCAL TIME ZONE"
                              else
                                "TIMESTAMP(#{fsprecision}) WITH LOCAL TIME ZONE"
                              end
                            end]

      def __data_type # :nodoc:
        return @data_type if defined? @data_type
        entry = DATA_TYPE_MAP[attr_get_ub2(OCI_ATTR_DATA_TYPE)]
        type = entry.nil? ? attr_get_ub2(OCI_ATTR_DATA_TYPE) : entry[0]
        type = type.call(self) if type.is_a? Proc
        @data_type = type
      end

      def __duration # :nodoc:
        case attr_get_ub2(OCI_ATTR_DURATION)
        when OCI_DURATION_SESSION
          :session
        when OCI_DURATION_TRANS
          :transaction
        when OCI_DURATION_NULL
          nil
        end
      end

      def __charset_form # :nodoc:
        case attr_get_ub1(OCI_ATTR_CHARSET_FORM)
        when 1; :implicit # for CHAR, VARCHAR2, CLOB w/o a specified set
        when 2; :nchar    # for NCHAR, NCHAR VARYING, NCLOB
        when 3; :explicit # for CHAR, etc, with "CHARACTER SET ..." syntax
        when 4; :flexible # for PL/SQL "flexible" parameters
        when 5; :lit_null # for typecheck of NULL and empty_clob() lits
        end
      end

      def __data_type_string # :nodoc:
        entry = DATA_TYPE_MAP[attr_get_ub2(OCI_ATTR_DATA_TYPE)]
        type = entry.nil? ? "unknown(#{attr_get_ub2(OCI_ATTR_DATA_TYPE)})" : entry[1]
        type = type.call(self) if type.is_a? Proc
        if respond_to?(:nullable?) && !nullable?
          type + " NOT NULL"
        else
          type
        end
      end

      def __typecode(idx) # :nodoc:
        case attr_get_ub2(idx)
        when 110; :ref         # OCI_TYPECODE_REF
        when  12; :date        # OCI_TYPECODE_DATE
        when  27; :signed8     # OCI_TYPECODE_SIGNED8
        when  28; :signed16    # OCI_TYPECODE_SIGNED16
        when  29; :signed32    # OCI_TYPECODE_SIGNED32
        when  21; :real        # OCI_TYPECODE_REAL
        when  22; :double      # OCI_TYPECODE_DOUBLE
        when 100; :binary_float  # OCI_TYPECODE_BFLOAT
        when 101; :binary_double # OCI_TYPECODE_BDOUBLE
        when   4; :float       # OCI_TYPECODE_FLOAT
        when   2; :number      # OCI_TYPECODE_NUMBER
        when   7; :decimal     # OCI_TYPECODE_DECIMAL
        when  23; :unsigned8   # OCI_TYPECODE_UNSIGNED8
        when  25; :unsigned16  # OCI_TYPECODE_UNSIGNED16
        when  26; :unsigned32  # OCI_TYPECODE_UNSIGNED32
        when 245; :octet       # OCI_TYPECODE_OCTET
        when 246; :smallint    # OCI_TYPECODE_SMALLINT
        when   3; :integer     # OCI_TYPECODE_INTEGER
        when  95; :raw         # OCI_TYPECODE_RAW
        when  32; :ptr         # OCI_TYPECODE_PTR
        when   9; :varchar2    # OCI_TYPECODE_VARCHAR2
        when  96; :char        # OCI_TYPECODE_CHAR
        when   1; :varchar     # OCI_TYPECODE_VARCHAR
        when 105; :mlslabel    # OCI_TYPECODE_MLSLABEL
        when 247; :varray      # OCI_TYPECODE_VARRAY
        when 248; :table       # OCI_TYPECODE_TABLE
        when 108; :named_type  # OCI_TYPECODE_OBJECT
        when  58; :opaque      # OCI_TYPECODE_OPAQUE
        when 122; :named_collection # OCI_TYPECODE_NAMEDCOLLECTION
        when 113; :blob        # OCI_TYPECODE_BLOB
        when 114; :bfile       # OCI_TYPECODE_BFILE
        when 112; :clob        # OCI_TYPECODE_CLOB
        when 115; :cfile       # OCI_TYPECODE_CFILE
        when 185; :time        # OCI_TYPECODE_TIME
        when 186; :time_tz     # OCI_TYPECODE_TIME_TZ
        when 187; :timestamp   # OCI_TYPECODE_TIMESTAMP
        when 188; :timestamp_tz # OCI_TYPECODE_TIMESTAMP_TZ
        when 232; :timestamp_ltz # OCI_TYPECODE_TIMESTAMP_LTZ
        when 189; :interval_ym # OCI_TYPECODE_INTERVAL_YM
        when 190; :interval_ds # OCI_TYPECODE_INTERVAL_DS
        when 104; :urowid      # OCI_TYPECODE_UROWID
        #when 228; :otmfirst   # OCI_TYPECODE_OTMFIRST
        #when 320; :otmlast    # OCI_TYPECODE_OTMLAST
        #when 228; :sysfirst   # OCI_TYPECODE_SYSFIRST
        #when 235; :syslast    # OCI_TYPECODE_SYSLAST
        when 266; :pls_integer # OCI_TYPECODE_PLS_INTEGER
        end
      end
    end

    # Information about database objects which cannot be classified
    # into other metadata classes.
    #
    # An instance of this class is returned by:
    # * OCI8::Metadata::Schema#all_objects
    class Unknown < Base
      register_ptype OCI_PTYPE_UNK
    end

    # Information about tables
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_table(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    #
    # See also:
    # * OCI8::Metadata::Base#obj_name
    # * OCI8::Metadata::Base#obj_schema
    class Table < Base
      register_ptype OCI_PTYPE_TABLE

      ## Table 6-2 Attributes Belonging to Tables or Views

      # call-seq:
      #   num_cols -> integer
      #
      # Returns number of columns
      def num_cols
        attr_get_ub2(OCI_ATTR_NUM_COLS)
      end

      # column list
      def list_columns # :nodoc:
        __param(OCI_ATTR_LIST_COLUMNS)
      end
      private :list_columns

      # call-seq:
      #   type_metadata -> an OCI8::Metadata::Type or nil
      #
      # Retruns an instance of OCI8::Metadata::Type if the table is an
      # {object table}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjint.htm#sthref61].
      # Otherwise, +nil+.
      def type_metadata
        __type_metadata(OCI8::Metadata::Type) if is_typed?
      end

      # call-seq:
      #   is_temporary? -> true or false
      #
      # Returns +true+ if the table is a
      # {temporary table}[http://download.oracle.com/docs/cd/B28359_01/server.111/b28318/schema.htm#i16096].
      # Otherwise, +false+.
      def is_temporary?
        attr_get_ub1(OCI_ATTR_IS_TEMPORARY) != 0
      end

      # call-seq:
      #   is_typed? -> true or false
      #
      # Returns +true+ if the table is a object table. Otherwise, +false+.
      def is_typed?
        attr_get_ub1(OCI_ATTR_IS_TYPED) != 0
      end

      # call-seq:
      #   duration -> :transaction, :session or nil
      #
      # Retruns +:transaction+ if the table is a
      # {transaction-specific temporary table}[http://download.oracle.com/docs/cd/B28359_01/server.111/b28286/statements_7002.htm#i2189569].
      # +:session+ if it is a
      # {session-specific temporary table}[http://download.oracle.com/docs/cd/B28359_01/server.111/b28286/statements_7002.htm#i2189569].
      # Otherwise, +nil+.
      def duration
        __duration
      end

      ## Table 6-3 Attributes Specific to Tables

      # call-seq:
      #   dba -> integer
      #
      # Returns a Data Block Address(DBA) of the segment header.
      #
      # The dba is converted to the file number and the block number
      # by DBMS_UTILITY.DATA_BLOCK_ADDRESS_FILE and
      # DBMS_UTILITY.DATA_BLOCK_ADDRESS_BLOCK respectively.
      def dba
        attr_get_ub4(OCI_ATTR_RDBA)
      end

      # call-seq:
      #   tablespace -> integer
      #
      # Returns a tablespace number the table resides in.
      def tablespace
        __word(OCI_ATTR_TABLESPACE)
      end

      # call-seq:
      #   clustered? -> true or false
      #
      # Returns +true+ if the table is part of a 
      # cluster[http://download.oracle.com/docs/cd/B28359_01/server.111/b28318/schema.htm#CNCPT608].
      # Otherwise, +false+.
      def clustered?
        attr_get_ub1(OCI_ATTR_CLUSTERED) != 0
      end

      # call-seq:
      #   partitioned? -> true or false
      #
      # Returns +true+ if the table is a
      # {partitioned table}[http://download.oracle.com/docs/cd/B28359_01/server.111/b28318/partconc.htm#i449863].
      # Otherwise, +false+.
      def partitioned?
        attr_get_ub1(OCI_ATTR_PARTITIONED) != 0
      end

      # call-seq:
      #   index_only? -> true or false
      #
      # Returns +true+ if the table is an
      # {index-organized table}[http://download.oracle.com/docs/cd/B28359_01/server.111/b28318/schema.htm#i23877]
      # Otherwise, +false+.
      def index_only?
        attr_get_ub1(OCI_ATTR_INDEX_ONLY) != 0
      end

      # call-seq:
      #   columns -> list of column information
      #
      # Returns an array of OCI8::Metadata::Column of the table.
      def columns
        @columns ||= list_columns.to_a
      end
    end

    # Information about views
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_table(name, false)
    # * OCI8#describe_view(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    #
    # See also:
    # * OCI8::Metadata::Base#obj_name
    # * OCI8::Metadata::Base#obj_schema
    class View < Base
      register_ptype OCI_PTYPE_VIEW

      ## Table 6-2 Attributes Belonging to Tables or Views

      # call-seq:
      #   num_cols -> integer
      #
      # Returns number of columns
      def num_cols
        attr_get_ub2(OCI_ATTR_NUM_COLS)
      end

      # column list
      def list_columns # :nodoc:
        __param(OCI_ATTR_LIST_COLUMNS)
      end
      private :list_columns

      # This does't work for view.
      #def type_metadata
      #  __type_metadata(OCI8::Metadata::Type)
      #end

      # This does't work for view.
      #def is_temporary?
      #  attr_get_ub1(OCI_ATTR_IS_TEMPORARY) != 0
      #end

      # This does't work for view.
      #def is_typed?
      #  attr_get_ub1(OCI_ATTR_IS_TYPED) != 0
      #end

      # This does't work for view.
      #def duration
      #  __duration
      #end

      # call-seq:
      #   columns -> list of column information
      #
      # Returns an array of OCI8::Metadata::Column of the table.
      def columns
        @columns ||= list_columns.to_a
      end
    end

    # Information about PL/SQL subprograms
    #
    # A PL/SQL subprogram is a named PL/SQL block that can be invoked
    # with a set of parameters. A subprogram can be either a procedure
    # or a function.
    #
    # This is the abstract base class of OCI8::Metadata::Procedure and
    # OCI8::Metadata::Function.
    class Subprogram < Base
      ## Table 6-4 Attribute Belonging to Procedures or Functions

      # Argument list
      def list_arguments # :nodoc:
        __param(OCI_ATTR_LIST_ARGUMENTS)
      end
      private :list_arguments

      def obj_id # :nodoc:
        super if is_standalone?
      end

      def obj_name # :nodoc:
        is_standalone? ? super : attr_get_string(OCI_ATTR_NAME)
      end

      def obj_schema # :nodoc:
        super if is_standalone?
      end

      # call-seq:
      #   is_invoker_rights? -> true or false
      #
      # Returns +true+ if the subprogram has
      # {invoker's rights}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28370/subprograms.htm#i18574].
      # Otherwise, +false+.
      def is_invoker_rights?
        attr_get_ub1(OCI_ATTR_IS_INVOKER_RIGHTS) != 0
      end

      ## Table 6-5 Attributes Specific to Package Subprograms

      # name of the subprogram
      #
      #def name
      #  attr_get_string(OCI_ATTR_NAME)
      #end
      alias name obj_name # :nodoc: for backward compatibility

      # call-seq:
      #   overload_id -> integer or nil
      #
      # Returns +nil+ for a standalone stored subprogram,
      # positive integer for a
      # {overloaded packaged subprogram}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28370/subprograms.htm#i12352].
      # , otherwise zero.
      def overload_id
        attr_get_ub2(OCI_ATTR_OVERLOAD_ID) unless is_standalone?
      end

      # call-seq:
      #   arguments -> list of argument information
      #
      # Returns an array of OCI8::Metadata::Argument of the subprogram.
      # If it is a function, the first array element is information of its return type.
      def arguments
        @arguments ||= list_arguments.to_a
      end

      # call-seq:
      #   is_standalone? -> true or false
      #
      # Returns +true+ if the subprogram is standalone, +false+
      # if packaged.
      def is_standalone?
        @is_standalone = true unless defined? @is_standalone
        @is_standalone
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name}>"
      end
    end

    # Information about procedures
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_procedure(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    # * OCI8::Metadata::Package#subprograms
    #
    # See OCI8::Metadata::Subprogram.
    class Procedure < Subprogram
      register_ptype OCI_PTYPE_PROC
    end

    # Information about functions
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_function(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    # * OCI8::Metadata::Package#subprograms
    #
    # See OCI8::Metadata::Subprogram.
    class Function < Subprogram
      register_ptype OCI_PTYPE_FUNC
    end

    # Information about packages.
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_package(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    class Package < Base
      register_ptype OCI_PTYPE_PKG

      ## Table 6-6 Attributes Belonging to Packages

      # subprogram list
      def list_subprograms # :nodoc:
        __param(OCI_ATTR_LIST_SUBPROGRAMS)
      end
      private :list_subprograms

      # call-seq:
      #   is_invoker_rights? -> true or false
      #
      # Returns +true+ if the package subprograms have
      # {invoker's rights}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28370/subprograms.htm#i18574].
      # Otherwise, +false+.
      def is_invoker_rights?
        attr_get_ub1(OCI_ATTR_IS_INVOKER_RIGHTS) != 0
      end

      # call-seq:
      #   subprograms -> array
      #
      # Returns an array of Function and Procedure defined within the Package.
      def subprograms
        @subprograms ||= list_subprograms.to_a.each do |prog|
          prog.instance_variable_set(:@is_standalone, false)
        end
      end
    end

    # Information about types
    #
    # An instance of this class is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_type(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    class Type < Base
      register_ptype OCI_PTYPE_TYPE

      ## Table 6-7 Attributes Belonging to Types

      # to type metadata if possible
      def type_metadata
        self
      end

      # call-seq:
      #   typecode -> :named_type or :named_collection
      #
      # Returns +:named_type+ if the type is an object type,
      # +:named_collection+ if it is a nested table or a varray.
      def typecode
        __typecode(OCI_ATTR_TYPECODE)
      end

      # call-seq:
      #   collection_typecode -> :table, :varray or nil
      #
      # Returns +:table+ if the type is a nested table,
      # +:varray+ if it is a varray. Otherwise, +nil+.
      def collection_typecode
        __typecode(OCI_ATTR_COLLECTION_TYPECODE) if typecode == :named_collection
      end

      # call-seq:
      #   is_incomplete_type? -> boolean
      #
      # Returns +true+ if the type is an
      # {incomplete type}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjmng.htm#i1003083],
      # which is used for {forward declaration}[http://en.wikipedia.org/wiki/Forward_declaration].
      # Otherwise, +false+.
      def is_incomplete_type?
        attr_get_ub1(OCI_ATTR_IS_INCOMPLETE_TYPE) != 0
      end

      # call-seq:
      #   is_system_type? -> boolean
      #
      # Always returns +false+ because there is no way to create
      # a type metadata object for a system type such as +NUMBER+,
      # +CHAR+ and +VARCHAR+.
      def is_system_type? # :nodoc:
        attr_get_ub1(OCI_ATTR_IS_SYSTEM_TYPE) != 0
      end

      # call-seq:
      #   is_predefined_type? -> boolean
      #
      # Always returns +false+.
      #--
      # I don't know the definition of predefined type...
      def is_predefined_type? # :nodoc:
        attr_get_ub1(OCI_ATTR_IS_PREDEFINED_TYPE) != 0
      end

      # call-seq:
      #   is_transient_type? -> boolean
      #
      # Always returns +false+ because there is no way to create
      # a type metadata object for a transient type, which is a type
      # dynamically created by C API.
      def is_transient_type? # :nodoc:
        attr_get_ub1(OCI_ATTR_IS_TRANSIENT_TYPE) != 0
      end

      # call-seq:
      #   is_predefined_type? -> boolean
      #
      # Always returns +false+.
      #--
      # I don't know the definition of system generated type.
      # What is different with system type and predefined type.
      def is_system_generated_type? # :nodoc:
        attr_get_ub1(OCI_ATTR_IS_SYSTEM_GENERATED_TYPE) != 0
      end

      # call-seq:
      #   has_nested_table? -> boolean
      #
      # Returns +true+ if the type is a nested table or
      # has a nested table attribute.
      # Otherwise, +false+.
      def has_nested_table?
        attr_get_ub1(OCI_ATTR_HAS_NESTED_TABLE) != 0
      end

      # call-seq:
      #   has_lob? -> boolean
      #
      # Returns +true+ if the type has a CLOB, NCLOB or BLOB
      # attribute.
      # Otherwise, +false+.
      def has_lob?
        attr_get_ub1(OCI_ATTR_HAS_LOB) != 0
      end

      # call-seq:
      #   has_file? -> boolean
      #
      # Returns +true+ if the type has a BFILE attribute.
      # Otherwise, +false+.
      def has_file?
        attr_get_ub1(OCI_ATTR_HAS_FILE) != 0
      end

      # call-seq:
      #   collection_element -> element information of the collection type
      #
      # Returns an OCI8::Metadata::Collection if the type is a nested
      # table or a varray.
      # Otherwise, +nil+.
      def collection_element
        __param(OCI_ATTR_COLLECTION_ELEMENT) if typecode == :named_collection
      end

      # call-seq:
      #   num_type_attrs -> integer
      #
      # Returns number of type attributes.
      def num_type_attrs
        attr_get_ub2(OCI_ATTR_NUM_TYPE_ATTRS)
      end

      # list of type attributes
      def list_type_attrs # :nodoc:
        __param(OCI_ATTR_LIST_TYPE_ATTRS)
      end
      private :list_type_attrs

      # call-seq:
      #   num_type_methods -> integer
      #
      # Returns number of type methods.
      def num_type_methods
        attr_get_ub2(OCI_ATTR_NUM_TYPE_METHODS)
      end

      # list of type methods
      def list_type_methods # :nodoc:
        __param(OCI_ATTR_LIST_TYPE_METHODS)
      end
      private :list_type_methods

      # call-seq:
      #   map_method -> map method information
      #
      # Returns an instance of OCI8::Metadata::TypeMethod of a
      # {map method}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjbas.htm#sthref180]
      # if it is defined in the type. Otherwise, +nil+.
      def map_method
        __param(OCI_ATTR_MAP_METHOD)
      end

      # call-seq:
      #   order_method -> order method information
      #
      # Returns an instance of OCI8::Metadata::TypeMethod of a
      # {order method}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjbas.htm#sthref185]
      # if it is defined in the type. Otherwise, +nil+.
      def order_method
        __param(OCI_ATTR_ORDER_METHOD)
      end

      # call-seq:
      #   is_invoker_rights? -> boolean
      #
      # Returns +true+ if the type has
      # {invoker's rights}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/appdev.111/b28371/adobjdes.htm#ADOBJ00810].
      # Otherwise, +false+.
      def is_invoker_rights?
        attr_get_ub1(OCI_ATTR_IS_INVOKER_RIGHTS) != 0
      end

      # call-seq:
      #   name -> string
      #
      # Returns the type name.
      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # call-seq:
      #   schema_name -> string
      #
      # Returns the schema name where the type has been created.
      def schema_name
        attr_get_string(OCI_ATTR_SCHEMA_NAME)
      end

      # call-seq:
      #   is_final_type? -> boolean
      #
      # Returns +true+ if the type is a
      # {final type}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjbas.htm#CIHFBHFC];
      # in other words, subtypes cannot be derived from the type.
      # Otherwise, +false+.
      def is_final_type?
        attr_get_ub1(OCI_ATTR_IS_FINAL_TYPE) != 0
      end

      # call-seq:
      #   is_instantiable_type? -> boolean
      #
      # Returns +true+ if the type is not declared without
      # {<tt>NOT INSTANTIABLE</tt>}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjbas.htm#i456586].
      # Otherwise, +false+.
      def is_instantiable_type?
        attr_get_ub1(OCI_ATTR_IS_INSTANTIABLE_TYPE) != 0
      end

      # call-seq:
      #   is_subtype? -> boolean
      #
      # Returns +true+ if the type is a
      # {subtype}[http://download.oracle.com/docs/cd/B28359_01/appdev.111/b28371/adobjbas.htm#BCFJJADG].
      # Otherwise, +false+.
      def is_subtype?
        attr_get_ub1(OCI_ATTR_IS_SUBTYPE) != 0
      end

      # call-seq:
      #   supertype_schema_name -> string or nil
      #
      # Returns the supertype's schema name if the type is a subtype.
      # Otherwise, +nil+.
      def supertype_schema_name
        attr_get_string(OCI_ATTR_SUPERTYPE_SCHEMA_NAME) if is_subtype?
      end

      # call-seq:
      #   supertype_name -> string or nil
      #
      # Returns the supertype's name if the type is a subtype.
      # Otherwise, +nil+.
      def supertype_name
        attr_get_string(OCI_ATTR_SUPERTYPE_NAME) if is_subtype?
      end

      # call-seq:
      #   type_attrs -> list of attribute information
      #
      # Returns an array of OCI8::Metadata::TypeAttr which the type has.
      def type_attrs
        @type_attrs ||= list_type_attrs.to_a
      end

      # call-seq:
      #   type_methods -> list of method information
      #
      # Returns an array of OCI8::Metadata::TypeMethod which the type has.
      def type_methods
        @type_methods ||= list_type_methods.to_a
      end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{schema_name}.#{name}>"
      end
    end

    # Metadata for a type attribute.
    #
    # This is returned by:
    # * OCI8::Metadata::Type#type_attrs
    class TypeAttr < Base
      register_ptype OCI_PTYPE_TYPE_ATTR

      ## Table 6-8 Attributes Belonging to Type Attributes

      # The maximum size of the type attribute. This length is
      # returned in bytes and not characters for strings and raws. It
      # returns 22 for NUMBERs.
      def data_size
        attr_get_ub2(OCI_ATTR_DATA_SIZE)
      end

      # typecode
      def typecode
        __typecode(OCI_ATTR_TYPECODE)
      end

      # the datatype of the type
      def data_type
        __data_type
      end

      # the type attribute name
      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # The precision of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? attr_get_sb2(OCI_ATTR_PRECISION) : attr_get_ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        attr_get_sb1(OCI_ATTR_SCALE)
      end

      # A string which is the type name. The returned value will
      # contain the type name if the datatype is <tt>:named_type</tt>
      # or <tt>:ref</tt>. If the datatype is <tt>:named_type</tt>, the
      # name of the named datatype's type is returned. If the datatype
      # is <tt>:ref</tt>, the type name of the named datatype pointed
      # to by the REF is returned.
      def type_name
        attr_get_string(OCI_ATTR_TYPE_NAME)
      end

      # schema name where the type has been created.
      def schema_name
        attr_get_string(OCI_ATTR_SCHEMA_NAME)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # character set id if the type attribute is of a string/character type.
      def charset_id
        attr_get_ub2(OCI_ATTR_CHARSET_ID)
      end

      # character set form, if the type attribute is of a string/character type.
      def charset_form
        __charset_form
      end

      if OCI8.oracle_client_version >= ORAVER_9_0
        # The fractional seconds precision of a datetime or interval.
        #
        # (unavailable on Oracle 8.1 or lower)
        def fsprecision
          attr_get_ub1(OCI_ATTR_FSPRECISION)
        end

        # The leading field precision of an interval
        #
        # (unavailable on Oracle 8.1 or lower)
        def lfprecision
          attr_get_ub1(OCI_ATTR_LFPRECISION)
        end
      end

      # character set name if the type attribute is of a string/character type.
      def charset_name
        __con.charset_id2name(charset_id)
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name} #{__data_type_string}>"
      end
    end

    # Metadata for a type method.
    #
    # This is returned by:
    # * OCI8::Metadata::Type#type_methods
    # * OCI8::Metadata::Type#map_method
    # * OCI8::Metadata::Type#order_method
    #
    #--
    # How can I know whether FUNCTION or PROCEDURE?
    #++
    class TypeMethod < Base
      register_ptype OCI_PTYPE_TYPE_METHOD

      ## Table 6-9 Attributes Belonging to Type Methods

      # Name of method (procedure or function)
      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # encapsulation level of the method. Values are <tt>:public</tt>
      # or <tt>:private</tt>.
      def encapsulation
        case attr_get_ub4(OCI_ATTR_ENCAPSULATION)
        when 0; :private
        when 1; :public
        end
      end

      # argument list
      def list_arguments
        __param(OCI_ATTR_LIST_ARGUMENTS)
      end
      private :list_arguments

      # indicates method is has rsult
      def has_result?
        __boolean(OCI_ATTR_HAS_RESULT)
      end

      # indicates method is a constructor
      def is_constructor?
        __boolean(OCI_ATTR_IS_CONSTRUCTOR)
      end

      # indicates method is a destructor
      def is_destructor?
        __boolean(OCI_ATTR_IS_DESTRUCTOR)
      end

      # indicates method is an operator
      def is_operator?
        __boolean(OCI_ATTR_IS_OPERATOR)
      end

      # indicates method is selfish
      def is_selfish?
        __boolean(OCI_ATTR_IS_SELFISH)
      end

      # indicates method is a map method
      def is_map?
        __boolean(OCI_ATTR_IS_MAP)
      end

      # Indicates method is an order method
      def is_order?
        __boolean(OCI_ATTR_IS_ORDER)
      end

      # indicates "Read No Data State"(does not query database tables) is set.
      def is_rnds?
        __boolean(OCI_ATTR_IS_RNDS)
      end

      # Indicates "Read No Package State"(does not reference the values of packaged variables) is set.
      def is_rnps?
        __boolean(OCI_ATTR_IS_RNPS)
      end

      # indicates "Write No Data State"(does not modify tables) is set.
      def is_wnds?
        __boolean(OCI_ATTR_IS_WNDS)
      end

      # indicates "Write No Package State"(does not change the values of packaged variables) is set.
      def is_wnps?
        __boolean(OCI_ATTR_IS_WNPS)
      end

      # indicates this is a final method
      def is_final_method?
        __boolean(OCI_ATTR_IS_FINAL_METHOD)
      end

      # indicates this is an instantiable method
      def is_instantiable_method?
        __boolean(OCI_ATTR_IS_INSTANTIABLE_METHOD)
      end

      # indicates this is an overriding method
      def is_overriding_method?
        __boolean(OCI_ATTR_IS_OVERRIDING_METHOD)
      end

      # array of TypeArgument objects.
      #
      # The first element is the return type in case of Function.
      def arguments
        @arguments ||= list_arguments.to_a
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name}>"
      end
    end

    # Metadata for a collection.
    #
    # This is returned by:
    # * OCI8::Metadata::Type.collection_element
    class Collection < Base
      register_ptype OCI_PTYPE_TYPE_COLL

      ## Table 6-10 Attributes Belonging to Collection Types 

      # The maximum size of the type attribute. This length is
      # returned in bytes and not characters for strings and raws. It
      # returns 22 for NUMBERs.
      def data_size
        attr_get_ub2(OCI_ATTR_DATA_SIZE)
      end

      # typecode
      def typecode
        __typecode(OCI_ATTR_TYPECODE)
      end

      # the datatype of the type
      def data_type
        __data_type
      end

      # the number of elements in an array. It is only valid for
      # collections that are arrays.
      def num_elems
        attr_get_ub4(OCI_ATTR_NUM_ELEMS)
      end

      # The precision of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? attr_get_sb2(OCI_ATTR_PRECISION) : attr_get_ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        attr_get_sb1(OCI_ATTR_SCALE)
      end

      # A string which is the type name. The returned value will
      # contain the type name if the datatype is <tt>:named_type</tt> or <tt>:ref</tt>.
      # If the datatype is <tt>:named_type</tt>, the name of the named datatype's
      # type is returned. If the datatype is <tt>:ref</tt>, the type name
      # of the named datatype pointed to by the REF is returned.
      def type_name
        attr_get_string(OCI_ATTR_TYPE_NAME)
      end

      # schema name where the type has been created.
      def schema_name
        attr_get_string(OCI_ATTR_SCHEMA_NAME)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # character set id if the type attribute is of a string/character type.
      def charset_id
        attr_get_ub2(OCI_ATTR_CHARSET_ID)
      end

      # character set form, if the type attribute is of a string/character type.
      def charset_form
        __charset_form
      end

      # character set name if the type attribute is of a string/character type.
      def charset_name
        __charset_name(charset_id)
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{__data_type_string}>"
      end
    end

    # Metadata for a synonym.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_synonym(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    class Synonym < Base
      register_ptype OCI_PTYPE_SYN

      ## Table 6-11 Attributes Belonging to Synonyms

      # object id
      def objid
        @objid ||= attr_get_ub4(OCI_ATTR_OBJID)
      end

      # schema name of the synonym translation
      def schema_name
        @schema_name ||= attr_get_string(OCI_ATTR_SCHEMA_NAME)
      end

      # object name of the synonym translation
      def name
        @name ||= attr_get_string(OCI_ATTR_NAME)
      end

      # database link name of the synonym translation or nil
      def link
        @link ||= attr_get_string(OCI_ATTR_LINK)
        @link.size == 0 ? nil : @link
      end

      # full-qualified synonym translation name with schema, object and database link name.
      def translated_name
        if link.nil?
          schema_name + '.' + name
        else
          schema_name + '.' + name + '@' + link
        end
      end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}.#{obj_name}>"
      end
    end

    # Metadata for a sequence.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_sequence(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    class Sequence < Base
      register_ptype OCI_PTYPE_SEQ

      ## Table 6-12 Attributes Belonging to Sequences

      # object id
      def objid
        attr_get_ub4(OCI_ATTR_OBJID)
      end

      # minimum value
      def min
        __oraint(OCI_ATTR_MIN)
      end

      # maximum value
      def max
        __oraint(OCI_ATTR_MAX)
      end

      # increment
      def incr
        __oraint(OCI_ATTR_INCR)
      end

      # number of sequence numbers cached; zero if the sequence is not a cached sequence.
      def cache
        __oraint(OCI_ATTR_CACHE)
      end

      # whether the sequence is ordered
      def order?
        __boolean(OCI_ATTR_ORDER)
      end

      # high-water mark
      def hw_mark
        __oraint(OCI_ATTR_HW_MARK)
      end
    end

    # Metadata for a sequence.
    #
    # This is returned by:
    # * OCI8::Metadata::Table#columns
    class Column < Base
      register_ptype OCI_PTYPE_COL

      ## Table 6-13 Attributes Belonging to Columns of Tables or Views

      if OCI8.oracle_client_version >= ORAVER_9_0
        # returns the type of length semantics of the column.
        # [<tt>:byte</tt>]  byte-length semantics
        # [<tt>:char</tt>]  character-length semantics.
        #
        # (unavailable on Oracle 8.1 or lower)
        def char_used?
          attr_get_ub1(OCI_ATTR_CHAR_USED) != 0
        end

        # returns the column character length which is the number of
        # characters allowed in the column. It is the counterpart of
        # OCI8::Metadata::Column#data_size which gets the byte length.
        def char_size
          attr_get_ub2(OCI_ATTR_CHAR_SIZE)
        end
      else
        def char_used?
          false
        end

        def char_size
          data_size
        end
      end

      # The maximum size of the column. This length is
      # returned in bytes and not characters for strings and raws.
      # This returns character length multiplied by NLS ratio for
      # character-length semantics columns when using Oracle 9i
      # or upper.
      def data_size
        attr_get_ub2(OCI_ATTR_DATA_SIZE)
      end

      # the datatype of the column.
      def data_type
        __data_type
      end

      # column name
      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # The precision of numeric columns. If the precision is nonzero
      # and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? attr_get_sb2(OCI_ATTR_PRECISION) : attr_get_ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric columns. If the precision is nonzero and
      # scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        attr_get_sb1(OCI_ATTR_SCALE)
      end

      # Returns 0 if null values are not permitted for the column
      def nullable?
        __boolean(OCI_ATTR_IS_NULL)
      end

      # Returns a string which is the type name. The returned value
      # will contain the type name if the datatype is <tt>:named_type</tt> or
      # <tt>:ref</tt>. If the datatype is <tt>:named_type</tt>, the name of the named
      # datatype's type is returned. If the datatype is <tt>:ref</tt>, the
      # type name of the named datatype pointed to by the REF is
      # returned
      def type_name
        rv = attr_get_string(OCI_ATTR_TYPE_NAME)
        rv.length == 0 ? nil : rv
      end

      # Returns a string with the schema name under which the type has been created
      def schema_name
        rv = attr_get_string(OCI_ATTR_SCHEMA_NAME)
        rv.length == 0 ? nil : rv
      end

      # to type metadata if possible
      def type_metadata
        case attr_get_ub2(OCI_ATTR_DATA_TYPE)
        when 108, 110 # named_type or ref
          __type_metadata(OCI8::Metadata::Type)
        else
          nil
        end
      end

      # The character set id, if the column is of a string/character type
      def charset_id
        attr_get_ub2(OCI_ATTR_CHARSET_ID)
      end

      # The character set form, if the column is of a string/character type
      def charset_form
        __charset_form
      end

      ## Table 6-8 Attributes Belonging to Type Attributes
      ## But Column also have these.

      if OCI8.oracle_client_version >= ORAVER_9_0
        # The fractional seconds precision of a datetime or interval.
        #
        # (unavailable on Oracle 8.1 or lower)
        def fsprecision
          attr_get_ub1(OCI_ATTR_FSPRECISION)
        end

        # The leading field precision of an interval
        #
        # (unavailable on Oracle 8.1 or lower)
        def lfprecision
          attr_get_ub1(OCI_ATTR_LFPRECISION)
        end
      end

      # The character set name, if the column is of a string/character type
      def charset_name
        __charset_name(charset_id)
      end

      def data_type_string
        __data_type_string
      end
      alias :type_string :data_type_string # :nodoc: old name of data_type_string

      def to_s
        %Q{"#{name}" #{__data_type_string}}
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name} #{__data_type_string}>"
      end
    end

    # Abstract super class of Argument, TypeArgument and TypeResult.
    class ArgBase < Base
      ## Table 6-14 Attributes Belonging to Arguments/Results

      # the argument name
      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # the position of the argument in the argument list.
      def position
        attr_get_ub2(OCI_ATTR_POSITION)
      end

      # typecode
      def typecode
        __typecode(OCI_ATTR_TYPECODE)
      end

      # the datatype of the argument
      def data_type
        __data_type
      end

      # The size of the datatype of the argument. This length is
      # returned in bytes and not characters for strings and raws. It
      # returns 22 for NUMBERs.
      def data_size
        attr_get_ub2(OCI_ATTR_DATA_SIZE)
      end

      # The precision of numeric arguments. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? attr_get_sb2(OCI_ATTR_PRECISION) : attr_get_ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric arguments. If the precision is nonzero
      # and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        attr_get_sb1(OCI_ATTR_SCALE)
      end

      # The datatype levels. This attribute always returns zero.
      def level
        attr_get_ub2(OCI_ATTR_LEVEL)
      end

      # Indicates whether an argument has a default
      def has_default
        attr_get_ub1(OCI_ATTR_HAS_DEFAULT)
      end

      # The list of arguments at the next level (when the argument is
      # of a record or table type).
      def list_arguments
        __param(OCI_ATTR_LIST_ARGUMENTS)
      end
      private :list_arguments

      # Indicates the argument mode. Values are <tt>:in</tt>,
      # <tt>:out</tt> or <tt>:inout</tt>
      def iomode
        case attr_get_ub4(OCI_ATTR_IOMODE)
        when 0; :in
        when 1; :out
        when 2; :inout
        end
      end

      # Returns a radix (if number type)
      def radix
        attr_get_ub1(OCI_ATTR_RADIX)
      end

      # doesn't work.
      # def nullable?
      #   __boolean(OCI_ATTR_IS_NULL)
      # end

      # Returns a string which is the type name, or the package name
      # in the case of package local types. The returned value will
      # contain the type name if the datatype is <tt>:named_type</tt> or
      # <tt>:ref</tt>. If the datatype is <tt>:named_type</tt>, the name of the named
      # datatype's type is returned. If the datatype is <tt>:ref</tt>, the type
      # name of the named datatype pointed to by the REF is returned.
      def type_name
        attr_get_string(OCI_ATTR_TYPE_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the schema name
      # under which the type was created, or under which the package
      # was created in the case of package local types
      def schema_name
        attr_get_string(OCI_ATTR_SCHEMA_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the type name,
      # in the case of package local types
      def sub_name
        attr_get_string(OCI_ATTR_SUB_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the database
      # link name of the database on which the type exists. This can
      # happen only in the case of package local types, when the
      # package is remote.
      def link
        attr_get_string(OCI_ATTR_LINK)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # Returns the character set ID if the argument is of a
      # string/character type
      def charset_id
        attr_get_ub2(OCI_ATTR_CHARSET_ID)
      end

      # Returns the character set form if the argument is of a
      # string/character type
      def charset_form
        __charset_form
      end

      # Returns the character set name if the argument is of a
      # string/character type
      def charset_name
        __charset_name(charset_id)
      end

      # The list of arguments at the next level (when the argument is
      # of a record or table type).
      def arguments
        @arguments ||= list_arguments.to_a
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name} #{__data_type_string}>"
      end
    end

    # Metadata for a argument.
    #
    # This is returned by:
    # * OCI8::Metadata::Procedure#arguments
    # * OCI8::Metadata::Function#arguments
    #
    # See ArgBase's methods.
    class Argument < ArgBase
      register_ptype OCI_PTYPE_ARG
    end

    # Metadata for a argument.
    #
    # This is returned by:
    # * OCI8::Metadata::TypeMethod#arguments
    #
    # See ArgBase's methods.
    class TypeArgument < ArgBase
      register_ptype OCI_PTYPE_TYPE_ARG
    end

    # Metadata for a argument.
    #
    # This is returned by: .... unknown.
    # What's method returns this value?
    #
    # See ArgBase's methods.
    class TypeResult < ArgBase
      register_ptype OCI_PTYPE_TYPE_RESULT
    end

    # internal use only.
    class List < Base # :nodoc:
      register_ptype OCI_PTYPE_LIST

      ## Table 6-15 List Attributes

      if OCI8::oracle_client_version < OCI8::ORAVER_8_1
        def ltype
          raise "This feature is unavailable on Oracle 8.0"
        end
      else
        def ltype
          attr_get_ub2(OCI_ATTR_LTYPE)
        end
      end

      # convert to array
      def to_a
        # Table 6-15 List Attributes
        case ltype
        when OCI_LTYPE_COLUMN;        offset = 1
        when OCI_LTYPE_ARG_PROC;      offset = 1
        when OCI_LTYPE_ARG_FUNC;      offset = 0
        when OCI_LTYPE_SUBPRG;        offset = 0
        when OCI_LTYPE_TYPE_ATTR;     offset = 1
        when OCI_LTYPE_TYPE_METHOD;   offset = 1
        when OCI_LTYPE_TYPE_ARG_PROC; offset = 0
        when OCI_LTYPE_TYPE_ARG_FUNC; offset = 1
        when OCI_LTYPE_SCH_OBJ;       offset = 0
        when OCI_LTYPE_DB_SCH;        offset = 0
        #when OCI_LTYPE_TYPE_SUBTYPE;  offset = ?
        #when OCI_LTYPE_TABLE_ALIAS;   offset = ?
        #when OCI_LTYPE_VARIABLE_TYPE; offset = ?
        #when OCI_LTYPE_NAME_VALUE;    offset = ?
        else
          raise NotImplementedError, "unsupported list type #{list.ltype}"
        end
        ary = []
        0.upto(num_params - 1) do |i|
          ary << __param_at(i + offset)
        end
        ary
      end
    end

    # Metadata for a schema.
    #
    # This is returned by:
    # * OCI8#describe_schema(schema_name)
    # * OCI8::Metadata::Database#schemas
    class Schema < Base
      register_ptype OCI_PTYPE_SCHEMA

      ## Table 6-16 Attributes Specific to Schemas

      def list_objects
        __param(OCI_ATTR_LIST_OBJECTS)
      end
      private :list_objects

      # array of objects in the schema.
      # This includes unusable objects.
      def all_objects
        @all_objects ||=
          begin
            begin
              objs = list_objects
            rescue OCIError => exc
              if exc.code != -1
                raise
              end
              # describe again.
              objs = __con.describe_schema(obj_schema).list_objects
            end
            objs.to_a.collect! do |elem|
              case elem
              when OCI8::Metadata::Type
                # to avoid a segmentation fault
                begin
                  __con.describe_type(elem.obj_schema + '.' + elem.obj_name)
                rescue OCIError
                  # ignore ORA-24372: invalid object for describe
                  raise if $!.code != 24372
                end
              else
                elem
              end
            end.compact
          end
      end

      # array of objects in the schema.
      def objects
        @objects ||= all_objects.reject do |obj|
          case obj
          when Unknown
            true
          when Synonym
            begin
              obj.objid
              false
            rescue OCIError
              true
            end
          else
            false
          end
        end
      end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}>"
      end
    end

    # Metadata for a database.
    #
    # This is returned by:
    # * OCI8#describe_database(database_name)
    class Database < Base
      register_ptype OCI_PTYPE_DATABASE

      # Table 6-17 Attributes Specific to Databases

      # database version
      def version
        attr_get_string(OCI_ATTR_VERSION)
      end

      # database character set Id
      def charset_id
        attr_get_ub2(OCI_ATTR_CHARSET_ID)
      end

      # database national language support character set Id
      def ncharset_id
        attr_get_ub2(OCI_ATTR_NCHARSET_ID)
      end

      # List of schemas in the database
      def list_schemas
        __param(OCI_ATTR_LIST_SCHEMAS)
      end
      private :list_schemas

      # Maximum length of a procedure name
      def max_proc_len
        attr_get_ub4(OCI_ATTR_MAX_PROC_LEN)
      end

      # Maximum length of a column name
      def max_column_len
        attr_get_ub4(OCI_ATTR_MAX_COLUMN_LEN)
      end

      # How a COMMIT operation affects cursors and prepared statements in
      # the database. Values are:
      # [<tt>:cusror_open</tt>]   preserve cursor state as before the commit
      #                           operation
      # [<tt>:cursor_closed</tt>] cursors are closed on COMMIT, but the
      #                           application can still re-execute the
      #                           statement without re-preparing it
      def cursor_commit_behavior
        case attr_get_ub1(OCI_ATTR_CURSOR_COMMIT_BEHAVIOR)
        when 0; :cusror_open
        when 1; :cursor_closed
        end
      end

      # Maximum length of a catalog (database) name
      def max_catalog_namelen
        attr_get_ub1(OCI_ATTR_MAX_CATALOG_NAMELEN)
      end

      # Position of the catalog in a qualified table. Values are
      # <tt>:cl_start</tt> and <tt>:cl_end</tt>
      def catalog_location
        case attr_get_ub1(OCI_ATTR_CATALOG_LOCATION)
        when 0; :cl_start
        when 1; :cl_end
        end
      end

      # Does database support savepoints? Values are
      # <tt>:sp_supported</tt> and <tt>:sp_unsupported</tt>
      def savepoint_support
        case attr_get_ub1(OCI_ATTR_SAVEPOINT_SUPPORT)
        when 0; :sp_supported
        when 1; :sp_unsupported
        end
      end

      # Does database support the nowait clause? Values are
      # <tt>:nw_supported</tt> and <tt>:nw_unsupported</tt>
      def nowait_support
        case attr_get_ub1(OCI_ATTR_NOWAIT_SUPPORT)
        when 0; :nw_supported
        when 1; :nw_unsupported
        end
      end

      # Is autocommit mode required for DDL statements? Values are
      # <tt>:ac_ddl</tt> and <tt>:no_ac_ddl</tt>
      def autocommit_ddl
        case attr_get_ub1(OCI_ATTR_AUTOCOMMIT_DDL)
        when 0; :ac_ddl
        when 1; :no_ac_ddl
        end
      end

      # Locking mode for the database. Values are <tt>:lock_immediate</tt> and
      # <tt>:lock_delayed</tt>
      def locking_mode
        case attr_get_ub1(OCI_ATTR_LOCKING_MODE)
        when 0; :lock_immediate
        when 1; :lock_delayed
        end
      end

      # database character set name
      def charset_name
        __charset_name(charset_id)
      end

      # database national language support character set name
      def ncharset_name
        __charset_name(ncharset_id)
      end

      # array of Schema objects in the database
      def schemas
        @schemas ||= list_schemas.to_a
      end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{obj_name} #{version}>"
      end
    end

=begin
    class Rule < Base # :nodoc:
      register_ptype OCI_PTYPE_RULE

      # Table 6-18 Attributes Specific to Rules

      def condition
        attr_get_string(OCI_ATTR_CONDITION)
      end

      def eval_context_owner
        attr_get_string(OCI_ATTR_EVAL_CONTEXT_OWNER)
      end

      def eval_context_name
        attr_get_string(OCI_ATTR_EVAL_CONTEXT_NAME)
      end

      def comment
        attr_get_string(OCI_ATTR_COMMENT)
      end

      # def list_action_context
      #   __param(???)
      # end
    end

    class RuleSet < Base # :nodoc:
      register_ptype OCI_PTYPE_RULE_SET

      # Table 6-19 Attributes Specific to Rule Sets

      def eval_context_owner
        attr_get_string(OCI_ATTR_EVAL_CONTEXT_OWNER)
      end

      def eval_context_name
        attr_get_string(OCI_ATTR_EVAL_CONTEXT_NAME)
      end

      def comment
        attr_get_string(OCI_ATTR_COMMENT)
      end

      # def list_rules
      #   __param(???)
      # end
    end

    class EvaluationContext < Base # :nodoc:
      register_ptype OCI_PTYPE_EVALUATION_CONTEXT

      # Table 6-20 Attributes Specific to Evaluation Contexts

      def evaluation_function
        attr_get_string(OCI_ATTR_EVALUATION_FUNCTION)
      end

      def comment
        attr_get_string(OCI_ATTR_COMMENT)
      end

      def list_table_aliases
        __param(OCI_ATTR_LIST_TABLE_ALIASES)
      end

      def list_variable_types
        __param(OCI_ATTR_LIST_VARIABLE_TYPES)
      end
    end

    class TableAlias < Base # :nodoc:
      register_ptype OCI_PTYPE_TABLE_ALIAS

      # Table 6-21 Attributes Specific to Table Aliases

      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      def table_name
        attr_get_string(OCI_ATTR_TABLE_NAME)
      end
    end

    class VariableType < Base # :nodoc:
      register_ptype OCI_PTYPE_VARIABLE_TYPE

      # Table 6-22 Attributes Specific to Variable Types

      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      def var_type
        attr_get_string(OCI_ATTR_VAR_TYPE)
      end

      def var_value_function
        attr_get_string(OCI_ATTR_VAR_VALUE_FUNCTION)
      end

      def var_method_function
        attr_get_string(OCI_ATTR_VAR_METHOD_FUNCTION)
      end
    end

    class NameValue < Base # :nodoc:
      register_ptype OCI_PTYPE_NAME_VALUE

      # Table 6-23 Attributes Specific to Name Value Pair

      def name
        attr_get_string(OCI_ATTR_NAME)
      end

      # not implemented
      def value
        __anydata(OCI_ATTR_VALUE)
      end
    end
=end
  end # OCI8::Metadata

  # return a subclass of OCI8::Metadata::Base
  # which has information about _object_name_.
  # OCI8::Metadata::Table, OCI8::Metadata::View,
  # OCI8::Metadata::Procedure, OCI8::Metadata::Function,
  # OCI8::Metadata::Package, OCI8::Metadata::Type,
  # OCI8::Metadata::Synonym or OCI8::Metadata::Sequence
  def describe_any(object_name)
    __describe(object_name, OCI8::Metadata::Unknown, true)
  end
  # returns a OCI8::Metadata::Table or a OCI8::Metadata::View. If the
  # name is a current schema's synonym name or a public synonym name,
  # it returns a OCI8::Metadata::Table or a OCI8::Metadata::View which
  # the synonym refers.
  #
  # If the second argument is true, this returns a
  # OCI8::Metadata::Table in the current schema.
  def describe_table(table_name, table_only = false)
    if table_only
      # check my own tables only.
      __describe(table_name, OCI8::Metadata::Table, false)
    else
      # check tables, views, synonyms and public synonyms.
      metadata = __describe(table_name, OCI8::Metadata::Unknown, true)
      case metadata
      when OCI8::Metadata::Table, OCI8::Metadata::View
        metadata
      when OCI8::Metadata::Synonym
        describe_table(metadata.translated_name)
      else
        raise OCIError.new("ORA-04043: object #{table_name} does not exist", 4043)
      end
    end
  end
  # returns a OCI8::Metadata::View in the current schema.
  def describe_view(view_name)
    __describe(view_name, OCI8::Metadata::View, false)
  end
  # returns a OCI8::Metadata::Procedure in the current schema.
  def describe_procedure(procedure_name)
    __describe(procedure_name, OCI8::Metadata::Procedure, false)
  end
  # returns a OCI8::Metadata::Function in the current schema.
  def describe_function(function_name)
    __describe(function_name, OCI8::Metadata::Function, false)
  end
  # returns a OCI8::Metadata::Package in the current schema.
  def describe_package(package_name)
    __describe(package_name, OCI8::Metadata::Package, false)
  end
  # returns a OCI8::Metadata::Type in the current schema.
  def describe_type(type_name)
    __describe(type_name, OCI8::Metadata::Type, false)
  end
  # returns a OCI8::Metadata::Synonym in the current schema.
  def describe_synonym(synonym_name, check_public_also = true)
    __describe(synonym_name, OCI8::Metadata::Synonym, check_public_also)
  end
  # returns a OCI8::Metadata::Sequence in the current schema.
  def describe_sequence(sequence_name)
    __describe(sequence_name, OCI8::Metadata::Sequence, false)
  end
  # returns a OCI8::Metadata::Schema in the database.
  def describe_schema(schema_name)
    __describe(schema_name, OCI8::Metadata::Schema, false)
  end
  # returns a OCI8::Metadata::Database.
  def describe_database(database_name)
    __describe(database_name, OCI8::Metadata::Database, false)
  end
end # OCI8
