/*
  define.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDefine
The define handle, which is created by ((<OCIStmt#defineByPos>)).

The fetched data of select statements is got via this handle.

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIDefine|))
=end
*/
#include "oci8.h"

/*
=begin
--- OCIDefine#get()
     get the selected date.

     :return value
        fetched data. Its datatype is correspond to the 2nd argument of ((<OCIStmt#defineByPos>)).

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

static VALUE oci8_set_data(VALUE self, VALUE val)
{
  oci8_bind_handle_t *hp;

  Data_Get_Struct(self, oci8_bind_handle_t, hp);
  oci8_set_value(hp, val);
  return self;
}

void Init_oci8_define(void)
{
  rb_define_method(cOCIDefine, "get", oci8_get_data, 0);
  rb_define_method(cOCIDefine, "set", oci8_set_data, 1);
}
