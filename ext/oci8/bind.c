/*
  define.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIBind
The bind handle, which is created by ((<OCIStmt#bindByPos>)) or ((<OCIStmt#bindByName>)).

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIBind|))
=end
*/
#include "oci8.h"

static VALUE bind_type_mapping;

/*
 * bind_string
 */
static sb4 bind_string_get_value_sz(VALUE vlength)
{
  if (NIL_P(vlength))
    rb_raise(rb_eArgError, "the length of String is not specified.");
  return NUM2INT(vlength);
}

static VALUE bind_string_get(oci8_bind_handle_t *hp)
{
  return rb_str_new(hp->value.str, hp->rlen);
}

static void bind_string_set(oci8_bind_handle_t *hp, VALUE val)
{
  Check_Type(val, T_STRING);
  if (hp->value_sz < RSTRING(val)->len) {
    rb_raise(rb_eArgError, "Assigned string is too long. %d (max %d)", RSTRING(val)->len, hp->value_sz);
  }
  memcpy(hp->value.str, RSTRING(val)->ptr, RSTRING(val)->len);
  hp->rlen = RSTRING(val)->len;
}

static oci8_bind_type_t bind_string = {
  0, SQLT_CHR,
  bind_string_get_value_sz,
  bind_string_get,
  bind_string_set
};

/*
 * bind_raw
 */

static oci8_bind_type_t bind_raw = {
  0, SQLT_BIN,
  bind_string_get_value_sz,
  bind_string_get,
  bind_string_set
};

/*
 * bind_fixnum
 */
static sb4 bind_fixnum_get_value_sz(VALUE vlength)
{
  return sizeof(sword);
}

static VALUE bind_fixnum_get(oci8_bind_handle_t *hp)
{
  return INT2FIX(hp->value.sw);
}

static void bind_fixnum_set(oci8_bind_handle_t *hp, VALUE val)
{
  Check_Type(val, T_FIXNUM);
  hp->value.sw = FIX2INT(val);
}

static oci8_bind_type_t bind_fixnum = {
  0, SQLT_INT,
  bind_fixnum_get_value_sz,
  bind_fixnum_get,
  bind_fixnum_set
};

/*
 * bind_float
 */
static sb4 bind_float_get_value_sz(VALUE vlength)
{
  return sizeof(double);
}

static VALUE bind_float_get(oci8_bind_handle_t *hp)
{
  return rb_float_new(hp->value.dbl);
}

static void bind_float_set(oci8_bind_handle_t *hp, VALUE val)
{
  Check_Type(val, T_FLOAT);
  hp->value.dbl = RFLOAT(val)->value;
}

static oci8_bind_type_t bind_float = {
  0, SQLT_FLT,
  bind_float_get_value_sz,
  bind_float_get,
  bind_float_set
};

/*
 * bind_oradate
 */
static sb4 bind_oradate_get_value_sz(VALUE vlength)
{
  return sizeof(ora_date_t);
}

static VALUE bind_oradate_get(oci8_bind_handle_t *hp)
{
  ora_date_t *od;
  VALUE obj = Data_Make_Struct(cOraDate, ora_date_t, NULL, xfree, od);
  memcpy(od, &(hp->value.od), sizeof(ora_date_t));
  return obj;
}

static void bind_oradate_set(oci8_bind_handle_t *hp, VALUE val)
{
  ora_date_t *od;
  if (!rb_obj_is_instance_of(val, cOraDate)) {
    rb_raise(rb_eTypeError, "invalid argument (expect OraDate)");
  }
  Data_Get_Struct(val, ora_date_t, od);
  memcpy(&(hp->value.od), od, sizeof(ora_date_t));
}

static oci8_bind_type_t bind_oradate = {
  0, SQLT_DAT,
  bind_oradate_get_value_sz,
  bind_oradate_get,
  bind_oradate_set
};

/*
 * bind_integer
 */
static sb4 bind_integer_get_value_sz(VALUE vlength)
{
  return sizeof(ora_number_t);
}

static VALUE bind_integer_get(oci8_bind_handle_t *hp)
{
  unsigned char buf[ORA_NUMBER_BUF_SIZE];
  ora_number_to_str(buf, NULL, &(hp->value.on), hp->rlen);
  return rb_cstr2inum(buf, 10);
}

static void bind_integer_set(oci8_bind_handle_t *hp, VALUE val)
{
  rb_notimplement();
}

static oci8_bind_type_t bind_integer = {
  0, SQLT_NUM,
  bind_integer_get_value_sz,
  bind_integer_get,
  bind_integer_set
};

/*
 * bind_oranumber
 */
static sb4 bind_oranumber_get_value_sz(VALUE vlength)
{
  return sizeof(ora_number_t);
}

static VALUE bind_oranumber_get(oci8_bind_handle_t *hp)
{
  ora_vnumber_t *ovn;
  VALUE obj = Data_Make_Struct(cOraNumber, ora_vnumber_t, NULL, xfree, ovn);
  ovn->size = hp->rlen;
  memcpy(&(ovn->num), &(hp->value.on), sizeof(ora_number_t));
  return obj;
}

static void bind_oranumber_set(oci8_bind_handle_t *hp, VALUE val)
{
  ora_vnumber_t *ovn;
  if (!rb_obj_is_instance_of(val, cOraNumber)) {
    rb_raise(rb_eTypeError, "invalid argument (expect OraNumber)");
  }
  Data_Get_Struct(val, ora_vnumber_t, ovn);
  hp->rlen = ovn->size;
  memcpy(&(hp->value.on), &(ovn->num), sizeof(ora_number_t));
}

static oci8_bind_type_t bind_oranumber = {
  0, SQLT_NUM,
  bind_oranumber_get_value_sz,
  bind_oranumber_get,
  bind_oranumber_set
};

/*
 * bind_rowid
 */
static sb4 bind_rowid_get_value_sz(VALUE vlength)
{
  if (!rb_obj_is_instance_of(vlength, cOCIRowid))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIRowid)", rb_class2name(CLASS_OF(vlength)));
  return sizeof(OCIRowid *);
}

static VALUE bind_handle_get(oci8_bind_handle_t *hp)
{
  return hp->value.v;
}

static void bind_handle_set(oci8_bind_handle_t *hp, VALUE val)
{
  rb_raise(rb_eRuntimeError, "it is not permitted to set to this handle.");
}

static oci8_bind_type_t bind_rowid = {
  1, SQLT_RDD,
  bind_rowid_get_value_sz,
  bind_handle_get,
  bind_handle_set
};

/*
 * bind_clob
 */
static sb4 bind_clob_get_value_sz(VALUE vlength)
{
  if (!rb_obj_is_instance_of(vlength, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(vlength)));
  return sizeof(OCILobLocator *);
}

static oci8_bind_type_t bind_clob = {
  1, SQLT_CLOB,
  bind_clob_get_value_sz,
  bind_handle_get,
  bind_handle_set
};

/*
 * bind_blob
 */
static sb4 bind_blob_get_value_sz(VALUE vlength)
{
  if (!rb_obj_is_instance_of(vlength, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(vlength)));
  return sizeof(OCILobLocator *);
}

static oci8_bind_type_t bind_blob = {
  1, SQLT_BLOB,
  bind_blob_get_value_sz,
  bind_handle_get,
  bind_handle_set
};

/*
 * bind_stmt
 */
static sb4 bind_stmt_get_value_sz(VALUE vlength)
{
  if (!rb_obj_is_instance_of(vlength, cOCIStmt))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIStmt)", rb_class2name(CLASS_OF(vlength)));
  return sizeof(OCIStmt *);
}

static oci8_bind_type_t bind_stmt = {
  1, SQLT_RSET,
  bind_stmt_get_value_sz,
  bind_handle_get,
  bind_handle_set
};

/*
=begin
--- OCIBind#get()
     get the bind value, which set by OCI call.

     :return value
        the bind value. Its datatype is correspond to the 2nd argument of ((<OCIStmt#bindByPos>)) or ((<OCIStmt#bindByName>)).

     correspond native OCI function: nothing
=end
*/
static VALUE oci8_get_data(VALUE self)
{
  oci8_bind_handle_t *hp;

  Data_Get_Struct(self, oci8_bind_handle_t, hp);
  if (hp->ind != 0)
    return Qnil;
  return hp->bind_type->get(hp);
}

/*
=begin
--- OCIBind#set(value)
     get the bind value to pass Oracle via OCI call.

     :value
        the value to set the bind handle. Its datatype must be correspond to the 2nd argument of ((<OCIStmt#bindByPos>)) or ((<OCIStmt#bindByName>)).

     correspond native OCI function: nothing
=end
*/
static VALUE oci8_set_data(VALUE self, VALUE val)
{
  oci8_bind_handle_t *hp;

  Data_Get_Struct(self, oci8_bind_handle_t, hp);
  if (NIL_P(val)) {
    hp->ind = -1;
  } else {
    hp->bind_type->set(hp, val);
    hp->ind = 0;
  }
  return self;
}

void Init_oci8_bind(void)
{
  bind_type_mapping = rb_hash_new();
  rb_global_variable(&bind_type_mapping);

  /* register oci8_bind_type_t structures to bind_type_mapping. */
  oci8_bind_type_set(INT2FIX(SQLT_CHR), &bind_string);
  oci8_bind_type_set(INT2FIX(SQLT_DAT), &bind_oradate);
  oci8_bind_type_set(INT2FIX(SQLT_LVB), &bind_raw);
  oci8_bind_type_set(INT2FIX(SQLT_BIN), &bind_raw);
  oci8_bind_type_set(INT2FIX(SQLT_RDD), &bind_rowid);
  oci8_bind_type_set(INT2FIX(SQLT_CLOB), &bind_clob);
  oci8_bind_type_set(INT2FIX(SQLT_BLOB), &bind_blob);
  oci8_bind_type_set(INT2FIX(SQLT_RSET), &bind_stmt);

  oci8_bind_type_set(rb_cFixnum, &bind_fixnum);
  oci8_bind_type_set(rb_cFloat, &bind_float);
  oci8_bind_type_set(rb_cInteger, &bind_integer);
  oci8_bind_type_set(rb_cBignum, &bind_integer);
  oci8_bind_type_set(cOraNumber, &bind_oranumber);

  rb_define_method(cOCIBind, "get", oci8_get_data, 0);
  rb_define_method(cOCIBind, "set", oci8_set_data, 1);
  rb_define_method(cOCIDefine, "get", oci8_get_data, 0);
}

/* register an oci8_bind_byte_t structure to bind_type_mapping. */
void oci8_bind_type_set(VALUE key, oci8_bind_type_t *bind_type)
{
  VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, bind_type);
  rb_hash_aset(bind_type_mapping, key, obj);
}

/* get an oci8_bind_byte_t structure from bind_type_mapping. */
oci8_bind_type_t *oci8_bind_type_get(VALUE key)
{
  VALUE val = rb_hash_aref(bind_type_mapping, key);
  if (NIL_P(val))
    return NULL;
  return DATA_PTR(val);
}
