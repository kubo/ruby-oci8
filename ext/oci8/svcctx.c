/*
  svcctx.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCISvcCtx
The service context handle is correspond to `session' compared
with other general database interfaces although OCI constains OCISession.

This handle cooperates with a ((<server handle|OCIServer>)), a
((<user session handle|OCISession>)), and a transaction handle.
But these three handles work at the back of it. So you don't have to use
them except when you have special purpose.

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCISvcCtx|))
=end
*/
#include "oci8.h"

/*
=begin
--- OCISvcCtx#logoff()
     disconnect from Oracle.

     If you use ((<OCIServer#attach>)) and ((<OCISession#begin>)) to logon,
     use ((<OCIServer#detach>)) and ((<OCISession#end>)) instead.
     See also ((<Simplified Logon>)) and ((<Explicit Attach and Begin Session>)).

     correspond native OCI function: ((|OCILogoff|))
=end
*/
static VALUE oci8_svcctx_logoff(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */

  rv = OCILogoff(h->hp, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

/*
=begin
--- OCISvcCtx#passwordChange(username, old_password, new_password [, mode])
     :username
        the username.
     :old_password
        old password of the user.
     :new_password
        new password of the user.
     :mode
        ((|OCI_DEFAULT|)) or ((|OCI_AUTH|)). Default value is ((|OCI_DEFAULT|)).

        For most cases, use default value. If you want to know detail,
        see "Oracle Call Interface Programmer's Guide".

     correspond native OCI function: ((|OCIPasswordChange|))
=end
*/
static VALUE oci8_password_change(int argc, VALUE *argv, VALUE self)
{
  VALUE vusername, vopasswd, vnpasswd, vmode;
  oci8_handle_t *h;
  oci8_string_t username, opasswd, npasswd;
  ub4 mode;
  sword rv;

  rb_scan_args(argc, argv, "31", &vusername, &vopasswd, &vnpasswd, &vmode);
  Get_Handle(self, h); /* 0 */
  Get_String(vusername, username); /* 1 */
  Get_String(vopasswd, opasswd); /* 2 */
  Get_String(vnpasswd, npasswd); /* 3 */
  Get_Int_With_Default(argc, 4, vmode, mode, OCI_DEFAULT); /* 4 */

  rv = OCIPasswordChange(h->hp, h->errhp, username.ptr, username.len,
			 opasswd.ptr, opasswd.len, npasswd.ptr, npasswd.len, mode);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

/*
=begin
--- OCISvcCtx#commit([flags])
     commit the transaction.

     :flags
        ((|OCI_DEFAULT|)) or ((|OCI_TRANS_TWOPHASE|)).
        Default value is ((|OCI_DEFAULT|)).

     correspond native OCI function: ((|OCITransCommit|))
=end
*/
static VALUE oci8_trans_commit(int argc, VALUE *argv, VALUE self)
{
  VALUE vflags;
  oci8_handle_t *h;
  ub4 flags;
  sword rv;

  rb_scan_args(argc, argv, "01", &vflags);
  Get_Handle(self, h); /* 0 */
  Get_Int_With_Default(argc, 1, vflags, flags, OCI_DEFAULT); /* 1 */

  rv = OCITransCommit(h->hp, h->errhp, flags);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

/*
=begin

--- OCISvcCtx#rollback([flags])
     rollback the transaction.

     :flags
        ((|OCI_DEFAULT|)) only valid. Default value is ((|OCI_DEFAULT|)).

     correspond native OCI function: ((|OCITransRollback|))
=end
*/
static VALUE oci8_trans_rollback(int argc, VALUE *argv, VALUE self)
{
  VALUE vflags;
  oci8_handle_t *h;
  ub4 flags;
  sword rv;

  rb_scan_args(argc, argv, "01", &vflags);
  Get_Handle(self, h); /* 0 */
  Get_Int_With_Default(argc, 1, vflags, flags, OCI_DEFAULT); /* 1 */

  rv = OCITransRollback(h->hp, h->errhp, flags);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

/* THIS WILL BE DELETED IN FUTURE RELEASE. */
static VALUE oci8_describe_any(VALUE self, VALUE vdsc, VALUE vname, VALUE vtype)
{
  oci8_handle_t *h;
  oci8_handle_t *dsch;
  oci8_string_t name;
  ub1 type;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vdsc, OCIDescribe, dsch); /* 1 */
  Get_String(vname, name); /* 2 */
  type = FIX2INT(vtype); /* 3 */

  rv = OCIDescribeAny(h->hp, h->errhp, name.ptr, name.len, OCI_OTYPE_NAME, OCI_DEFAULT, type, dsch->hp);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

static VALUE oci8_close_all_files(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCILobFileCloseAll(h->hp, h->errhp);
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  return self;
}

static VALUE oci8_pid(VALUE self)
{
  oci8_handle_t *h;

  Get_Handle(self, h); /* 0 */
  return INT2FIX(h->u.svcctx.pid);
}

void Init_oci8_svcctx(void)
{
  rb_define_method(cOCISvcCtx, "logoff", oci8_svcctx_logoff, 0);
  rb_define_method(cOCISvcCtx, "passwordChange", oci8_password_change, -1);
  rb_define_method(cOCISvcCtx, "commit", oci8_trans_commit, -1);
  rb_define_method(cOCISvcCtx, "rollback", oci8_trans_rollback, -1);
  rb_define_method(cOCISvcCtx, "describeAny", oci8_describe_any, 3); /* delete later. */
  rb_define_method(cOCISvcCtx, "version", oci8_server_version, 0);
#ifdef HAVE_OCISERVERRELEASE
  rb_define_method(cOCISvcCtx, "release", oci8_server_release, 0);
#endif
  rb_define_method(cOCISvcCtx, "break", oci8_break, 0);
#ifdef HAVE_OCIRESET
  rb_define_method(cOCISvcCtx, "reset", oci8_reset, 0);
#endif
  rb_define_method(cOCISvcCtx, "close_all_files", oci8_close_all_files, 0);
  rb_define_method(cOCISvcCtx, "pid", oci8_pid, 0);
}

/*
=begin
--- OCISvcCtx#version()
     get server version.

     :return value
        string of server version. For example
          Oracle8 Release 8.0.5.0.0 - Production
          PL/SQL Release 8.0.5.0.0 - Production

     correspond native OCI function: ((|OCIServerVersion|))

--- OCISvcCtx#release()
     get server version number and string

     :return value
        array of number and string. For example

          version_number, version_str = svc.release()
          version_number is 0x8005000.
          version_str is
            Oracle8 Release 8.0.5.0.0 - Production
            PL/SQL Release 8.0.5.0.0 - Production

     correspond native OCI function: ((|OCIServerVersion|))

     Oracle 9i or later?
=end
*/
