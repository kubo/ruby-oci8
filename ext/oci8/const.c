/*
  const.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

  define constant values.
*/
#include "oci8.h"

ID oci8_id_new;

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

  oci8_id_new = rb_intern("new");

  rb_define_global_const("OCI_DEFAULT", INT2FIX(OCI_DEFAULT));
  rb_define_global_const("OCI_COMMIT_ON_SUCCESS", INT2FIX(OCI_COMMIT_ON_SUCCESS));

  /* TYPE CODE */
  for (i = 0;i < NUM_OF_OCI8_TYPECODE;i++)
    rb_define_global_const(oci8_typecode[i].name, oci8_typecode[i].value);
  rb_define_global_const("OCI_TYPECODE_NAMES", oci8_make_names(oci8_typecode, NUM_OF_OCI8_TYPECODE));
}
