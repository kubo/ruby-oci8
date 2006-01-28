/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  param.c - part of ruby-oci8

  Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIParam
super class: ((<OCIDescriptor>))
=end
*/
#include "oci8.h"

static ID id_at_name;
static ID id_at_data_type;
static ID id_at_data_size;
static ID id_at_precision;
static ID id_at_scale;

static VALUE cOCIParam;

VALUE oci8_param_create(OCIParam *parmhp, OCIError *errhp)
{
    union {
        text *ptr;
        ub2 _ub2;
        sb2 _sb2;
        sb1 _sb1;
    } val;
    ub4 size;
    sword rv;
    VALUE obj = Data_Wrap_Struct(cOCIParam, 0, 0, 0);

    /* name */
    rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, &size, OCI_ATTR_NAME, errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(errhp, rv, NULL);
    rb_ivar_set(obj, id_at_name, rb_str_new(val.ptr, size));
    /* data_type */
    rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, 0, OCI_ATTR_DATA_TYPE, errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(errhp, rv, NULL);
    rb_ivar_set(obj, id_at_data_type, INT2FIX(val._ub2));
    /* data_size */
    rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, 0, OCI_ATTR_DATA_SIZE, errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(errhp, rv, NULL);
    rb_ivar_set(obj, id_at_data_size, INT2FIX(val._ub2));
    /* precision */
    rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, 0, OCI_ATTR_PRECISION, errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(errhp, rv, NULL);
    rb_ivar_set(obj, id_at_precision, INT2FIX(val._sb2));
    /* scale */
    rv = OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &val, 0, OCI_ATTR_SCALE, errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(errhp, rv, NULL);
    rb_ivar_set(obj, id_at_scale, INT2FIX(val._sb1));
    return obj;
}

void Init_oci8_param(void)
{
    id_at_name = rb_intern("@name");
    id_at_data_type = rb_intern("@data_type");
    id_at_data_size = rb_intern("@data_size");
    id_at_precision = rb_intern("@precision");
    id_at_scale = rb_intern("@scale");

    cOCIParam = rb_define_class("OCIParam", rb_cObject);
    rb_define_attr(cOCIParam, "name", Qtrue, Qfalse);
    rb_define_attr(cOCIParam, "data_type", Qtrue, Qfalse);
    rb_define_attr(cOCIParam, "data_size", Qtrue, Qfalse);
    rb_define_attr(cOCIParam, "precision", Qtrue, Qfalse);
    rb_define_attr(cOCIParam, "scale", Qtrue, Qfalse);
}
