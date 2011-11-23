#--
# ocihandle.rb -- Constants in OCIHandle.
#
# Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>
#++

class OCIHandle

  #################################
  #
  # Attribute Types in oci.h
  #
  #################################

  # maximum size of the data
  OCI_ATTR_DATA_SIZE          =   1

  # the SQL type of the column/argument
  OCI_ATTR_DATA_TYPE          =   2
  # the name of the column/argument
  OCI_ATTR_NAME               =   4
  # precision if number type
  OCI_ATTR_PRECISION          =   5
  # scale if number type
  OCI_ATTR_SCALE              =   6
  # is it null ?
  OCI_ATTR_IS_NULL            =   7
  # name of the named data type or a package name
  OCI_ATTR_TYPE_NAME          =   8
  # the schema name
  OCI_ATTR_SCHEMA_NAME        =   9
  # type name if package private type
  OCI_ATTR_SUB_NAME           =  10
  # relative position
  OCI_ATTR_POSITION           =  11
  # packed decimal scale
  OCI_ATTR_PDSCL              =  16
  # fs prec for datetime data types
  OCI_ATTR_FSPRECISION        = OCI_ATTR_PDSCL
  # packed decimal format
  OCI_ATTR_PDPRC              =  17
  # fs prec for datetime data types
  OCI_ATTR_LFPRECISION        = OCI_ATTR_PDPRC
  # username attribute
  OCI_ATTR_USERNAME           =  22
  # password attribute
  OCI_ATTR_PASSWORD           =  23
  # Character Set ID
  OCI_ATTR_CHARSET_ID         =  31
  # Character Set Form
  OCI_ATTR_CHARSET_FORM       =  32
  # number of columns
  OCI_ATTR_NUM_COLS           = 102
  # parameter of the column list
  OCI_ATTR_LIST_COLUMNS       = 103
  # DBA of the segment header
  OCI_ATTR_RDBA               = 104
  # whether the table is clustered
  OCI_ATTR_CLUSTERED          = 105
  # whether the table is partitioned
  OCI_ATTR_PARTITIONED        = 106
  # whether the table is index only
  OCI_ATTR_INDEX_ONLY         = 107
  # parameter of the argument list
  OCI_ATTR_LIST_ARGUMENTS     = 108
  # parameter of the subprogram list
  OCI_ATTR_LIST_SUBPROGRAMS   = 109
  # REF to the type descriptor
  OCI_ATTR_REF_TDO            = 110
  # the database link name 
  OCI_ATTR_LINK               = 111
  # minimum value
  OCI_ATTR_MIN                = 112
  # maximum value
  OCI_ATTR_MAX                = 113
  # increment value
  OCI_ATTR_INCR               = 114
  # number of sequence numbers cached
  OCI_ATTR_CACHE              = 115
  # whether the sequence is ordered
  OCI_ATTR_ORDER              = 116
  # high-water mark
  OCI_ATTR_HW_MARK            = 117
  # type's schema name
  OCI_ATTR_TYPE_SCHEMA        = 118
  # timestamp of the object
  OCI_ATTR_TIMESTAMP          = 119
  # number of parameters
  OCI_ATTR_NUM_PARAMS         = 121
  # object id for a table or view
  OCI_ATTR_OBJID              = 122
  # overload ID for funcs and procs
  OCI_ATTR_OVERLOAD_ID        = 125
  # table name space
  OCI_ATTR_TABLESPACE         = 126
  # list type
  OCI_ATTR_LTYPE              = 128
  # whether table is temporary
  OCI_ATTR_IS_TEMPORARY       = 130
  # whether table is typed
  OCI_ATTR_IS_TYPED           = 131
  # duration of temporary table
  OCI_ATTR_DURATION           = 132
  # is invoker rights
  OCI_ATTR_IS_INVOKER_RIGHTS  = 133
  # top level schema obj name
  OCI_ATTR_OBJ_NAME           = 134
  # schema name
  OCI_ATTR_OBJ_SCHEMA         = 135
  # top level schema object id
  OCI_ATTR_OBJ_ID             = 136

  OCI_ATTR_CONN_NOWAIT        = 178
  OCI_ATTR_CONN_BUSY_COUNT    = 179
  OCI_ATTR_CONN_OPEN_COUNT    = 180
  OCI_ATTR_CONN_TIMEOUT       = 181
  OCI_ATTR_CONN_MIN           = 183
  OCI_ATTR_CONN_MAX           = 184
  OCI_ATTR_CONN_INCR          = 185

  # is this position overloaded
  OCI_ATTR_OVERLOAD           = 210
  # level for structured types
  OCI_ATTR_LEVEL              = 211
  # has a default value
  OCI_ATTR_HAS_DEFAULT        = 212
  # in, out inout
  OCI_ATTR_IOMODE             = 213
  # returns a radix
  OCI_ATTR_RADIX              = 214
  # total number of arguments
  OCI_ATTR_NUM_ARGS           = 215
  # object or collection
  OCI_ATTR_TYPECODE           = 216
  # varray or nested table
  OCI_ATTR_COLLECTION_TYPECODE = 217
  # user assigned version
  OCI_ATTR_VERSION            = 218
  # is this an incomplete type
  OCI_ATTR_IS_INCOMPLETE_TYPE = 219
  # a system type
  OCI_ATTR_IS_SYSTEM_TYPE     = 220
  # a predefined type
  OCI_ATTR_IS_PREDEFINED_TYPE = 221
  # a transient type
  OCI_ATTR_IS_TRANSIENT_TYPE  = 222
  # system generated type
  OCI_ATTR_IS_SYSTEM_GENERATED_TYPE = 223
  # contains nested table attr
  OCI_ATTR_HAS_NESTED_TABLE   = 224
  # has a lob attribute
  OCI_ATTR_HAS_LOB            = 225
  # has a file attribute
  OCI_ATTR_HAS_FILE           = 226
  # has a collection attribute
  OCI_ATTR_COLLECTION_ELEMENT = 227
  # number of attribute types
  OCI_ATTR_NUM_TYPE_ATTRS     = 228
  # list of type attributes
  OCI_ATTR_LIST_TYPE_ATTRS    = 229
  # number of type methods
  OCI_ATTR_NUM_TYPE_METHODS   = 230
  # list of type methods
  OCI_ATTR_LIST_TYPE_METHODS  = 231
  # map method of type
  OCI_ATTR_MAP_METHOD         = 232
  # order method of type 
  OCI_ATTR_ORDER_METHOD       = 233
  # number of elements
  OCI_ATTR_NUM_ELEMS          = 234
  # encapsulation level
  OCI_ATTR_ENCAPSULATION      = 235
  # method selfish
  OCI_ATTR_IS_SELFISH         = 236
  # virtual
  OCI_ATTR_IS_VIRTUAL         = 237
  # inline
  OCI_ATTR_IS_INLINE          = 238
  # constant
  OCI_ATTR_IS_CONSTANT        = 239
  # has result
  OCI_ATTR_HAS_RESULT         = 240
  # constructor
  OCI_ATTR_IS_CONSTRUCTOR     = 241
  # destructor
  OCI_ATTR_IS_DESTRUCTOR      = 242
  # operator
  OCI_ATTR_IS_OPERATOR        = 243
  # a map method
  OCI_ATTR_IS_MAP             = 244
  # order method
  OCI_ATTR_IS_ORDER           = 245
  # read no data state method
  OCI_ATTR_IS_RNDS            = 246
  # read no process state
  OCI_ATTR_IS_RNPS            = 247
  # write no data state method
  OCI_ATTR_IS_WNDS            = 248
  # write no process state
  OCI_ATTR_IS_WNPS            = 249
  OCI_ATTR_IS_SUBTYPE         = 258
  OCI_ATTR_SUPERTYPE_SCHEMA_NAME = 259
  OCI_ATTR_SUPERTYPE_NAME     = 260
  # list of objects in schema
  OCI_ATTR_LIST_OBJECTS       = 261
  # char set id
  OCI_ATTR_NCHARSET_ID        = 262
  # list of schemas
  OCI_ATTR_LIST_SCHEMAS       = 263
  # max procedure length
  OCI_ATTR_MAX_PROC_LEN       = 264
  # max column name length
  OCI_ATTR_MAX_COLUMN_LEN     = 265
  # cursor commit behavior
  OCI_ATTR_CURSOR_COMMIT_BEHAVIOR = 266
  # catalog namelength
  OCI_ATTR_MAX_CATALOG_NAMELEN = 267
  # catalog location
  OCI_ATTR_CATALOG_LOCATION   = 268
  # savepoint support
  OCI_ATTR_SAVEPOINT_SUPPORT  = 269
  # nowait support
  OCI_ATTR_NOWAIT_SUPPORT     = 270
  # autocommit DDL
  OCI_ATTR_AUTOCOMMIT_DDL     = 271
  # locking mode
  OCI_ATTR_LOCKING_MODE       = 272
  # value of client id to set
  OCI_ATTR_CLIENT_IDENTIFIER  = 278
  # is final type ?
  OCI_ATTR_IS_FINAL_TYPE      = 279
  # is instantiable type ?
  OCI_ATTR_IS_INSTANTIABLE_TYPE = 280
  # is final method ?
  OCI_ATTR_IS_FINAL_METHOD    = 281
  # is instantiable method ?
  OCI_ATTR_IS_INSTANTIABLE_METHOD = 282
  # is overriding method ?
  OCI_ATTR_IS_OVERRIDING_METHOD = 283
  # Describe the base object
  OCI_ATTR_DESC_SYNBASE       = 284
  # char length semantics
  OCI_ATTR_CHAR_USED          = 285
  # char length
  OCI_ATTR_CHAR_SIZE          = 286
  # rule condition
  OCI_ATTR_CONDITION          = 342
  # comment
  OCI_ATTR_COMMENT            = 343
  # Anydata value
  OCI_ATTR_VALUE              = 344
  # eval context owner
  OCI_ATTR_EVAL_CONTEXT_OWNER = 345
  # eval context name
  OCI_ATTR_EVAL_CONTEXT_NAME  = 346
  # eval function name
  OCI_ATTR_EVALUATION_FUNCTION = 347
  # variable type
  OCI_ATTR_VAR_TYPE           = 348
  # variable value function
  OCI_ATTR_VAR_VALUE_FUNCTION = 349
  # variable method function
  OCI_ATTR_VAR_METHOD_FUNCTION = 350
  # action context
  OCI_ATTR_ACTION_CONTEXT     = 351
  # list of table aliases
  OCI_ATTR_LIST_TABLE_ALIASES = 352
  # list of variable types
  OCI_ATTR_LIST_VARIABLE_TYPES = 353
  # table name
  OCI_ATTR_TABLE_NAME         = 356

  #################################
  #
  # Various Modes
  #
  #################################

  # the default value for parameters and attributes
  OCI_DEFAULT                 =   0

  #################################
  #
  # Credential Types
  #
  #################################

  # database username/password credentials
  OCI_CRED_RDBMS              =   1
  # externally provided credentials
  OCI_CRED_EXT                =   2

  #################################
  #
  # Authentication Modes
  #
  #################################

  # for SYSDBA authorization
  OCI_SYSDBA                  = 0x0002
  # for SYSOPER authorization
  OCI_SYSOPER                 = 0x0004
  # for SYSASM authorization
  OCI_SYSASM                  = 0x8000

  #################################
  #
  # Attach Modes
  #
  #################################

  # Attach using server handle from pool
  OCI_CPOOL                   = 0x0200

  #################################
  #
  # OCI Parameter Types
  #
  #################################

  # parameter type for unknown type
  OCI_PTYPE_UNK               =  0
  # parameter type for table
  OCI_PTYPE_TABLE             =  1
  # parameter type for view
  OCI_PTYPE_VIEW              =  2
  # parameter type for procedure
  OCI_PTYPE_PROC              =  3
  # parameter type for function
  OCI_PTYPE_FUNC              =  4
  # parameter type for package
  OCI_PTYPE_PKG               =  5
  # parameter type for user-defined type
  OCI_PTYPE_TYPE              =  6
  # parameter type for synonym
  OCI_PTYPE_SYN               =  7
  # parameter type for sequence
  OCI_PTYPE_SEQ               =  8
  # parameter type for column
  OCI_PTYPE_COL               =  9
  # parameter type for argument
  OCI_PTYPE_ARG               = 10
  # parameter type for list
  OCI_PTYPE_LIST              = 11
  # parameter type for user-defined type's attribute
  OCI_PTYPE_TYPE_ATTR         = 12
  # parameter type for collection type's element
  OCI_PTYPE_TYPE_COLL         = 13
  # parameter type for user-defined type's method
  OCI_PTYPE_TYPE_METHOD       = 14
  # parameter type for user-defined type method's arg
  OCI_PTYPE_TYPE_ARG          = 15
  # parameter type for user-defined type method's result
  OCI_PTYPE_TYPE_RESULT       = 16
  # parameter type for schema
  OCI_PTYPE_SCHEMA            = 17
  # parameter type for database
  OCI_PTYPE_DATABASE          = 18
  # parameter type for rule
  OCI_PTYPE_RULE              = 19
  # parameter type for rule set
  OCI_PTYPE_RULE_SET          = 20
  # parameter type for evaluation context
  OCI_PTYPE_EVALUATION_CONTEXT = 21
  # parameter type for table alias
  OCI_PTYPE_TABLE_ALIAS       = 22
  # parameter type for variable type
  OCI_PTYPE_VARIABLE_TYPE     = 23
  # parameter type for name value pair
  OCI_PTYPE_NAME_VALUE        = 24

  #################################
  #
  # OCI List Types
  #
  #################################

  # list type for unknown type
  OCI_LTYPE_UNK                =  0
  # list type for column list
  OCI_LTYPE_COLUMN             =  1
  # list type for procedure argument list
  OCI_LTYPE_ARG_PROC           =  2
  # list type for function argument list
  OCI_LTYPE_ARG_FUNC           =  3
  # list type for subprogram list
  OCI_LTYPE_SUBPRG             =  4
  # list type for type attribute
  OCI_LTYPE_TYPE_ATTR          =  5
  # list type for type method
  OCI_LTYPE_TYPE_METHOD        =  6
  # list type for type method w/o result argument list
  OCI_LTYPE_TYPE_ARG_PROC      =  7
  # list type for type method w/result argument list
  OCI_LTYPE_TYPE_ARG_FUNC      =  8
  # list type for schema object list
  OCI_LTYPE_SCH_OBJ            =  9
  # list type for database schema list
  OCI_LTYPE_DB_SCH             = 10
  # list type for subtype list
  OCI_LTYPE_TYPE_SUBTYPE       = 11
  # list type for table alias list
  OCI_LTYPE_TABLE_ALIAS        = 12
  # list type for variable type list
  OCI_LTYPE_VARIABLE_TYPE      = 13
  # list type for name value list
  OCI_LTYPE_NAME_VALUE         = 14

  #################################
  #
  # OBJECT Duration in oro.h
  #
  #################################

  #
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
end
