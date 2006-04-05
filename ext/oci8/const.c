/*
  const.c - part of ruby-oci8

  Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>

  define constant values.
*/
#include "oci8.h"

ID oci8_id_code;
ID oci8_id_define_array;
ID oci8_id_bind_hash;
ID oci8_id_message;
ID oci8_id_new;
ID oci8_id_parse_error_offset;
ID oci8_id_server;
ID oci8_id_session;
ID oci8_id_sql;

struct oci8_names {
  const char *name;
  VALUE value;
};
typedef struct oci8_names oci8_names_t;
#define ENTRY(name) {#name, INT2FIX(name)}

static oci8_names_t oci8_typecode[] = {
  ENTRY(OCI_TYPECODE_VARCHAR),          /* 1(SQLT_CHR) */
  ENTRY(OCI_TYPECODE_NUMBER),           /* 2(SQLT_NUM) */
  ENTRY(OCI_TYPECODE_DATE),             /* 12(SQLT_DAT) */
  ENTRY(OCI_TYPECODE_UNSIGNED8),        /* 23(SQLT_BIN) */
  ENTRY(OCI_TYPECODE_RAW),              /* 95(SQLT_LVB) */
  ENTRY(OCI_TYPECODE_CHAR),             /* 96(SQLT_AFC) */
  ENTRY(OCI_TYPECODE_OBJECT),           /* 108(SQLT_NTY) */
  ENTRY(OCI_TYPECODE_BLOB),             /* 113(SQLT_BLOB) */
  ENTRY(OCI_TYPECODE_CLOB),             /* 112(SQLT_CLOB) */
  ENTRY(OCI_TYPECODE_NAMEDCOLLECTION),  /* 122(SQLT_NCO) */
  ENTRY(SQLT_RDD) /* 104 */
};
#define NUM_OF_OCI8_TYPECODE (sizeof(oci8_typecode) / sizeof(oci8_typecode[0]))

static VALUE oci8_make_names(oci8_names_t *names, size_t size)
{
  volatile VALUE ary;
  int i;

  ary = rb_ary_new();
  for (i = 0;i < size;i++)
    rb_ary_store(ary, FIX2INT(names[i].value), rb_str_new2(names[i].name));
  return ary;
}

void  Init_oci8_const(void)
{
  int i;

  oci8_id_code = rb_intern("code");
  oci8_id_define_array = rb_intern("define_array");
  oci8_id_bind_hash = rb_intern("bind_hash");
  oci8_id_message = rb_intern("message");
  oci8_id_new = rb_intern("new");
  oci8_id_parse_error_offset = rb_intern("parse_error_offset");
  oci8_id_server = rb_intern("server");
  oci8_id_session = rb_intern("session");
  oci8_id_sql = rb_intern("sql");

  rb_define_global_const("OCI_DEFAULT", INT2FIX(OCI_DEFAULT));
  rb_define_global_const("OCI_OBJECT", INT2FIX(OCI_OBJECT));
  rb_define_global_const("OCI_CRED_RDBMS", INT2FIX(OCI_CRED_RDBMS));
  rb_define_global_const("OCI_CRED_EXT", INT2FIX(OCI_CRED_EXT));
  rb_define_global_const("OCI_MIGRATE", INT2FIX(OCI_MIGRATE));
  rb_define_global_const("OCI_SYSDBA", INT2FIX(OCI_SYSDBA));
  rb_define_global_const("OCI_SYSOPER", INT2FIX(OCI_SYSOPER));
  rb_define_global_const("OCI_PRELIM_AUTH", INT2FIX(OCI_PRELIM_AUTH));

  /* OCIStmt#prepare */
  rb_define_global_const("OCI_NTV_SYNTAX", INT2FIX(OCI_NTV_SYNTAX));
  rb_define_global_const("OCI_V7_SYNTAX", INT2FIX(OCI_V7_SYNTAX));
  rb_define_global_const("OCI_V8_SYNTAX", INT2FIX(OCI_V8_SYNTAX));

  /* OCIStmt#execute */
#ifdef OCI_BATCH_ERRORS
  rb_define_global_const("OCI_BATCH_ERRORS", INT2FIX(OCI_BATCH_ERRORS));
#endif
  rb_define_global_const("OCI_COMMIT_ON_SUCCESS", INT2FIX(OCI_COMMIT_ON_SUCCESS));
  rb_define_global_const("OCI_DESCRIBE_ONLY", INT2FIX(OCI_DESCRIBE_ONLY));
  rb_define_global_const("OCI_EXACT_FETCH", INT2FIX(OCI_EXACT_FETCH));
#ifdef OCI_PARSE_ONLY
  rb_define_global_const("OCI_PARSE_ONLY", INT2FIX(OCI_PARSE_ONLY));
#endif
#ifdef OCI_STMT_SCROLLABLE_READONLY
  rb_define_global_const("OCI_STMT_SCROLLABLE_READONLY", INT2FIX(OCI_STMT_SCROLLABLE_READONLY));
#endif

  rb_define_global_const("OCI_AUTH", INT2FIX(OCI_AUTH));
#ifdef OCI_SHARED
  rb_define_global_const("OCI_SHARED", INT2FIX(OCI_SHARED));
#endif
#ifdef OCI_CPOOL
  rb_define_global_const("OCI_CPOOL", INT2FIX(OCI_CPOOL));
#endif
#ifdef OCI_NO_SHARING
  rb_define_global_const("OCI_NO_SHARING", INT2FIX(OCI_NO_SHARING));
#endif

  rb_define_global_const("OCI_FETCH_NEXT", INT2FIX(OCI_FETCH_NEXT));

  for (i = 0;i < oci8_attr_size;i++) {
    rb_define_global_const(oci8_attr_list[i].name, INT2FIX(i));
  }

  /* TYPE CODE */
  for (i = 0;i < NUM_OF_OCI8_TYPECODE;i++)
    rb_define_global_const(oci8_typecode[i].name, oci8_typecode[i].value);
  rb_define_global_const("OCI_TYPECODE_NAMES", oci8_make_names(oci8_typecode, NUM_OF_OCI8_TYPECODE));

  /* OCI Parameter Types */
  rb_define_global_const("OCI_PTYPE_UNK", INT2FIX(OCI_PTYPE_UNK));
  rb_define_global_const("OCI_PTYPE_TABLE", INT2FIX(OCI_PTYPE_TABLE));
  rb_define_global_const("OCI_PTYPE_VIEW", INT2FIX(OCI_PTYPE_VIEW));
  rb_define_global_const("OCI_PTYPE_PROC", INT2FIX(OCI_PTYPE_PROC));
  rb_define_global_const("OCI_PTYPE_FUNC", INT2FIX(OCI_PTYPE_FUNC));
  rb_define_global_const("OCI_PTYPE_PKG", INT2FIX(OCI_PTYPE_PKG));
  rb_define_global_const("OCI_PTYPE_TYPE", INT2FIX(OCI_PTYPE_TYPE));
  rb_define_global_const("OCI_PTYPE_SYN", INT2FIX(OCI_PTYPE_SYN));
  rb_define_global_const("OCI_PTYPE_SEQ", INT2FIX(OCI_PTYPE_SEQ));
  rb_define_global_const("OCI_PTYPE_COL", INT2FIX(OCI_PTYPE_COL));
  rb_define_global_const("OCI_PTYPE_ARG", INT2FIX(OCI_PTYPE_ARG));
  rb_define_global_const("OCI_PTYPE_LIST", INT2FIX(OCI_PTYPE_LIST));
  rb_define_global_const("OCI_PTYPE_TYPE_ATTR", INT2FIX(OCI_PTYPE_TYPE_ATTR));
  rb_define_global_const("OCI_PTYPE_TYPE_COLL", INT2FIX(OCI_PTYPE_TYPE_COLL));
  rb_define_global_const("OCI_PTYPE_TYPE_METHOD", INT2FIX(OCI_PTYPE_TYPE_METHOD));
  rb_define_global_const("OCI_PTYPE_TYPE_ARG", INT2FIX(OCI_PTYPE_TYPE_ARG));
  rb_define_global_const("OCI_PTYPE_TYPE_RESULT", INT2FIX(OCI_PTYPE_TYPE_RESULT));
#ifdef OCI_PTYPE_SCHEMA
  rb_define_global_const("OCI_PTYPE_SCHEMA", INT2FIX(OCI_PTYPE_SCHEMA));
#endif
#ifdef OCI_PTYPE_DATABASE
  rb_define_global_const("OCI_PTYPE_DATABASE", INT2FIX(OCI_PTYPE_DATABASE));
#endif

  /* Bind and Define Options */
  rb_define_global_const("OCI_DATA_AT_EXEC", INT2FIX(OCI_DATA_AT_EXEC));
  rb_define_global_const("OCI_DYNAMIC_FETCH", INT2FIX(OCI_DYNAMIC_FETCH));

  /* OCI Statement Types */
  rb_define_global_const("OCI_STMT_SELECT", INT2FIX(OCI_STMT_SELECT));
  rb_define_global_const("OCI_STMT_UPDATE", INT2FIX(OCI_STMT_UPDATE));
  rb_define_global_const("OCI_STMT_DELETE", INT2FIX(OCI_STMT_DELETE));
  rb_define_global_const("OCI_STMT_INSERT", INT2FIX(OCI_STMT_INSERT));
  rb_define_global_const("OCI_STMT_CREATE", INT2FIX(OCI_STMT_CREATE));
  rb_define_global_const("OCI_STMT_DROP", INT2FIX(OCI_STMT_DROP));
  rb_define_global_const("OCI_STMT_ALTER", INT2FIX(OCI_STMT_ALTER));
  rb_define_global_const("OCI_STMT_BEGIN", INT2FIX(OCI_STMT_BEGIN));
  rb_define_global_const("OCI_STMT_DECLARE", INT2FIX(OCI_STMT_DECLARE));

  /* Piece Definitions */
  rb_define_global_const("OCI_ONE_PIECE", INT2FIX(OCI_ONE_PIECE));
  rb_define_global_const("OCI_FIRST_PIECE", INT2FIX(OCI_FIRST_PIECE));
  rb_define_global_const("OCI_NEXT_PIECE", INT2FIX(OCI_NEXT_PIECE));
  rb_define_global_const("OCI_LAST_PIECE", INT2FIX(OCI_LAST_PIECE));
}
