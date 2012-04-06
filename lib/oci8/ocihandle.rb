# ocihandle.rb -- Constants in OCIHandle.
#
# Copyright (C) 2010-2012 KUBO Takehiro <kubo@jiubao.org>

# The abstract base class to implement common behavior of OCI handle data types and OCI descriptor data types.
#
class OCIHandle

  #################################
  #
  # Attribute Types in oci.h
  #
  #################################

  # maximum size of the data
  # @private
  OCI_ATTR_DATA_SIZE          =   1

  # the SQL type of the column/argument
  # @private
  OCI_ATTR_DATA_TYPE          =   2
  # the name of the column/argument
  # @private
  OCI_ATTR_NAME               =   4
  # precision if number type
  # @private
  OCI_ATTR_PRECISION          =   5
  # scale if number type
  # @private
  OCI_ATTR_SCALE              =   6
  # is it null ?
  # @private
  OCI_ATTR_IS_NULL            =   7
  # name of the named data type or a package name
  # @private
  OCI_ATTR_TYPE_NAME          =   8
  # the schema name
  # @private
  OCI_ATTR_SCHEMA_NAME        =   9
  # type name if package private type
  # @private
  OCI_ATTR_SUB_NAME           =  10
  # relative position
  # @private
  OCI_ATTR_POSITION           =  11
  # packed decimal scale
  # @private
  OCI_ATTR_PDSCL              =  16
  # fs prec for datetime data types
  # @private
  OCI_ATTR_FSPRECISION        = OCI_ATTR_PDSCL
  # packed decimal format
  # @private
  OCI_ATTR_PDPRC              =  17
  # fs prec for datetime data types
  # @private
  OCI_ATTR_LFPRECISION        = OCI_ATTR_PDPRC
  # username attribute
  # @private
  OCI_ATTR_USERNAME           =  22
  # password attribute
  # @private
  OCI_ATTR_PASSWORD           =  23
  # Character Set ID
  # @private
  OCI_ATTR_CHARSET_ID         =  31
  # Character Set Form
  # @private
  OCI_ATTR_CHARSET_FORM       =  32
  # number of columns
  # @private
  OCI_ATTR_NUM_COLS           = 102
  # parameter of the column list
  # @private
  OCI_ATTR_LIST_COLUMNS       = 103
  # DBA of the segment header
  # @private
  OCI_ATTR_RDBA               = 104
  # whether the table is clustered
  # @private
  OCI_ATTR_CLUSTERED          = 105
  # whether the table is partitioned
  # @private
  OCI_ATTR_PARTITIONED        = 106
  # whether the table is index only
  # @private
  OCI_ATTR_INDEX_ONLY         = 107
  # parameter of the argument list
  # @private
  OCI_ATTR_LIST_ARGUMENTS     = 108
  # parameter of the subprogram list
  # @private
  OCI_ATTR_LIST_SUBPROGRAMS   = 109
  # REF to the type descriptor
  # @private
  OCI_ATTR_REF_TDO            = 110
  # the database link name 
  # @private
  OCI_ATTR_LINK               = 111
  # minimum value
  # @private
  OCI_ATTR_MIN                = 112
  # maximum value
  # @private
  OCI_ATTR_MAX                = 113
  # increment value
  # @private
  OCI_ATTR_INCR               = 114
  # number of sequence numbers cached
  # @private
  OCI_ATTR_CACHE              = 115
  # whether the sequence is ordered
  # @private
  OCI_ATTR_ORDER              = 116
  # high-water mark
  # @private
  OCI_ATTR_HW_MARK            = 117
  # type's schema name
  # @private
  OCI_ATTR_TYPE_SCHEMA        = 118
  # timestamp of the object
  # @private
  OCI_ATTR_TIMESTAMP          = 119
  # number of parameters
  # @private
  OCI_ATTR_NUM_PARAMS         = 121
  # object id for a table or view
  # @private
  OCI_ATTR_OBJID              = 122
  # overload ID for funcs and procs
  # @private
  OCI_ATTR_OVERLOAD_ID        = 125
  # table name space
  # @private
  OCI_ATTR_TABLESPACE         = 126
  # list type
  # @private
  OCI_ATTR_LTYPE              = 128
  # whether table is temporary
  # @private
  OCI_ATTR_IS_TEMPORARY       = 130
  # whether table is typed
  # @private
  OCI_ATTR_IS_TYPED           = 131
  # duration of temporary table
  # @private
  OCI_ATTR_DURATION           = 132
  # is invoker rights
  # @private
  OCI_ATTR_IS_INVOKER_RIGHTS  = 133
  # top level schema obj name
  # @private
  OCI_ATTR_OBJ_NAME           = 134
  # schema name
  # @private
  OCI_ATTR_OBJ_SCHEMA         = 135
  # top level schema object id
  # @private
  OCI_ATTR_OBJ_ID             = 136

  # @private
  OCI_ATTR_CONN_NOWAIT        = 178
  # @private
  OCI_ATTR_CONN_BUSY_COUNT    = 179
  # @private
  OCI_ATTR_CONN_OPEN_COUNT    = 180
  # @private
  OCI_ATTR_CONN_TIMEOUT       = 181
  # @private
  OCI_ATTR_CONN_MIN           = 183
  # @private
  OCI_ATTR_CONN_MAX           = 184
  # @private
  OCI_ATTR_CONN_INCR          = 185

  # is this position overloaded
  # @private
  OCI_ATTR_OVERLOAD           = 210
  # level for structured types
  # @private
  OCI_ATTR_LEVEL              = 211
  # has a default value
  # @private
  OCI_ATTR_HAS_DEFAULT        = 212
  # in, out inout
  # @private
  OCI_ATTR_IOMODE             = 213
  # returns a radix
  # @private
  OCI_ATTR_RADIX              = 214
  # total number of arguments
  # @private
  OCI_ATTR_NUM_ARGS           = 215
  # object or collection
  # @private
  OCI_ATTR_TYPECODE           = 216
  # varray or nested table
  # @private
  OCI_ATTR_COLLECTION_TYPECODE = 217
  # user assigned version
  # @private
  OCI_ATTR_VERSION            = 218
  # is this an incomplete type
  # @private
  OCI_ATTR_IS_INCOMPLETE_TYPE = 219
  # a system type
  # @private
  OCI_ATTR_IS_SYSTEM_TYPE     = 220
  # a predefined type
  # @private
  OCI_ATTR_IS_PREDEFINED_TYPE = 221
  # a transient type
  # @private
  OCI_ATTR_IS_TRANSIENT_TYPE  = 222
  # system generated type
  # @private
  OCI_ATTR_IS_SYSTEM_GENERATED_TYPE = 223
  # contains nested table attr
  # @private
  OCI_ATTR_HAS_NESTED_TABLE   = 224
  # has a lob attribute
  # @private
  OCI_ATTR_HAS_LOB            = 225
  # has a file attribute
  # @private
  OCI_ATTR_HAS_FILE           = 226
  # has a collection attribute
  # @private
  OCI_ATTR_COLLECTION_ELEMENT = 227
  # number of attribute types
  # @private
  OCI_ATTR_NUM_TYPE_ATTRS     = 228
  # list of type attributes
  # @private
  OCI_ATTR_LIST_TYPE_ATTRS    = 229
  # number of type methods
  # @private
  OCI_ATTR_NUM_TYPE_METHODS   = 230
  # list of type methods
  # @private
  OCI_ATTR_LIST_TYPE_METHODS  = 231
  # map method of type
  # @private
  OCI_ATTR_MAP_METHOD         = 232
  # order method of type 
  # @private
  OCI_ATTR_ORDER_METHOD       = 233
  # number of elements
  # @private
  OCI_ATTR_NUM_ELEMS          = 234
  # encapsulation level
  # @private
  OCI_ATTR_ENCAPSULATION      = 235
  # method selfish
  # @private
  OCI_ATTR_IS_SELFISH         = 236
  # virtual
  # @private
  OCI_ATTR_IS_VIRTUAL         = 237
  # inline
  # @private
  OCI_ATTR_IS_INLINE          = 238
  # constant
  # @private
  OCI_ATTR_IS_CONSTANT        = 239
  # has result
  # @private
  OCI_ATTR_HAS_RESULT         = 240
  # constructor
  # @private
  OCI_ATTR_IS_CONSTRUCTOR     = 241
  # destructor
  # @private
  OCI_ATTR_IS_DESTRUCTOR      = 242
  # operator
  # @private
  OCI_ATTR_IS_OPERATOR        = 243
  # a map method
  # @private
  OCI_ATTR_IS_MAP             = 244
  # order method
  # @private
  OCI_ATTR_IS_ORDER           = 245
  # read no data state method
  # @private
  OCI_ATTR_IS_RNDS            = 246
  # read no process state
  # @private
  OCI_ATTR_IS_RNPS            = 247
  # write no data state method
  # @private
  OCI_ATTR_IS_WNDS            = 248
  # write no process state
  # @private
  OCI_ATTR_IS_WNPS            = 249
  # @private
  OCI_ATTR_IS_SUBTYPE         = 258
  # @private
  OCI_ATTR_SUPERTYPE_SCHEMA_NAME = 259
  # @private
  OCI_ATTR_SUPERTYPE_NAME     = 260
  # list of objects in schema
  # @private
  OCI_ATTR_LIST_OBJECTS       = 261
  # char set id
  # @private
  OCI_ATTR_NCHARSET_ID        = 262
  # list of schemas
  # @private
  OCI_ATTR_LIST_SCHEMAS       = 263
  # max procedure length
  # @private
  OCI_ATTR_MAX_PROC_LEN       = 264
  # max column name length
  # @private
  OCI_ATTR_MAX_COLUMN_LEN     = 265
  # cursor commit behavior
  # @private
  OCI_ATTR_CURSOR_COMMIT_BEHAVIOR = 266
  # catalog namelength
  # @private
  OCI_ATTR_MAX_CATALOG_NAMELEN = 267
  # catalog location
  # @private
  OCI_ATTR_CATALOG_LOCATION   = 268
  # savepoint support
  # @private
  OCI_ATTR_SAVEPOINT_SUPPORT  = 269
  # nowait support
  # @private
  OCI_ATTR_NOWAIT_SUPPORT     = 270
  # autocommit DDL
  # @private
  OCI_ATTR_AUTOCOMMIT_DDL     = 271
  # locking mode
  # @private
  OCI_ATTR_LOCKING_MODE       = 272
  # value of client id to set
  # @private
  OCI_ATTR_CLIENT_IDENTIFIER  = 278
  # is final type ?
  # @private
  OCI_ATTR_IS_FINAL_TYPE      = 279
  # is instantiable type ?
  # @private
  OCI_ATTR_IS_INSTANTIABLE_TYPE = 280
  # is final method ?
  # @private
  OCI_ATTR_IS_FINAL_METHOD    = 281
  # is instantiable method ?
  # @private
  OCI_ATTR_IS_INSTANTIABLE_METHOD = 282
  # is overriding method ?
  # @private
  OCI_ATTR_IS_OVERRIDING_METHOD = 283
  # Describe the base object
  # @private
  OCI_ATTR_DESC_SYNBASE       = 284
  # char length semantics
  # @private
  OCI_ATTR_CHAR_USED          = 285
  # char length
  # @private
  OCI_ATTR_CHAR_SIZE          = 286
  # rule condition
  # @private
  OCI_ATTR_CONDITION          = 342
  # comment
  # @private
  OCI_ATTR_COMMENT            = 343
  # Anydata value
  # @private
  OCI_ATTR_VALUE              = 344
  # eval context owner
  # @private
  OCI_ATTR_EVAL_CONTEXT_OWNER = 345
  # eval context name
  # @private
  OCI_ATTR_EVAL_CONTEXT_NAME  = 346
  # eval function name
  # @private
  OCI_ATTR_EVALUATION_FUNCTION = 347
  # variable type
  # @private
  OCI_ATTR_VAR_TYPE           = 348
  # variable value function
  # @private
  OCI_ATTR_VAR_VALUE_FUNCTION = 349
  # variable method function
  # @private
  OCI_ATTR_VAR_METHOD_FUNCTION = 350
  # action context
  # @private
  OCI_ATTR_ACTION_CONTEXT     = 351
  # list of table aliases
  # @private
  OCI_ATTR_LIST_TABLE_ALIASES = 352
  # list of variable types
  # @private
  OCI_ATTR_LIST_VARIABLE_TYPES = 353
  # table name
  # @private
  OCI_ATTR_TABLE_NAME         = 356

  #################################
  #
  # Various Modes
  #
  #################################

  # the default value for parameters and attributes
  # @private
  OCI_DEFAULT                 =   0

  #################################
  #
  # Credential Types
  #
  #################################

  # database username/password credentials
  # @private
  OCI_CRED_RDBMS              =   1
  # externally provided credentials
  # @private
  OCI_CRED_EXT                =   2

  #################################
  #
  # Authentication Modes
  #
  #################################

  # for SYSDBA authorization
  # @private
  OCI_SYSDBA                  = 0x0002
  # for SYSOPER authorization
  # @private
  OCI_SYSOPER                 = 0x0004
  # for SYSASM authorization
  # @private
  OCI_SYSASM                  = 0x8000

  #################################
  #
  # OCI Parameter Types
  #
  #################################

  # parameter type for unknown type
  # @private
  OCI_PTYPE_UNK               =  0
  # parameter type for table
  # @private
  OCI_PTYPE_TABLE             =  1
  # parameter type for view
  # @private
  OCI_PTYPE_VIEW              =  2
  # parameter type for procedure
  # @private
  OCI_PTYPE_PROC              =  3
  # parameter type for function
  # @private
  OCI_PTYPE_FUNC              =  4
  # parameter type for package
  # @private
  OCI_PTYPE_PKG               =  5
  # parameter type for user-defined type
  # @private
  OCI_PTYPE_TYPE              =  6
  # parameter type for synonym
  # @private
  OCI_PTYPE_SYN               =  7
  # parameter type for sequence
  # @private
  OCI_PTYPE_SEQ               =  8
  # parameter type for column
  # @private
  OCI_PTYPE_COL               =  9
  # parameter type for argument
  # @private
  OCI_PTYPE_ARG               = 10
  # parameter type for list
  # @private
  OCI_PTYPE_LIST              = 11
  # parameter type for user-defined type's attribute
  # @private
  OCI_PTYPE_TYPE_ATTR         = 12
  # parameter type for collection type's element
  # @private
  OCI_PTYPE_TYPE_COLL         = 13
  # parameter type for user-defined type's method
  # @private
  OCI_PTYPE_TYPE_METHOD       = 14
  # parameter type for user-defined type method's arg
  # @private
  OCI_PTYPE_TYPE_ARG          = 15
  # parameter type for user-defined type method's result
  # @private
  OCI_PTYPE_TYPE_RESULT       = 16
  # parameter type for schema
  # @private
  OCI_PTYPE_SCHEMA            = 17
  # parameter type for database
  # @private
  OCI_PTYPE_DATABASE          = 18
  # parameter type for rule
  # @private
  OCI_PTYPE_RULE              = 19
  # parameter type for rule set
  # @private
  OCI_PTYPE_RULE_SET          = 20
  # parameter type for evaluation context
  # @private
  OCI_PTYPE_EVALUATION_CONTEXT = 21
  # parameter type for table alias
  # @private
  OCI_PTYPE_TABLE_ALIAS       = 22
  # parameter type for variable type
  # @private
  OCI_PTYPE_VARIABLE_TYPE     = 23
  # parameter type for name value pair
  # @private
  OCI_PTYPE_NAME_VALUE        = 24

  #################################
  #
  # OCI List Types
  #
  #################################

  # list type for unknown type
  # @private
  OCI_LTYPE_UNK                =  0
  # list type for column list
  # @private
  OCI_LTYPE_COLUMN             =  1
  # list type for procedure argument list
  # @private
  OCI_LTYPE_ARG_PROC           =  2
  # list type for function argument list
  # @private
  OCI_LTYPE_ARG_FUNC           =  3
  # list type for subprogram list
  # @private
  OCI_LTYPE_SUBPRG             =  4
  # list type for type attribute
  # @private
  OCI_LTYPE_TYPE_ATTR          =  5
  # list type for type method
  # @private
  OCI_LTYPE_TYPE_METHOD        =  6
  # list type for type method w/o result argument list
  # @private
  OCI_LTYPE_TYPE_ARG_PROC      =  7
  # list type for type method w/result argument list
  # @private
  OCI_LTYPE_TYPE_ARG_FUNC      =  8
  # list type for schema object list
  # @private
  OCI_LTYPE_SCH_OBJ            =  9
  # list type for database schema list
  # @private
  OCI_LTYPE_DB_SCH             = 10
  # list type for subtype list
  # @private
  OCI_LTYPE_TYPE_SUBTYPE       = 11
  # list type for table alias list
  # @private
  OCI_LTYPE_TABLE_ALIAS        = 12
  # list type for variable type list
  # @private
  OCI_LTYPE_VARIABLE_TYPE      = 13
  # list type for name value list
  # @private
  OCI_LTYPE_NAME_VALUE         = 14

  #################################
  #
  # OBJECT Duration in oro.h
  #
  #################################

  #
  # @private
  OCI_DURATION_INVALID       = 0xFFFF
  # @private
  OCI_DURATION_BEGIN         = 10
  # @private
  OCI_DURATION_NULL          = OCI_DURATION_BEGIN - 1
  # @private
  OCI_DURATION_DEFAULT       = OCI_DURATION_BEGIN - 2
  # @private
  OCI_DURATION_USER_CALLBACK = OCI_DURATION_BEGIN - 3
  # @private
  OCI_DURATION_NEXT          = OCI_DURATION_BEGIN - 4
  # @private
  OCI_DURATION_SESSION       = OCI_DURATION_BEGIN
  # @private
  OCI_DURATION_TRANS         = OCI_DURATION_BEGIN + 1
  # @private
  OCI_DURATION_CALL          = OCI_DURATION_BEGIN + 2
  # @private
  OCI_DURATION_STATEMENT     = OCI_DURATION_BEGIN + 3
  # @private
  OCI_DURATION_CALLOUT       = OCI_DURATION_BEGIN + 4
end
