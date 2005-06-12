/*
  param.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIParam
super class: ((<OCIDescriptor>))
=end
*/
#include "oci8.h"

static VALUE oci8_param_get_name(VALUE self)
{
  return oci8_get_string_attr(self, OCI_ATTR_NAME);
}

static VALUE oci8_param_get_data_type(VALUE self)
{
  return oci8_get_ub2_attr(self, OCI_ATTR_DATA_TYPE);
}

static VALUE oci8_param_get_data_size(VALUE self)
{
  return oci8_get_ub2_attr(self, OCI_ATTR_DATA_SIZE);
}

static VALUE oci8_param_get_precision(VALUE self)
{
  return oci8_get_sb2_attr(self, OCI_ATTR_PRECISION);
}

static VALUE oci8_param_get_scale(VALUE self)
{
  return oci8_get_sb1_attr(self, OCI_ATTR_SCALE);
}

void Init_oci8_param(void)
{
  rb_define_method(cOCIParam, "paramGet", oci8_param_get, 1);
  rb_define_method(cOCIParam, "name", oci8_param_get_name, 0);
  rb_define_method(cOCIParam, "data_type", oci8_param_get_data_type, 0);
  rb_define_method(cOCIParam, "data_size", oci8_param_get_data_size, 0);
  rb_define_method(cOCIParam, "precision", oci8_param_get_precision, 0);
  rb_define_method(cOCIParam, "scale", oci8_param_get_scale, 0);
}

VALUE oci8_param_get(VALUE self, VALUE pos)
{
  oci8_handle_t *h;
  OCIParam *parmhp = NULL;
  oci8_handle_t *parmh;
  VALUE obj;
  sword rv;

  Get_Handle(self, h);
  Check_Type(pos, T_FIXNUM);
  rv = OCIParamGet(h->hp, h->type, h->errhp, (dvoid *)&parmhp, FIX2INT(pos));
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  parmh = oci8_make_handle(OCI_DTYPE_PARAM, parmhp, h->errhp, h, 0);
  parmh->u.param.is_implicit = (h->type == OCI_HTYPE_STMT) ? 1 : 0;

  obj = parmh->self;
  return obj;
}
