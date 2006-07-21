/*
  define.c - part of ruby-oci8

  Copyright (C) 2002,2006 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIBind
The bind handle, which is created by ((<OCIStmt#bindByPos>)) or ((<OCIStmt#bindByName>)).

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIBind|))
=end
*/
#include "oci8.h"

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
  oci8_bind_handle_t *defnhp;
  VALUE obj;

  Data_Get_Struct(self, oci8_bind_handle_t, defnhp);
  obj = oci8_get_value(defnhp);
  return obj;
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
  oci8_set_value(hp, val);
  return self;
}

void Init_oci8_bind(void)
{
  rb_define_method(cOCIBind, "get", oci8_get_data, 0);
  rb_define_method(cOCIBind, "set", oci8_set_data, 1);
}

VALUE oci8_get_value(oci8_bind_handle_t *hp)
{
  ora_date_t *od;
  ora_vnumber_t *ovn;
  VALUE obj;
  unsigned char buf[ORA_NUMBER_BUF_SIZE];
  int year, month, day, hour, minute, second;
  static ID id_local = (ID)-1;

  if (hp->ind != 0)
    return Qnil;
  switch (hp->bind_type) {
  case BIND_STRING:
    return rb_str_new(hp->value.str.buf, hp->value.str.len);
  case BIND_FIXNUM:
    return INT2FIX(hp->value.sw);
  case BIND_INTEGER_VIA_ORA_NUMBER:
    ora_number_to_str(buf, NULL, &(hp->value.on), hp->rlen);
    return rb_cstr2inum(buf, 10);
  case BIND_TIME_VIA_ORA_DATE:
    oci8_get_ora_date(&(hp->value.od), &year, &month, &day, &hour, &minute, &second);
    if (id_local == (ID)-1)
      id_local = rb_intern("local");
    return rb_funcall(rb_cTime, id_local, 6, INT2FIX(year), INT2FIX(month), INT2FIX(day), INT2FIX(hour), INT2FIX(minute), INT2FIX(second));
  case BIND_FLOAT:
    return rb_float_new(hp->value.dbl);
  case BIND_ORA_DATE:
    obj = Data_Make_Struct(cOraDate, ora_date_t, NULL, xfree, od);
    memcpy(od, &(hp->value.od), sizeof(ora_date_t));
    return obj;
  case BIND_ORA_NUMBER:
    obj = Data_Make_Struct(cOraNumber, ora_vnumber_t, NULL, xfree, ovn);
    ovn->size = hp->rlen;
    memcpy(&(ovn->num), &(hp->value.on), sizeof(ora_number_t));
    return obj;
  case BIND_HANDLE:
    return hp->value.v;
  }
  rb_bug("unsupported data type: %d", hp->bind_type);
}

void oci8_set_value(oci8_bind_handle_t *hp, VALUE val)
{
  ora_date_t *od;
  ora_vnumber_t *ovn;
  int year, mon, day, hour, min, sec;
  static ID id_year = (ID)-1;
  static ID id_mon = (ID)-1;
  static ID id_day = (ID)-1;
  static ID id_hour = (ID)-1;
  static ID id_min = (ID)-1;
  static ID id_sec = (ID)-1;

  if (NIL_P(val)) {
    hp->ind = -1;
    return;
  }
  switch (hp->bind_type) {
  case BIND_FIXNUM:
    Check_Type(val, T_FIXNUM);
    hp->value.sw = FIX2INT(val);
    break;
  case BIND_STRING:
    Check_Type(val, T_STRING);
    if (hp->value_sz - 4 < RSTRING(val)->len) {
      rb_raise(rb_eArgError, "Assigned string is too long. %d (max %d)", RSTRING(val)->len, hp->value_sz - 4);
    }
    memcpy(hp->value.str.buf, RSTRING(val)->ptr, RSTRING(val)->len);
    hp->value.str.len = RSTRING(val)->len;
    break;
  case BIND_TIME_VIA_ORA_DATE:
    if (!rb_obj_is_instance_of(val, rb_cTime)) {
      rb_raise(rb_eTypeError, "invalid argument (expect Time)");
    }
#define GET_TIME_OF_XXX(name) \
    if (id_##name == (ID)-1) \
      id_##name = rb_intern(#name); \
    name = FIX2INT(rb_funcall(val, id_##name, 0))
    GET_TIME_OF_XXX(year);
    GET_TIME_OF_XXX(mon);
    GET_TIME_OF_XXX(day);
    GET_TIME_OF_XXX(hour);
    GET_TIME_OF_XXX(min);
    GET_TIME_OF_XXX(sec);
    oci8_set_ora_date(&(hp->value.od), year, mon, day, hour, min, sec);
    break;
  case BIND_FLOAT:
    Check_Type(val, T_FLOAT);
    hp->value.dbl = RFLOAT(val)->value;
    break;
  case BIND_ORA_DATE:
    if (!rb_obj_is_instance_of(val, cOraDate)) {
      rb_raise(rb_eTypeError, "invalid argument (expect OraDate)");
    }
    Data_Get_Struct(val, ora_date_t, od);
    memcpy(&(hp->value.od), od, sizeof(ora_date_t));
    break;
  case BIND_INTEGER_VIA_ORA_NUMBER:
  case BIND_ORA_NUMBER:
    if (rb_obj_is_instance_of(val, cOraNumber)) {
      Data_Get_Struct(val, ora_vnumber_t, ovn);
      hp->rlen = ovn->size;
      memcpy(&(hp->value.on), &(ovn->num), sizeof(ora_number_t));
    } else if (rb_obj_is_kind_of(val, rb_cNumeric)) {
      ora_vnumber_t ovn;
      if (set_oci_vnumber(&ovn, val, hp->errhp) == 0) {
	rb_raise(rb_eTypeError, "could not bind value.");
      }
      hp->rlen = ovn.size;
      memcpy(&(hp->value.on), &(ovn.num), sizeof(ora_number_t));
    } else {
      rb_raise(rb_eTypeError, "invalid argument (expect OraNumber)");
    }
    break;
  case BIND_HANDLE:
    rb_raise(rb_eRuntimeError, "it is not permitted to set to this handle.");
  default:
    rb_bug("unsupported data type: %d", hp->bind_type);
  }
  hp->ind = 0;
}
