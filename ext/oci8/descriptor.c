/*
  descriptor.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDescriptor
The abstract class of OCI descriptors.
=end
*/
#include "oci8.h"

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
  rb_define_singleton_method(cOCIDescriptor, "new", oci8_s_new, 0);
}
