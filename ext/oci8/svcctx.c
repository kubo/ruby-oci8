/*
  svcctx.c - part of ruby-oci8

  Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>

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

enum {T_IMPLICIT, T_EXPLICIT};
static VALUE sym_SYSDBA;
static VALUE sym_SYSOPER;

static VALUE oci8_svcctx_initialize(int argc, VALUE *argv, VALUE self)
{
  VALUE vusername;
  VALUE vpassword;
  VALUE vdbname;
  VALUE vmode;
  oci8_handle_t *h;
  oci8_handle_t *envh;
  oci8_string_t u, p, d;
  sword rv;

  rb_scan_args(argc, argv, "31", &vusername, &vpassword, &vdbname, &vmode);

  Get_Handle(self, h); /* 0 */
  Check_Handle(oci8_env, OCIEnv, envh); /* 1 */
  Get_String(vusername, u); /* 2 */
  Get_String(vpassword, p); /* 3 */
  Get_String(vdbname, d); /* 4 */
  if (NIL_P(vmode)) {
#if defined(HAVE_OCISESSIONGET)
#error TODO
#elif defined(HAVE_OCILOGON2)
#error TODO
#else
    rv = OCILogon(envh->hp, envh->errhp, (OCISvcCtx **)&h->hp,
		  u.ptr, u.len, p.ptr, p.len, d.ptr, d.len);
    if (rv != OCI_SUCCESS) {
      oci8_raise(envh->errhp, rv, NULL);
    }
#endif
    h->errhp = envh->errhp;
    h->u.svcctx.logon_type = T_IMPLICIT;
  } else {
    ub4 mode;
    OCISession *authhp = NULL;
    OCIServer *srvhp = NULL;

    Check_Type(vmode, T_SYMBOL);
    if (vmode == sym_SYSDBA) {
      mode = OCI_SYSDBA;
    } else if (vmode == sym_SYSOPER) {
      mode = OCI_SYSOPER;
    } else {
      rb_raise(rb_eArgError, "invalid privilege name %s (expect :SYSDBA or :SYSOPER)", rb_id2name(SYM2ID(vmode)));
    }
    h->errhp = envh->errhp;
    /* allocate OCI handles. */
    rv = OCIHandleAlloc(envh->hp, (void*)&h->hp, OCI_HTYPE_SVCCTX, 0, 0);
    if (rv != OCI_SUCCESS)
      oci8_env_raise(envh->hp, rv);
    rv = OCIHandleAlloc(envh->hp, (void*)&h->u.svcctx.authhp, OCI_HTYPE_SESSION, 0, 0);
    if (rv != OCI_SUCCESS)
      oci8_env_raise(envh->hp, rv);
    authhp = h->u.svcctx.authhp;
    rv = OCIHandleAlloc(envh->hp, (void*)&h->u.svcctx.srvhp, OCI_HTYPE_SERVER, 0, 0);
    if (rv != OCI_SUCCESS)
      oci8_env_raise(envh->hp, rv);
    srvhp = h->u.svcctx.srvhp;

    /* set username and password to OCISession. */
    rv = OCIAttrSet(authhp, OCI_HTYPE_SESSION, u.ptr, u.len, OCI_ATTR_USERNAME, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    rv = OCIAttrSet(authhp, OCI_HTYPE_SESSION, p.ptr, p.len, OCI_ATTR_PASSWORD, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);

    /* attach to server and set to OCISvcCtx. */
    rv = OCIServerAttach(srvhp, h->errhp, d.ptr, d.len, OCI_DEFAULT);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    rv = OCIAttrSet(h->hp, OCI_HTYPE_SVCCTX, srvhp, 0, OCI_ATTR_SERVER, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);

    /* attach to server and set to OCISvcCtx. */
    rv = OCISessionBegin(h->hp, h->errhp, authhp, OCI_CRED_RDBMS, mode);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    rv = OCIAttrSet(h->hp, OCI_HTYPE_SVCCTX, authhp, 0, OCI_ATTR_SESSION, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    h->u.svcctx.logon_type = T_EXPLICIT;
  }
  h->type = OCI_HTYPE_SVCCTX;
  oci8_link(envh, h);
  return Qnil;
}

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
  switch (h->u.svcctx.logon_type) {
  case T_IMPLICIT:
    rv = OCILogoff(h->hp, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    break;
  case T_EXPLICIT:
    rv = OCISessionEnd(h->hp, h->errhp, h->u.svcctx.authhp, OCI_DEFAULT);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    rv = OCIServerDetach(h->u.svcctx.srvhp, h->errhp, OCI_DEFAULT);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
    break;
  }
  oci8_handle_free(self);
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

static VALUE oci8_svcctx_non_blocking_p(VALUE self)
{
  oci8_handle_t *h;
  sb1 non_blocking;
  sword rv;

  Get_Handle(self, h); /* 0 */
  if (h->u.svcctx.srvhp == NULL) {
    rv = OCIAttrGet(h->hp, OCI_HTYPE_SVCCTX, &h->u.svcctx.srvhp, 0, OCI_ATTR_SERVER, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
  }
  rv = OCIAttrGet(h->u.svcctx.srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return non_blocking ? Qtrue : Qfalse;
}

static VALUE oci8_svcctx_set_non_blocking(VALUE self, VALUE val)
{
  oci8_handle_t *h;
  sb1 non_blocking;
  sword rv;

  Get_Handle(self, h); /* 0 */
  if (h->u.svcctx.srvhp == NULL) {
    rv = OCIAttrGet(h->hp, OCI_HTYPE_SVCCTX, &h->u.svcctx.srvhp, 0, OCI_ATTR_SERVER, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
  }
  rv = OCIAttrGet(h->u.svcctx.srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  if (RTEST(val) && !non_blocking) {
    /* toggle blocking / non-blocking. */
    rv = OCIAttrSet(h->u.svcctx.srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, h->errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(h->errhp, rv, NULL);
  }
  return val;
}

static VALUE oci8_server_version(VALUE self)
{
  oci8_handle_t *h;
  OraText buf[1024];
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIServerVersion(h->hp, h->errhp, buf, sizeof(buf), h->type);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return rb_str_new2(buf);
}

#ifdef HAVE_OCISERVERRELEASE
static VALUE oci8_server_release(VALUE self)
{
  oci8_handle_t *h;
  OraText buf[1024];
  ub4 version = 0;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIServerRelease(h->hp, h->errhp, buf, sizeof(buf), h->type, &version);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return rb_ary_new3(2, INT2FIX(version), rb_str_new2(buf));
}
#endif

static VALUE oci8_break(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIBreak(h->hp, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

#ifdef HAVE_OCIRESET
static VALUE oci8_reset(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIReset(h->hp, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}
#endif

void Init_oci8_svcctx(void)
{
  sym_SYSDBA = ID2SYM(rb_intern("SYSDBA"));
  sym_SYSOPER = ID2SYM(rb_intern("SYSOPER"));

  rb_define_method(cOCISvcCtx, "initialize", oci8_svcctx_initialize, -1);
  rb_define_method(cOCISvcCtx, "logoff", oci8_svcctx_logoff, 0);
  rb_define_method(cOCISvcCtx, "passwordChange", oci8_password_change, -1);
  rb_define_method(cOCISvcCtx, "commit", oci8_trans_commit, -1);
  rb_define_method(cOCISvcCtx, "rollback", oci8_trans_rollback, -1);
  rb_define_method(cOCISvcCtx, "non_blocking?", oci8_svcctx_non_blocking_p, 0);
  rb_define_method(cOCISvcCtx, "non_blocking=", oci8_svcctx_set_non_blocking, 1);
  rb_define_method(cOCISvcCtx, "version", oci8_server_version, 0);
#ifdef HAVE_OCISERVERRELEASE
  rb_define_method(cOCISvcCtx, "release", oci8_server_release, 0);
#endif
  rb_define_method(cOCISvcCtx, "break", oci8_break, 0);
#ifdef HAVE_OCIRESET
  rb_define_method(cOCISvcCtx, "reset", oci8_reset, 0);
#endif
}
