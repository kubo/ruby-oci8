/*
  describe.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDescribe
The describe handle, which is used for the explicit describe.

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIDescribe|))

For example:
  dsc = env.alloc(OCIDescribe)
  dsc.describeAny(svc, "EMP", OCI_PTYPE_TABLE)
  parm = dsc.attrGet(OCI_ATTR_PARAM)
  ...get various information from parm...

TODO: more explanation and examples.

=end
*/
#include "oci8.h"

/*
=begin
--- OCIDescribe#describeAny(svc, name, type)
     get various information of Oracle's schema objects: tables, views, synonyms, 
     procedures, functions, packages, sequences, and types.

     :svc
        ((<service context handle|OCISvcCtx>)) in which the object to describe exists.
     :name
        name of object to describe.
     :type
        type of object to describe. 

        * ((|OCI_PTYPE_TABLE|)), for tables
        * ((|OCI_PTYPE_VIEW|)), for views
        * ((|OCI_PTYPE_PROC|)), for procedures
        * ((|OCI_PTYPE_FUNC|)), for functions
        * ((|OCI_PTYPE_PKG|)), for packages
        * ((|OCI_PTYPE_TYPE|)), for types
        * ((|OCI_PTYPE_SYN|)), for synonyms
        * ((|OCI_PTYPE_SEQ|)), for sequences
        * ((|OCI_PTYPE_SCHEMA|)), for schemas
        * ((|OCI_PTYPE_DATABASE|)), for databases
        * ((|OCI_PTYPE_UNK|)), for unknown schema objects

     correspond native OCI function: ((|OCIDescribeAny|))

     ((*note*)): To use this method in Oracle 8.0.5 for Linux,
     call OCIEnv.create with OCI_OBJECT or segmentation fault occurs.
     This bug was fixed 8.0.6 or later.
=end
*/
static VALUE oci8_describe_any(VALUE self, VALUE vdsc, VALUE vname, VALUE vtype)
{
  oci8_handle_t *h;
  oci8_handle_t *svch;
  oci8_string_t name;
  ub1 type;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vdsc, OCISvcCtx, svch); /* 1 */
  Get_String(vname, name); /* 2 */
  type = FIX2INT(vtype); /* 3 */

  rv = OCIDescribeAny(svch->hp, h->errhp, name.ptr, name.len, OCI_OTYPE_NAME, OCI_DEFAULT, type, h->hp);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

void Init_oci8_describe(void)
{
  rb_define_method(cOCIDescribe, "describeAny", oci8_describe_any, 3);
}
