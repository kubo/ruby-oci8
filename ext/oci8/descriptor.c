/*
  descriptor.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDescriptor
The abstract class of OCI descriptors.
=end
*/
#include "oci8.h"

VALUE oci8_descriptor_do_initialize(VALUE self, ub4 type)
{
  sword rv;
  oci8_handle_t *envh;
  oci8_handle_t *h;

  Get_Handle(self, h);
  Check_Handle(oci8_env, OCIEnv, envh);
  rv = OCIDescriptorAlloc(envh->hp, &h->hp, type, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envh->hp, rv);
  h->type = type;
  h->errhp = envh->errhp;
  oci8_link(envh, h);
  return Qnil;
}

/*
=begin
--- OCIDescriptor#attrGet(type)
     :type
       the type of attribute.
     :return value
       depends on argument ((|type|)).

     See also ((<Attributes of Handles and Descriptors>)).
=end
*/

/*
=begin
--- OCIDescriptor#free()
     explicitly free the OCI's data structure associated with this object.
     This call will not free ruby's object itself, which wrap the OCI's data structure.
=end
*/

static VALUE oci8_rowid_initialize(VALUE self)
{
  oci8_handle_t *h;

  Get_Handle(self, h);
  oci8_descriptor_do_initialize(self, OCI_DTYPE_ROWID);
  return Qnil;
}

void Init_oci8_descriptor(void)
{
  rb_define_method(cOCIDescriptor, "free", oci8_handle_free, 0);
  rb_define_method(cOCIRowid, "initialize", oci8_rowid_initialize, 0);
}
