# Implement OCI8::Metadata.
# See {'Describing Schema Metadata' in Oracle Call Interface Programmer's Guide}
# [http://download-west.oracle.com/docs/cd/B19306_01/appdev.102/b14250/oci06des.htm]

#
class OCI8

  # :stopdoc:

  # Attribute Types in oci.h
  OCI_ATTR_DATA_SIZE          =   1 # maximum size of the data
  OCI_ATTR_DATA_TYPE          =   2 # the SQL type of the column/argument
  OCI_ATTR_NAME               =   4 # the name of the column/argument
  OCI_ATTR_PRECISION          =   5 # precision if number type
  OCI_ATTR_SCALE              =   6 # scale if number type
  OCI_ATTR_IS_NULL            =   7 # is it null ?
  OCI_ATTR_TYPE_NAME          =   8 # name of the named data type or a package name
  OCI_ATTR_SCHEMA_NAME        =   9 # the schema name
  OCI_ATTR_SUB_NAME           =  10 # type name if package private type
  OCI_ATTR_POSITION           =  11 # relative position
  OCI_ATTR_PDSCL              =  16 # packed decimal scale
  OCI_ATTR_FSPRECISION = OCI_ATTR_PDSCL # fs prec for datetime data types
  OCI_ATTR_PDPRC              =  17 # packed decimal format
  OCI_ATTR_LFPRECISION = OCI_ATTR_PDPRC # fs prec for datetime data types
  OCI_ATTR_CHARSET_ID         =  31 # Character Set ID
  OCI_ATTR_CHARSET_FORM       =  32 # Character Set Form
  OCI_ATTR_NUM_COLS           = 102 # number of columns
  OCI_ATTR_LIST_COLUMNS       = 103 # parameter of the column list
  OCI_ATTR_RDBA               = 104 # DBA of the segment header
  OCI_ATTR_CLUSTERED          = 105 # whether the table is clustered
  OCI_ATTR_PARTITIONED        = 106 # whether the table is partitioned
  OCI_ATTR_INDEX_ONLY         = 107 # whether the table is index only
  OCI_ATTR_LIST_ARGUMENTS     = 108 # parameter of the argument list
  OCI_ATTR_LIST_SUBPROGRAMS   = 109 # parameter of the subprogram list
  OCI_ATTR_REF_TDO            = 110 # REF to the type descriptor
  OCI_ATTR_LINK               = 111 # the database link name 
  OCI_ATTR_MIN                = 112 # minimum value
  OCI_ATTR_MAX                = 113 # maximum value
  OCI_ATTR_INCR               = 114 # increment value
  OCI_ATTR_CACHE              = 115 # number of sequence numbers cached
  OCI_ATTR_ORDER              = 116 # whether the sequence is ordered
  OCI_ATTR_HW_MARK            = 117 # high-water mark
  OCI_ATTR_TYPE_SCHEMA        = 118 # type's schema name
  OCI_ATTR_TIMESTAMP          = 119 # timestamp of the object
  OCI_ATTR_NUM_PARAMS         = 121 # number of parameters
  OCI_ATTR_OBJID              = 122 # object id for a table or view
  OCI_ATTR_OVERLOAD_ID        = 125 # overload ID for funcs and procs
  OCI_ATTR_TABLESPACE         = 126 # table name space
  OCI_ATTR_LTYPE              = 128 # list type
  OCI_ATTR_IS_TEMPORARY       = 130 # whether table is temporary
  OCI_ATTR_IS_TYPED           = 131 # whether table is typed
  OCI_ATTR_DURATION           = 132 # duration of temporary table
  OCI_ATTR_IS_INVOKER_RIGHTS  = 133 # is invoker rights
  OCI_ATTR_OBJ_NAME           = 134 # top level schema obj name
  OCI_ATTR_OBJ_SCHEMA         = 135 # schema name
  OCI_ATTR_OBJ_ID             = 136 # top level schema object id
  # OCI_ATTR_OVERLOAD           = 210 # is this position overloaded
  OCI_ATTR_LEVEL              = 211 # level for structured types
  OCI_ATTR_HAS_DEFAULT        = 212 # has a default value
  OCI_ATTR_IOMODE             = 213 # in, out inout
  OCI_ATTR_RADIX              = 214 # returns a radix
  # OCI_ATTR_NUM_ARGS           = 215 # total number of arguments
  OCI_ATTR_TYPECODE           = 216 # object or collection
  OCI_ATTR_COLLECTION_TYPECODE = 217 # varray or nested table
  OCI_ATTR_VERSION            = 218 # user assigned version
  OCI_ATTR_IS_INCOMPLETE_TYPE = 219 # is this an incomplete type
  OCI_ATTR_IS_SYSTEM_TYPE     = 220 # a system type
  OCI_ATTR_IS_PREDEFINED_TYPE = 221 # a predefined type
  OCI_ATTR_IS_TRANSIENT_TYPE  = 222 # a transient type
  OCI_ATTR_IS_SYSTEM_GENERATED_TYPE = 223 # system generated type
  OCI_ATTR_HAS_NESTED_TABLE   = 224 # contains nested table attr
  OCI_ATTR_HAS_LOB            = 225 # has a lob attribute
  OCI_ATTR_HAS_FILE           = 226 # has a file attribute
  OCI_ATTR_COLLECTION_ELEMENT = 227 # has a collection attribute
  OCI_ATTR_NUM_TYPE_ATTRS     = 228 # number of attribute types
  OCI_ATTR_LIST_TYPE_ATTRS    = 229 # list of type attributes
  OCI_ATTR_NUM_TYPE_METHODS   = 230 # number of type methods
  OCI_ATTR_LIST_TYPE_METHODS  = 231 # list of type methods
  OCI_ATTR_MAP_METHOD         = 232 # map method of type
  OCI_ATTR_ORDER_METHOD       = 233 # order method of type 
  OCI_ATTR_NUM_ELEMS          = 234 # number of elements
  OCI_ATTR_ENCAPSULATION      = 235 # encapsulation level
  OCI_ATTR_IS_SELFISH         = 236 # method selfish
  # OCI_ATTR_IS_VIRTUAL         = 237 # virtual
  # OCI_ATTR_IS_INLINE          = 238 # inline
  # OCI_ATTR_IS_CONSTANT        = 239 # constant
  OCI_ATTR_HAS_RESULT         = 240 # has result
  OCI_ATTR_IS_CONSTRUCTOR     = 241 # constructor
  OCI_ATTR_IS_DESTRUCTOR      = 242 # destructor
  # OCI_ATTR_IS_OPERATOR        = 243 # operator
  OCI_ATTR_IS_MAP             = 244 # a map method
  OCI_ATTR_IS_ORDER           = 245 # order method
  OCI_ATTR_IS_RNDS            = 246 # read no data state method
  OCI_ATTR_IS_RNPS            = 247 # read no process state
  OCI_ATTR_IS_WNDS            = 248 # write no data state method
  OCI_ATTR_IS_WNPS            = 249 # write no process state
  OCI_ATTR_IS_SUBTYPE         = 258
  OCI_ATTR_SUPERTYPE_SCHEMA_NAME = 259
  OCI_ATTR_SUPERTYPE_NAME     = 260
  OCI_ATTR_LIST_OBJECTS       = 261 # list of objects in schema
  OCI_ATTR_NCHARSET_ID        = 262 # char set id
  OCI_ATTR_LIST_SCHEMAS       = 263 # list of schemas
  OCI_ATTR_MAX_PROC_LEN       = 264 # max procedure length
  OCI_ATTR_MAX_COLUMN_LEN     = 265 # max column name length
  OCI_ATTR_CURSOR_COMMIT_BEHAVIOR = 266 # cursor commit behavior
  OCI_ATTR_MAX_CATALOG_NAMELEN = 267 # catalog namelength
  OCI_ATTR_CATALOG_LOCATION   = 268 # catalog location
  OCI_ATTR_SAVEPOINT_SUPPORT  = 269 # savepoint support
  OCI_ATTR_NOWAIT_SUPPORT     = 270 # nowait support
  OCI_ATTR_AUTOCOMMIT_DDL     = 271 # autocommit DDL
  OCI_ATTR_LOCKING_MODE       = 272 # locking mode
  # OCI_ATTR_CLIENT_IDENTIFIER  = 278 # value of client id to set
  OCI_ATTR_IS_FINAL_TYPE      = 279 # is final type ?
  OCI_ATTR_IS_INSTANTIABLE_TYPE = 280 # is instantiable type ?
  OCI_ATTR_IS_FINAL_METHOD    = 281 # is final method ?
  OCI_ATTR_IS_INSTANTIABLE_METHOD = 282 # is instantiable method ?
  OCI_ATTR_IS_OVERRIDING_METHOD = 283 # is overriding method ?
  # OCI_ATTR_DESC_SYNBASE       = 284 # Describe the base object
  OCI_ATTR_CHAR_USED          = 285 # char length semantics
  OCI_ATTR_CHAR_SIZE          = 286 # char length
  OCI_ATTR_CONDITION          = 342 # rule condition
  OCI_ATTR_COMMENT            = 343 # comment
  OCI_ATTR_VALUE              = 344 # Anydata value
  OCI_ATTR_EVAL_CONTEXT_OWNER = 345 # eval context owner
  OCI_ATTR_EVAL_CONTEXT_NAME  = 346 # eval context name
  OCI_ATTR_EVALUATION_FUNCTION = 347 # eval function name
  OCI_ATTR_VAR_TYPE           = 348 # variable type
  OCI_ATTR_VAR_VALUE_FUNCTION = 349 # variable value function
  OCI_ATTR_VAR_METHOD_FUNCTION = 350 # variable method function
  # OCI_ATTR_ACTION_CONTEXT     = 351 # action context
  OCI_ATTR_LIST_TABLE_ALIASES = 352 # list of table aliases
  OCI_ATTR_LIST_VARIABLE_TYPES = 353 # list of variable types
  OCI_ATTR_TABLE_NAME         = 356 # table name

  # OCI Parameter Types
  OCI_PTYPE_UNK                =  0 # unknown
  OCI_PTYPE_TABLE              =  1 # table
  OCI_PTYPE_VIEW               =  2 # view
  OCI_PTYPE_PROC               =  3 # procedure
  OCI_PTYPE_FUNC               =  4 # function
  OCI_PTYPE_PKG                =  5 # package
  OCI_PTYPE_TYPE               =  6 # user-defined type
  OCI_PTYPE_SYN                =  7 # synonym
  OCI_PTYPE_SEQ                =  8 # sequence
  OCI_PTYPE_COL                =  9 # column
  OCI_PTYPE_ARG                = 10 # argument
  OCI_PTYPE_LIST               = 11 # list
  OCI_PTYPE_TYPE_ATTR          = 12 # user-defined type's attribute
  OCI_PTYPE_TYPE_COLL          = 13 # collection type's element
  OCI_PTYPE_TYPE_METHOD        = 14 # user-defined type's method
  OCI_PTYPE_TYPE_ARG           = 15 # user-defined type method's arg
  OCI_PTYPE_TYPE_RESULT        = 16 # user-defined type method's result
  OCI_PTYPE_SCHEMA             = 17 # schema
  OCI_PTYPE_DATABASE           = 18 # database
  OCI_PTYPE_RULE               = 19 # rule
  OCI_PTYPE_RULE_SET           = 20 # rule set
  OCI_PTYPE_EVALUATION_CONTEXT = 21 # evaluation context
  OCI_PTYPE_TABLE_ALIAS        = 22 # table alias
  OCI_PTYPE_VARIABLE_TYPE      = 23 # variable type
  OCI_PTYPE_NAME_VALUE         = 24 # name value pair

  # OCI List Types
  OCI_LTYPE_UNK                =  0 # unknown
  OCI_LTYPE_COLUMN             =  1 # column list
  OCI_LTYPE_ARG_PROC           =  2 # procedure argument list
  OCI_LTYPE_ARG_FUNC           =  3 # function argument list
  OCI_LTYPE_SUBPRG             =  4 # subprogram list
  OCI_LTYPE_TYPE_ATTR          =  5 # type attribute
  OCI_LTYPE_TYPE_METHOD        =  6 # type method
  OCI_LTYPE_TYPE_ARG_PROC      =  7 # type method w/o result argument list
  OCI_LTYPE_TYPE_ARG_FUNC      =  8 # type method w/result argument list
  OCI_LTYPE_SCH_OBJ            =  9 # schema object list
  OCI_LTYPE_DB_SCH             = 10 # database schema list
  OCI_LTYPE_TYPE_SUBTYPE       = 11 # subtype list
  OCI_LTYPE_TABLE_ALIAS        = 12 # table alias list
  OCI_LTYPE_VARIABLE_TYPE      = 13 # variable type list
  OCI_LTYPE_NAME_VALUE         = 14 # name value list

  # OBJECT Duration in oro.h
  OCI_DURATION_INVALID       = 0xFFFF
  OCI_DURATION_BEGIN         = 10
  OCI_DURATION_NULL          = OCI_DURATION_BEGIN - 1
  OCI_DURATION_DEFAULT       = OCI_DURATION_BEGIN - 2
  OCI_DURATION_USER_CALLBACK = OCI_DURATION_BEGIN - 3
  OCI_DURATION_NEXT          = OCI_DURATION_BEGIN - 4
  OCI_DURATION_SESSION       = OCI_DURATION_BEGIN
  OCI_DURATION_TRANS         = OCI_DURATION_BEGIN + 1
  OCI_DURATION_CALL          = OCI_DURATION_BEGIN + 2
  OCI_DURATION_STATEMENT     = OCI_DURATION_BEGIN + 3
  OCI_DURATION_CALLOUT       = OCI_DURATION_BEGIN + 4

  # :startdoc:

  # = OCI8 can describe database object's metadata.
  # [user objects]
  #     OCI8#describe_any(object_name)
  # [table or view]
  #     OCI8#describe_table(table_name, table_only = false)
  # [view]
  #     OCI8#describe_view(view_name)
  # [procedure]
  #     OCI8#describe_procedure(procedure_name)
  # [function]
  #     OCI8#describe_function(function_name)
  # [package]
  #     OCI8#describe_package(package_name)
  # [type]
  #     OCI8#describe_type(type_name)
  # [synonym]
  #     OCI8#describe_synonym(synonym_name, check_public_also = false)
  # [sequence]
  #     OCI8#describe_sequence(sequence_name)
  # [schema]
  #     OCI8#describe_schema(schema_name)
  # [database]
  #     OCI8#describe_database(database_name)
  #
  # The name can be supplied as 'OBJECT_NAME' or 'SCHEMA_NAME.OBJECT_NAME'.
  # For example: 'emp', 'scott.emp'.
  #
  # = Retrieving Column Datatypes for a Table
  #
  #  conn = OCI8.new('ruby', 'oci8')
  #  table = conn.describe_table('EMPLOYEES')
  #  table.columns.each do |col|
  #    if col.char_used
  #      col_width = col.char_size
  #    else
  #      col_width = col.data_size
  #    end
  #  end
  module Metadata
    # Abstract super class for Metadata classes.
    class Base
      # Table 6-1 Attributes Belonging to All Parameters

      # don't use this. The number of parameters
      def num_params
        __ub2(OCI_ATTR_NUM_PARAMS)
      end
      private :num_params

      # object or schema ID
      def obj_id
        __ub4(OCI_ATTR_OBJ_ID)
      end

      # database name or object name in a schema
      def obj_name
        __text(OCI_ATTR_OBJ_NAME)
      end

      # schema name where the object is located
      def obj_schema
        __text(OCI_ATTR_OBJ_SCHEMA)
      end

      # The timestamp of the object
      def timestamp
        __oradate(OCI_ATTR_TIMESTAMP)
      end

      def inspect # :nodoc:
        "#<#{self.class.name}:(#{obj_id}) #{obj_schema}.#{obj_name}>"
      end
      private

      def __boolean(idx)
        __ub1(idx) == 0 ? false : true
      end
      alias __word __sb4
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
                              "#{p.schema_name}.#{p.type_name}"
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

      def __data_type
        return @data_type if defined? @data_type
        entry = DATA_TYPE_MAP[__ub2(OCI_ATTR_DATA_TYPE)]
        type = entry.nil? ? __ub2(OCI_ATTR_DATA_TYPE) : entry[0]
        type = type.call(self) if type.is_a? Proc
        @data_type = type
      end

      def __duration
        case __ub2(OCI_ATTR_DURATION)
        when OCI_DURATION_SESSION
          :session
        when OCI_DURATION_TRANS
          :transaction
        when OCI_DURATION_NULL
          nil
        end
      end

      def __charset_form
        case __ub1(OCI_ATTR_CHARSET_FORM)
        when 1; :implicit # for CHAR, VARCHAR2, CLOB w/o a specified set
        when 2; :nchar    # for NCHAR, NCHAR VARYING, NCLOB
        when 3; :explicit # for CHAR, etc, with "CHARACTER SET ..." syntax
        when 4; :flexible # for PL/SQL "flexible" parameters
        when 5; :lit_null # for typecheck of NULL and empty_clob() lits
        end
      end

      def __type_string
        entry = DATA_TYPE_MAP[__ub2(OCI_ATTR_DATA_TYPE)]
        type = entry.nil? ? "unknown(#{__ub2(OCI_ATTR_DATA_TYPE)})" : entry[1]
        type = type.call(self) if type.is_a? Proc
        if respond_to?(:nullable?) && !nullable?
          type + " NOT NULL"
        else
          type
        end
      end

      def __typecode(idx)
        case __ub2(idx)
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
        when 228; :otmfirst    # OCI_TYPECODE_OTMFIRST
        when 320; :otmlast     # OCI_TYPECODE_OTMLAST
        when 228; :sysfirst    # OCI_TYPECODE_SYSFIRST
        when 235; :syslast     # OCI_TYPECODE_SYSLAST
        when 266; :pls_integer # OCI_TYPECODE_PLS_INTEGER
        end
      end
    end

    # metadata for a object which cannot be classified into other
    # metadata classes.
    #
    # This is returned by:
    # * OCI8::Metadata::Schema#all_objects
    class Unknown < Base
      register_ptype OCI_PTYPE_UNK
    end

    # Metadata for a table.
    #
    # This is returned by:
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

      # number of columns
      def num_cols
        __ub2(OCI_ATTR_NUM_COLS)
      end

      # column list
      def list_columns
        __param(OCI_ATTR_LIST_COLUMNS)
      end
      private :list_columns

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # indicates the table is temporary.
      def is_temporary?
        __boolean(OCI_ATTR_IS_TEMPORARY)
      end

      # indicates the table is typed.
      def is_typed?
        __boolean(OCI_ATTR_IS_TYPED)
      end

      # Duration of a temporary table. Values can be <tt>:session</tt> or
      # <tt>:transaction</tt>. nil if not a temporary table.
      def duration
        __duration
      end

      ## Table 6-3 Attributes Specific to Tables

      # data block address of the segment header. (How to use this?)
      def dba
        __ub4(OCI_ATTR_RDBA)
      end

      # tablespace the table resides in. (How to use this?)
      def tablespace
        __word(OCI_ATTR_TABLESPACE)
      end

      # indicates the table is clustered.
      def clustered?
        __boolean(OCI_ATTR_CLUSTERED)
      end

      # indicates the table is partitioned.
      def partitioned?
        __boolean(OCI_ATTR_PARTITIONED)
      end

      # indicates the table is index-only.
      def index_only?
        __boolean(OCI_ATTR_INDEX_ONLY)
      end

      # array of Column objects in a table.
      def columns
        @columns ||= list_columns.to_a
      end
    end

    # Metadata for a view.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_table(name, true)
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

      # number of columns
      def num_cols
        __ub2(OCI_ATTR_NUM_COLS)
      end

      # column list
      def list_columns
        __param(OCI_ATTR_LIST_COLUMNS)
      end
      private :list_columns

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # indicates the table is temporary.
      def is_temporary?
        __boolean(OCI_ATTR_IS_TEMPORARY)
      end

      # indicates the table is typed.
      def is_typed?
        __boolean(OCI_ATTR_IS_TYPED)
      end

      # Duration of a temporary table. Values can be <tt>:session</tt> or
      # <tt>:transaction</tt>. nil if not a temporary table.
      def duration
        __duration
      end

      # array of Column objects in a table.
      def columns
        @columns ||= list_columns.to_a
      end
    end

    # Abstract super class of Procedure and Function.
    #
    #--
    # How can I know whether FUNCTION or PROCEDURE?
    #++
    class ProcBase < Base
      ## Table 6-4 Attribute Belonging to Procedures or Functions

      # Argument list
      def list_arguments
        __param(OCI_ATTR_LIST_ARGUMENTS)
      end
      private :list_arguments

      # indicates the procedure or function has invoker's rights
      def is_invoker_rights?
        __boolean(OCI_ATTR_IS_INVOKER_RIGHTS)
      end

      ## Table 6-5 Attributes Specific to Package Subprograms

      # name of the procedure or function.
      #
      # available only for a Package subprogram.
      def name
        __text(OCI_ATTR_NAME)
      end

      # overloading ID number (relevant in case the procedure or
      # function is part of a package and is overloaded). Values
      # returned may be different from direct query of a PL/SQL
      # function or procedure. (What this means?)
      #
      # available only for a Package subprogram.
      def overload_id
        __ub2(OCI_ATTR_OVERLOAD_ID)
      end

      # array of Argument objects.
      #
      # The first element is the return type in case of Function.
      def arguments
        @arguments ||= list_arguments.to_a
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name}>"
      end
    end

    # Metadata for a procedure.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_procedure(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    # * OCI8::Metadata::Package#subprograms
    #
    # See ProcBase's methods.
    class Procedure < ProcBase
      register_ptype OCI_PTYPE_PROC
    end

    # Metadata for a function.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_function(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    # * OCI8::Metadata::Package#subprograms
    #
    # See ProcBase's methods.
    class Function < ProcBase
      register_ptype OCI_PTYPE_FUNC
    end

    # Metadata for a package.
    #
    # This is returned by:
    # * OCI8#describe_any(name)
    # * OCI8#describe_package(name)
    # * OCI8::Metadata::Schema#all_objects
    # * OCI8::Metadata::Schema#objects
    class Package < Base
      register_ptype OCI_PTYPE_PKG

      ## Table 6-6 Attributes Belonging to Packages

      # subprogram list
      def list_subprograms
        __param(OCI_ATTR_LIST_SUBPROGRAMS)
      end
      private :list_subprograms

      # is the package invoker's rights?
      def is_invoker_rights?
        __boolean(OCI_ATTR_IS_INVOKER_RIGHTS)
      end

      # array of Procedure or Function objects.
      def subprograms
        @subprograms ||= list_subprograms.to_a
      end
    end

    # Metadata for a type.
    #
    # This is returned by:
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

      # typecode. :object or :named_collection
      def typecode
        __typecode(OCI_ATTR_TYPECODE)
      end

      # typecode of collection if type is collection. nil if otherwise.
      def collection_typecode
        __typecode(OCI_ATTR_COLLECTION_TYPECODE) if typecode == :named_collection
      end

      # indicates this is an incomplete type
      def is_incomplete_type?
        __boolean(OCI_ATTR_IS_INCOMPLETE_TYPE)
      end

      # indicates this is a system type
      def is_system_type?
        __boolean(OCI_ATTR_IS_SYSTEM_TYPE)
      end

      # indicates this is a predefined type
      def is_predefined_type?
        __boolean(OCI_ATTR_IS_PREDEFINED_TYPE)
      end

      # indicates this is a transient type
      def is_transient_type?
        __boolean(OCI_ATTR_IS_TRANSIENT_TYPE)
      end

      # indicates this is a system-generated type
      def is_system_generated_type?
        __boolean(OCI_ATTR_IS_SYSTEM_GENERATED_TYPE)
      end

      # indicates this type contains a nested table attribute.
      def has_nested_table?
        __boolean(OCI_ATTR_HAS_NESTED_TABLE)
      end

      # indicates this type contains a LOB attribute
      def has_lob?
        __boolean(OCI_ATTR_HAS_LOB)
      end

      # indicates this type contains a BFILE attribute
      def has_file?
        __boolean(OCI_ATTR_HAS_FILE)
      end

      # returns OCI8::Metadata::Collection if type is collection. nil if otherwise.
      def collection_element
        __param(OCI_ATTR_COLLECTION_ELEMENT) if typecode == :named_collection
      end

      # number of type attributes
      def num_type_attrs
        __ub2(OCI_ATTR_NUM_TYPE_ATTRS)
      end

      # list of type attributes
      def list_type_attrs
        __param(OCI_ATTR_LIST_TYPE_ATTRS)
      end
      private :list_type_attrs

      # number of type methods
      def num_type_methods
        __ub2(OCI_ATTR_NUM_TYPE_METHODS)
      end

      # list of type methods
      def list_type_methods
        __param(OCI_ATTR_LIST_TYPE_METHODS)
      end
      private :list_type_methods

      # map method of type
      def map_method
        __param(OCI_ATTR_MAP_METHOD)
      end

      # order method of type
      def order_method
        __param(OCI_ATTR_ORDER_METHOD)
      end

      # indicates the type has invoker's rights
      def is_invoker_rights?
        __boolean(OCI_ATTR_IS_INVOKER_RIGHTS)
      end

      # type name
      def name
        __text(OCI_ATTR_NAME)
      end

      # schema name where the type has been created
      def schema_name
        __text(OCI_ATTR_SCHEMA_NAME)
      end

      # indicates this is a final type
      def is_final_type?
        __boolean(OCI_ATTR_IS_FINAL_TYPE)
      end

      # indicates this is an instantiable type
      def is_instantiable_type?
        __boolean(OCI_ATTR_IS_INSTANTIABLE_TYPE)
      end

      # indicates this is a subtype
      def is_subtype?
        __boolean(OCI_ATTR_IS_SUBTYPE)
      end

      # supertype's schema name
      def supertype_schema_name
        __text(OCI_ATTR_SUPERTYPE_SCHEMA_NAME) if is_subtype?
      end

      # supertype's name
      def supertype_name
        __text(OCI_ATTR_SUPERTYPE_NAME) if is_subtype?
      end

      # array of TypeAttr objects.
      def type_attrs
        @type_attrs ||= list_type_attrs.to_a
      end

      # array of TypeMethod objects.
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
        __ub2_nc(OCI_ATTR_DATA_SIZE)
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
        __text(OCI_ATTR_NAME)
      end

      # The precision of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? __sb2(OCI_ATTR_PRECISION) : __ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        __sb1(OCI_ATTR_SCALE)
      end

      # A string which is the type name. The returned value will
      # contain the type name if the datatype is <tt>:named_type</tt>
      # or <tt>:ref</tt>. If the datatype is <tt>:named_type</tt>, the
      # name of the named datatype's type is returned. If the datatype
      # is <tt>:ref</tt>, the type name of the named datatype pointed
      # to by the REF is returned.
      def type_name
        __text(OCI_ATTR_TYPE_NAME)
      end

      # schema name where the type has been created.
      def schema_name
        __text(OCI_ATTR_SCHEMA_NAME)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # character set id if the type attribute is of a string/character type.
      def charset_id
        __ub2(OCI_ATTR_CHARSET_ID)
      end

      # character set form, if the type attribute is of a string/character type.
      def charset_form
        __charset_form
      end

      if OCI8.oracle_client_version >= ORAVER_9_1
        # The fractional seconds precision of a datetime or interval.
        #
        # (unavailable on Oracle 8.1 or lower)
        def fsprecision
          __ub1(OCI_ATTR_FSPRECISION)
        end

        # The leading field precision of an interval
        #
        # (unavailable on Oracle 8.1 or lower)
        def lfprecision
          __ub1(OCI_ATTR_LFPRECISION)
        end
      end

      # character set name if the type attribute is of a string/character type.
      def charset_name
        __charset_name(charset_id)
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name} #{__type_string}>"
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
        __text(OCI_ATTR_NAME)
      end

      # encapsulation level of the method. Values are <tt>:public</tt>
      # or <tt>:private</tt>.
      def encapsulation
        case __ub4(OCI_ATTR_ENCAPSULATION)
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
        __ub2_nc(OCI_ATTR_DATA_SIZE)
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
        __ub4(OCI_ATTR_NUM_ELEMS)
      end

      # The precision of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? __sb2(OCI_ATTR_PRECISION) : __ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric type attributes. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        __sb1(OCI_ATTR_SCALE)
      end

      # A string which is the type name. The returned value will
      # contain the type name if the datatype is <tt>:named_type</tt> or <tt>:ref</tt>.
      # If the datatype is <tt>:named_type</tt>, the name of the named datatype's
      # type is returned. If the datatype is <tt>:ref</tt>, the type name
      # of the named datatype pointed to by the REF is returned.
      def type_name
        __text(OCI_ATTR_TYPE_NAME)
      end

      # schema name where the type has been created.
      def schema_name
        __text(OCI_ATTR_SCHEMA_NAME)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # character set id if the type attribute is of a string/character type.
      def charset_id
        __ub2(OCI_ATTR_CHARSET_ID)
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
        "#<#{self.class.name}: #{__type_string}>"
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
        @objid ||= __ub4(OCI_ATTR_OBJID)
      end

      # schema name of the synonym translation
      def schema_name
        @schema_name ||= __text(OCI_ATTR_SCHEMA_NAME)
      end

      # object name of the synonym translation
      def name
        @name ||= __text(OCI_ATTR_NAME)
      end

      # database link name of the synonym translation or nil
      def link
        @link ||= __text(OCI_ATTR_LINK)
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
        __ub4(OCI_ATTR_OBJID)
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

      if OCI8.oracle_client_version >= ORAVER_9_1
        # returns the type of length semantics of the column.
        # [<tt>:byte</tt>]  byte-length semantics
        # [<tt>:char</tt>]  character-length semantics.
        #
        # (unavailable on Oracle 8.1 or lower)
        def char_used?
          __ub4(OCI_ATTR_CHAR_USED) != 0
        end

        # returns the column character length which is the number of
        # characters allowed in the column. It is the counterpart of
        # OCI8::Metadata::Column#data_size which gets the byte length.
        #
        # (unavailable on Oracle 8.1 or lower)
        def char_size
          __ub2(OCI_ATTR_CHAR_SIZE)
        end
      else
        def char_used?
          false
        end
      end

      # The maximum size of the column. This length is
      # returned in bytes and not characters for strings and raws. It
      # returns 22 for NUMBERs.
      def data_size
        __ub2_nc(OCI_ATTR_DATA_SIZE)
      end

      # the datatype of the column.
      def data_type
        __data_type
      end

      # column name
      def name
        __text(OCI_ATTR_NAME)
      end

      # The precision of numeric columns. If the precision is nonzero
      # and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? __sb2(OCI_ATTR_PRECISION) : __ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric columns. If the precision is nonzero and
      # scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        __sb1(OCI_ATTR_SCALE)
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
        rv = __text(OCI_ATTR_TYPE_NAME)
        rv.length == 0 ? nil : rv
      end

      # Returns a string with the schema name under which the type has been created
      def schema_name
        rv = __text(OCI_ATTR_SCHEMA_NAME)
        rv.length == 0 ? nil : rv
      end

      # to type metadata if possible
      def type_metadata
        case __ub2(OCI_ATTR_DATA_TYPE)
        when 108, 110 # named_type or ref
          __type_metadata(OCI8::Metadata::Type)
        else
          nil
        end
      end

      # The character set id, if the column is of a string/character type
      def charset_id
        __ub2(OCI_ATTR_CHARSET_ID)
      end

      # The character set form, if the column is of a string/character type
      def charset_form
        __charset_form
      end

      ## Table 6-8 Attributes Belonging to Type Attributes
      ## But Column also have these.

      if OCI8.oracle_client_version >= ORAVER_9_1
        # The fractional seconds precision of a datetime or interval.
        #
        # (unavailable on Oracle 8.1 or lower)
        def fsprecision
          __ub1(OCI_ATTR_FSPRECISION)
        end

        # The leading field precision of an interval
        #
        # (unavailable on Oracle 8.1 or lower)
        def lfprecision
          __ub1(OCI_ATTR_LFPRECISION)
        end
      end

      # The character set name, if the column is of a string/character type
      def charset_name
        __charset_name(charset_id)
      end

      def type_string
        __type_string
      end

      def to_s
        %Q{"#{name}" #{__type_string}}
      end

      def inspect # :nodoc:
        "#<#{self.class.name}: #{name} #{__type_string}>"
      end
    end

    # Abstract super class of Argument, TypeArgument and TypeResult.
    class ArgBase < Base
      ## Table 6-14 Attributes Belonging to Arguments/Results

      # the argument name
      def name
        __text(OCI_ATTR_NAME)
      end

      # the position of the argument in the argument list.
      def position
        __ub2(OCI_ATTR_POSITION)
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
        __ub2_nc(OCI_ATTR_DATA_SIZE)
      end

      # The precision of numeric arguments. If the precision is
      # nonzero and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def precision
        __is_implicit? ? __sb2(OCI_ATTR_PRECISION) : __ub1(OCI_ATTR_PRECISION)
      end

      # The scale of numeric arguments. If the precision is nonzero
      # and scale is -127, then it is a FLOAT, else it is a
      # NUMBER(precision, scale). For the case when precision is 0,
      # NUMBER(precision, scale) can be represented simply as NUMBER.
      def scale
        __sb1(OCI_ATTR_SCALE)
      end

      # The datatype levels. This attribute always returns zero.
      def level
        __ub2(OCI_ATTR_LEVEL)
      end

      # Indicates whether an argument has a default
      def has_default
        __ub1(OCI_ATTR_HAS_DEFAULT)
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
        case __ub4(OCI_ATTR_IOMODE)
        when 0; :in
        when 1; :out
        when 2; :inout
        end
      end

      # Returns a radix (if number type)
      def radix
        __ub1(OCI_ATTR_RADIX)
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
        __text(OCI_ATTR_TYPE_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the schema name
      # under which the type was created, or under which the package
      # was created in the case of package local types
      def schema_name
        __text(OCI_ATTR_SCHEMA_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the type name,
      # in the case of package local types
      def sub_name
        __text(OCI_ATTR_SUB_NAME)
      end

      # For <tt>:named_type</tt> or <tt>:ref</tt>, returns a string with the database
      # link name of the database on which the type exists. This can
      # happen only in the case of package local types, when the
      # package is remote.
      def link
        __text(OCI_ATTR_LINK)
      end

      # to type metadata if possible
      def type_metadata
        __type_metadata(OCI8::Metadata::Type)
      end

      # Returns the character set ID if the argument is of a
      # string/character type
      def charset_id
        __ub2(OCI_ATTR_CHARSET_ID)
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
        "#<#{self.class.name}: #{name} #{__type_string}>"
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
          __ub2(OCI_ATTR_LTYPE)
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
        unless @all_objects
          begin
            objs = list_objects
          rescue OCIError => exc
            if exc.code != -1
              raise
            end
            # describe again.
            objs = __con.describe_schema(obj_schema).list_objects
          end
          @all_objects = objs.to_a
        end
        @all_objects
      end

      # array of objects in the schema.
      def objects
        unless @objects
          @objects = all_objects.dup.reject! do |obj|
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
        @objects
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
        __text(OCI_ATTR_VERSION)
      end

      # database character set Id
      def charset_id
        __ub2(OCI_ATTR_CHARSET_ID)
      end

      # database national language support character set Id
      def ncharset_id
        __ub2(OCI_ATTR_NCHARSET_ID)
      end

      # List of schemas in the database
      def list_schemas
        __param(OCI_ATTR_LIST_SCHEMAS)
      end
      private :list_schemas

      # Maximum length of a procedure name
      def max_proc_len
        __ub4(OCI_ATTR_MAX_PROC_LEN)
      end

      # Maximum length of a column name
      def max_column_len
        __ub4(OCI_ATTR_MAX_COLUMN_LEN)
      end

      # How a COMMIT operation affects cursors and prepared statements in
      # the database. Values are:
      # [<tt>:cusror_open</tt>]   preserve cursor state as before the commit
      #                           operation
      # [<tt>:cursor_closed</tt>] cursors are closed on COMMIT, but the
      #                           application can still re-execute the
      #                           statement without re-preparing it
      def cursor_commit_behavior
        case __ub1(OCI_ATTR_CURSOR_COMMIT_BEHAVIOR)
        when 0; :cusror_open
        when 1; :cursor_closed
        end
      end

      # Maximum length of a catalog (database) name
      def max_catalog_namelen
        __ub1(OCI_ATTR_MAX_CATALOG_NAMELEN)
      end

      # Position of the catalog in a qualified table. Values are
      # <tt>:cl_start</tt> and <tt>:cl_end</tt>
      def catalog_location
        case __ub1(OCI_ATTR_CATALOG_LOCATION)
        when 0; :cl_start
        when 1; :cl_end
        end
      end

      # Does database support savepoints? Values are
      # <tt>:sp_supported</tt> and <tt>:sp_unsupported</tt>
      def savepoint_support
        case __ub1(OCI_ATTR_SAVEPOINT_SUPPORT)
        when 0; :sp_supported
        when 1; :sp_unsupported
        end
      end

      # Does database support the nowait clause? Values are
      # <tt>:nw_supported</tt> and <tt>:nw_unsupported</tt>
      def nowait_support
        case __ub1(OCI_ATTR_NOWAIT_SUPPORT)
        when 0; :nw_supported
        when 1; :nw_unsupported
        end
      end

      # Is autocommit mode required for DDL statements? Values are
      # <tt>:ac_ddl</tt> and <tt>:no_ac_ddl</tt>
      def autocommit_ddl
        case __ub1(OCI_ATTR_AUTOCOMMIT_DDL)
        when 0; :ac_ddl
        when 1; :no_ac_ddl
        end
      end

      # Locking mode for the database. Values are <tt>:lock_immediate</tt> and
      # <tt>:lock_delayed</tt>
      def locking_mode
        case __ub1(OCI_ATTR_LOCKING_MODE)
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
        __text(OCI_ATTR_CONDITION)
      end

      def eval_context_owner
        __text(OCI_ATTR_EVAL_CONTEXT_OWNER)
      end

      def eval_context_name
        __text(OCI_ATTR_EVAL_CONTEXT_NAME)
      end

      def comment
        __text(OCI_ATTR_COMMENT)
      end

      # def list_action_context
      #   __param(???)
      # end
    end

    class RuleSet < Base # :nodoc:
      register_ptype OCI_PTYPE_RULE_SET

      # Table 6-19 Attributes Specific to Rule Sets

      def eval_context_owner
        __text(OCI_ATTR_EVAL_CONTEXT_OWNER)
      end

      def eval_context_name
        __text(OCI_ATTR_EVAL_CONTEXT_NAME)
      end

      def comment
        __text(OCI_ATTR_COMMENT)
      end

      # def list_rules
      #   __param(???)
      # end
    end

    class EvaluationContext < Base # :nodoc:
      register_ptype OCI_PTYPE_EVALUATION_CONTEXT

      # Table 6-20 Attributes Specific to Evaluation Contexts

      def evaluation_function
        __text(OCI_ATTR_EVALUATION_FUNCTION)
      end

      def comment
        __text(OCI_ATTR_COMMENT)
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
        __text(OCI_ATTR_NAME)
      end

      def table_name
        __text(OCI_ATTR_TABLE_NAME)
      end
    end

    class VariableType < Base # :nodoc:
      register_ptype OCI_PTYPE_VARIABLE_TYPE

      # Table 6-22 Attributes Specific to Variable Types

      def name
        __text(OCI_ATTR_NAME)
      end

      def var_type
        __text(OCI_ATTR_VAR_TYPE)
      end

      def var_value_function
        __text(OCI_ATTR_VAR_VALUE_FUNCTION)
      end

      def var_method_function
        __text(OCI_ATTR_VAR_METHOD_FUNCTION)
      end
    end

    class NameValue < Base # :nodoc:
      register_ptype OCI_PTYPE_NAME_VALUE

      # Table 6-23 Attributes Specific to Name Value Pair

      def name
        __text(OCI_ATTR_NAME)
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
