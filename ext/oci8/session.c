/*
  session.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCISession
If you use ((<OCIEnv#logon>)), you have no need to use this handle directly.
Because ((<OCIEnv#logon>)) create this handle implicitly and set it to ((<OCISvcCtx>)).

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCISession|))
=end
*/
#include "oci8.h"

/*
=begin
--- OCISession#begin(svc [, credt [, mode]])
     start user session under the specified server context.
     :svc
        ((<OCISvcCtx>)).
     :credt
        ((|OCI_CRED_RDBMS|)) or ((|OCI_CRED_EXT|)).
        Default value is ((|OCI_CRED_RDBMS|)).

        If you use ((|OCI_CRED_RDBMS|)), set ((<OCI_ATTR_USERNAME>))
        and ((<OCI_ATTR_PASSWORD>)) in advance.

     :mode
        ((|OCI_DEFAULT|)), ((|OCI_MIGRATE|)), ((|OCI_SYSDBA|)), ((|OCI_SYSOPER|)), 
        (((|OCI_SYSDBA|)) | ((|OCI_PRELIM_AUTH|))), or (((|OCI_SYSOPER|)) | ((|OCI_PRELIM_AUTH|))).
        Default value is ((|OCI_DEFAULT|)).

        If you need SYSDBA or SYSOPER privilege, use 
        ((|OCI_SYSDBA|)) or ((|OCI_SYSOPER|)) respectively.

     correspond native OCI function: ((|OCISessionBegin|))
=end
*/
static VALUE oci8_session_begin(int argc, VALUE *argv, VALUE self)
{
  VALUE vsvc, vcredt, vmode;
  oci8_handle_t *h;
  oci8_handle_t *svch;
  ub4 credt;
  ub4 mode;
  sword rv;

  rb_scan_args(argc, argv, "12", &vsvc, &vcredt, &vmode);
  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  Get_Int_With_Default(argc, 2, vcredt, credt, OCI_NTV_SYNTAX); /* 2 */
  Get_Int_With_Default(argc, 3, vmode, mode, OCI_DEFAULT); /* 3 */

  rv = OCISessionBegin(svch->hp, h->errhp, h->hp, credt, mode);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

/*
=begin
--- OCISession#end(svc [, vmode])
     terminate user Authentication Context

     :svc
        ((<OCISvcCtx>)).
     :mode
        ((|OCI_DEFAULT|)) only valid. Defalt value is ((|OCI_DEFAULT|)).

     correspond native OCI function: ((|OCISessionEnd|))
=end
*/
static VALUE oci8_session_end(int argc, VALUE *argv, VALUE self)
{
  VALUE vsvc, vmode;
  oci8_handle_t *h;
  oci8_handle_t *svch;
  ub4 mode;
  sword rv;

  rb_scan_args(argc, argv, "11", &vsvc, &vmode);
  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  Get_Int_With_Default(argc, 2, vmode, mode, OCI_DEFAULT); /* 2 */

  rv = OCISessionEnd(svch->hp, h->errhp, h->hp, mode);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

void Init_oci8_session(void)
{
  rb_define_method(cOCISession, "begin", oci8_session_begin, -1);
  rb_define_method(cOCISession, "end", oci8_session_end, -1);
}
