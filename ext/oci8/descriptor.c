/*
  descriptor.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDescriptor
The abstract class of OCI descriptors.
=end
*/
#include "oci8.h"

VALUE oci8_descriptor_do_initialize(VALUE self, VALUE venv, ub4 type)
{
  sword rv;
  oci8_handle_t *envh;
  oci8_handle_t *h;

  Get_Handle(self, h);
  Check_Handle(venv, OCIEnv, envh); /* 1 */
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

void Init_oci8_descriptor(void)
{
  rb_define_method(cOCIDescriptor, "attrGet", oci8_attr_get, 1);
  rb_define_method(cOCIDescriptor, "attrSet", oci8_attr_set, 2);
  rb_define_method(cOCIDescriptor, "free", oci8_handle_free, 0);
}
