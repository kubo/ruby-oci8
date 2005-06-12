/*
  bind.c - part of ruby-oci8

  Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIBind
The bind handle, which is created by ((<OCIStmt#bindByPos>)) or ((<OCIStmt#bindByName>)).

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIBind|))
=end
*/
#include "oci8.h"

static ID id_bind_type;
static ID id_set;

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_handle_t *bh)
{
  return rb_str_new(bh->valuep, bh->rlen);
}

static void bind_string_set(oci8_bind_handle_t *bh, VALUE val)
{
  StringValue(val);
  if (RSTRING(val)->len > bh->value_sz) {
    rb_raise(rb_eArgError, "too long String to set. (%d for %d)", RSTRING(val)->len, bh->value_sz);
  }
  memcpy(bh->valuep, RSTRING(val)->ptr, RSTRING(val)->len);
  bh->rlen = RSTRING(val)->len;
}

static void bind_string_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  sb4 sz = 0;

  if (NIL_P(length)) {
    if (NIL_P(*val)) {
      rb_raise(rb_eArgError, "value and length are both null.");
    }
    StringValue(*val);
    sz = RSTRING(*val)->len;
  } else {
    sz = NUM2INT(length);
  }
  if (sz <= 0) {
    rb_raise(rb_eArgError, "invalid bind length %d", sz);
  }
  bh->valuep = xmalloc(sz);
  bh->value_sz = sz;
}

static void bind_string_free(oci8_bind_handle_t *bh)
{
  if (bh->valuep != NULL)
    xfree(bh->valuep);
}

static oci8_bind_type_t bind_string = {
  bind_string_get,
  bind_string_set,
  bind_string_init,
  bind_string_free,
  SQLT_CHR,
};

/*
 * bind_raw
 */
static oci8_bind_type_t bind_raw = {
  bind_string_get,
  bind_string_set,
  bind_string_init,
  bind_string_free,
  SQLT_BIN,
};

/*
 * bind_fixnum
 */
static VALUE bind_fixnum_get(oci8_bind_handle_t *bh)
{
  return INT2FIX(bh->value.sw);
}

static void bind_fixnum_set(oci8_bind_handle_t *bh, VALUE val)
{
  Check_Type(val, T_FIXNUM);
  bh->value.sw = FIX2INT(val);
}

static void bind_fixnum_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.sw;
  bh->value_sz = sizeof(bh->value.sw);
}

static oci8_bind_type_t bind_fixnum = {
  bind_fixnum_get,
  bind_fixnum_set,
  bind_fixnum_init,
  NULL,
  SQLT_INT,
};

/*
 * bind_float
 */
static VALUE bind_float_get(oci8_bind_handle_t *bh)
{
  return rb_float_new(bh->value.dbl);
}

static void bind_float_set(oci8_bind_handle_t *bh, VALUE val)
{
  Check_Type(val, T_FLOAT);
  bh->value.dbl = RFLOAT(val)->value;
}

static void bind_float_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.dbl;
  bh->value_sz = sizeof(bh->value.dbl);
}

static oci8_bind_type_t bind_float = {
  bind_float_get,
  bind_float_set,
  bind_float_init,
  NULL,
  SQLT_FLT,
};

/*
 * bind_oradate
 */
static VALUE bind_oradate_get(oci8_bind_handle_t *bh)
{
  ora_date_t *od;
  VALUE obj = Data_Make_Struct(cOraDate, ora_date_t, NULL, xfree, od);
  memcpy(od, &(bh->value.od), sizeof(ora_date_t));
  return obj;
}

static void bind_oradate_set(oci8_bind_handle_t *bh, VALUE val)
{
  ora_date_t *od;
  Check_Object(val, OraDate);
  Data_Get_Struct(val, ora_date_t, od);
  memcpy(&(bh->value.od), od, sizeof(ora_date_t));
}

static void bind_oradate_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.od;
  bh->value_sz = sizeof(bh->value.od);
}

static oci8_bind_type_t bind_oradate = {
  bind_oradate_get,
  bind_oradate_set,
  bind_oradate_init,
  NULL,
  SQLT_DAT,
};

/*
 * bind_oranumber
 */
static VALUE bind_oranumber_get(oci8_bind_handle_t *bh)
{
  ora_vnumber_t *ovn;
  VALUE obj = Data_Make_Struct(cOraNumber, ora_vnumber_t, NULL, xfree, ovn);
  ovn->size = bh->rlen;
  memcpy(&(ovn->num), &(bh->value.on), sizeof(ora_number_t));
  return obj;
}

static void bind_oranumber_set(oci8_bind_handle_t *bh, VALUE val)
{
  ora_vnumber_t *ovn;
  Check_Object(val, OraNumber);
  Data_Get_Struct(val, ora_vnumber_t, ovn);
  bh->rlen = ovn->size;
  memcpy(&(bh->value.on), &(ovn->num), sizeof(ora_number_t));
}

static void bind_oranumber_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.on;
  bh->value_sz = sizeof(bh->value.on);
}

static oci8_bind_type_t bind_oranumber = {
  bind_oranumber_get,
  bind_oranumber_set,
  bind_oranumber_init,
  NULL,
  SQLT_NUM,
};

/*
 * bind_integer
 */
static VALUE bind_integer_get(oci8_bind_handle_t *bh)
{
  unsigned char buf[ORA_NUMBER_BUF_SIZE];
  ora_number_to_str(buf, NULL, &bh->value.on, bh->rlen);
  return rb_cstr2inum(buf, 10);
}

static void bind_integer_set(oci8_bind_handle_t *bh, VALUE val)
{
  rb_notimplement();
}

static oci8_bind_type_t bind_integer = {
  bind_integer_get,
  bind_integer_set,
  bind_oranumber_init,
  NULL,
  SQLT_NUM,
};

/*
 * bind_rowid
 */
static VALUE bind_handle_get(oci8_bind_handle_t *bh)
{
  return bh->obj;
}

static void bind_rowid_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCIRowid))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIRowid)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static void bind_rowid_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.hp;
  bh->value_sz = sizeof(bh->value.hp);
  if (NIL_P(*val)) {
    *val = rb_funcall(cOCIRowid, oci8_id_new, 0);
  }
}

static oci8_bind_type_t bind_rowid = {
  bind_handle_get,
  bind_rowid_set,
  bind_rowid_init,
  NULL,
  SQLT_RDD,
};

/*
 * bind_clob
 */
static void bind_clob_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static void bind_lob_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.hp;
  bh->value_sz = sizeof(bh->value.hp);
  if (NIL_P(*val)) {
    *val = rb_funcall(cOCILobLocator, oci8_id_new, 0);
  }
}

static oci8_bind_type_t bind_clob = {
  bind_handle_get,
  bind_clob_set,
  bind_lob_init,
  NULL,
  SQLT_CLOB,
};

/*
 * bind_blob
 */
static void bind_blob_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static oci8_bind_type_t bind_blob = {
  bind_handle_get,
  bind_blob_set,
  bind_lob_init,
  NULL,
  SQLT_BLOB,
};

/*
 * bind_stmt
 */
static void bind_stmt_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCIStmt))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIStmt)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static void bind_stmt_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.hp;
  bh->value_sz = sizeof(bh->value.hp);
  if (NIL_P(*val)) {
    *val = rb_funcall(cOCIStmt, oci8_id_new, 0);
  }
}

static oci8_bind_type_t bind_stmt = {
  bind_handle_get,
  bind_stmt_set,
  bind_stmt_init,
  NULL,
  SQLT_RSET,
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
  oci8_bind_handle_t *bh;

  Data_Get_Struct(self, oci8_bind_handle_t, bh);
  if (bh->ind != 0)
    return Qnil;
  return bh->bind_type->get(bh);
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
  oci8_bind_handle_t *bh;

  Data_Get_Struct(self, oci8_bind_handle_t, bh);
  if (NIL_P(val)) {
    bh->ind = -1;
  } else {
    bh->bind_type->set(bh, val);
    bh->ind = 0;
  }
  return self;
}

VALUE oci8_bind_s_allocate(VALUE klass)
{
  oci8_bind_handle_t *bh;
  VALUE obj;
  VALUE bind_type;

  if (klass == cOCIBind) {
    rb_raise(rb_eRuntimeError, "Could not create an abstract class OCIBind");
  }
  obj = Data_Make_Struct(klass, oci8_bind_handle_t, oci8_handle_mark, oci8_handle_cleanup, bh);
  bh->self = obj;
  bh->ind = -1;
  bh->obj = Qnil;
  bind_type = rb_ivar_get(klass, id_bind_type);
  while (NIL_P(bind_type)) {
    klass = RCLASS(klass)->super;
    bind_type = rb_ivar_get(klass, id_bind_type);
  }
  bh->bind_type = DATA_PTR(bind_type);
  return obj;
}

VALUE oci8_bind_initialize(VALUE self, VALUE val, VALUE length, VALUE prec, VALUE scale)
{
  oci8_bind_handle_t *bh = DATA_PTR(self);

  bh->bind_type->init(bh, &val, length, prec, scale);
  bh->rlen = bh->value_sz;
  if (!NIL_P(val)) {
    /* don't call oci8_set_data() directly.
     * #set(val) may be overwritten by subclass.
     */
    rb_funcall(self, id_set, 1, val);
  }
  return Qnil;
}

static struct {
  const char *name;
  oci8_bind_type_t *bind_type;
} bind_types[] = {
  {"String",    &bind_string},
  {"RAW",       &bind_raw},
  {"Fixnum",    &bind_fixnum},
  {"Float",     &bind_float},
  {"OraDate",   &bind_oradate},
  {"OraNumber", &bind_oranumber},
  {"Integer",   &bind_integer},
  {"OCIRowid",  &bind_rowid},
  {"CLOB",      &bind_clob},
  {"BLOB",      &bind_blob},
  {"Cursor",    &bind_stmt},
};
#define NUM_OF_BIND_TYPES (sizeof(bind_types)/sizeof(bind_types[0]))

void Init_oci8_bind(void)
{
  int i;
  id_bind_type = rb_intern("bind_type");
  id_set = rb_intern("set");

  rb_define_alloc_func(cOCIBind, oci8_bind_s_allocate);

  for (i = 0; i < NUM_OF_BIND_TYPES; i++) {
    VALUE klass = rb_define_class_under(mOCI8BindType, bind_types[i].name, cOCIBind);
    rb_ivar_set(klass, id_bind_type, Data_Wrap_Struct(rb_cObject, 0, 0, bind_types[i].bind_type));
  }

  rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 4);
  rb_define_method(cOCIBind, "get", oci8_get_data, 0);
  rb_define_method(cOCIBind, "set", oci8_set_data, 1);
}
