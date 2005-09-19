/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  svcctx.c - part of ruby-oci8

  Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>

*/
#include "oci8.h"

static VALUE cOCI8;

enum logon_type_t {T_IMPLICIT, T_EXPLICIT};

typedef struct  {
    oci8_base_t base;
    enum logon_type_t logon_type;
    OCISession *authhp;
    OCIServer *srvhp;
} oci8_svcctx_t;

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;
    if (svcctx->authhp) {
        OCIHandleFree(svcctx->authhp, OCI_HTYPE_SESSION);
        svcctx->authhp = NULL;
    }
    if (svcctx->srvhp) {
        OCIHandleFree(svcctx->srvhp, OCI_HTYPE_SERVER);
        svcctx->srvhp = NULL;
    }
}

static oci8_base_class_t oci8_svcctx_class = {
    NULL,
    oci8_svcctx_free,
    sizeof(oci8_svcctx_t)
};

static VALUE sym_SYSDBA;
static VALUE sym_SYSOPER;

static VALUE oci8_svcctx_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE vusername;
    VALUE vpassword;
    VALUE vdbname;
    VALUE vmode;
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    rb_scan_args(argc, argv, "31", &vusername, &vpassword, &vdbname, &vmode);

    StringValue(vusername); /* 1 */
    StringValue(vpassword); /* 2 */
    if (!NIL_P(vdbname)) {
        StringValue(vdbname); /* 3 */
    }
    if (NIL_P(vmode)) { /* 4 */
#if defined(HAVE_OCISESSIONGET)
#error TODO
#elif defined(HAVE_OCILOGON2)
#error TODO
#else
        rv = OCILogon(oci8_envhp, oci8_errhp, (OCISvcCtx **)&svcctx->base.hp,
                      RSTRING(vusername)->ptr, RSTRING(vusername)->len,
                      RSTRING(vpassword)->ptr, RSTRING(vpassword)->len,
                      NIL_P(vdbname) ? NULL : RSTRING(vdbname)->ptr,
                      NIL_P(vdbname) ? 0 : RSTRING(vdbname)->len);
        if (rv != OCI_SUCCESS) {
            oci8_raise(oci8_errhp, rv, NULL);
        }
#endif
        svcctx->base.type = OCI_HTYPE_SVCCTX;
        svcctx->logon_type = T_IMPLICIT;
    } else {
        ub4 mode;

        Check_Type(vmode, T_SYMBOL);
        if (vmode == sym_SYSDBA) {
            mode = OCI_SYSDBA;
        } else if (vmode == sym_SYSOPER) {
            mode = OCI_SYSOPER;
        } else {
            rb_raise(rb_eArgError, "invalid privilege name %s (expect :SYSDBA or :SYSOPER)", rb_id2name(SYM2ID(vmode)));
        }
        /* allocate OCI handles. */
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->base.hp, OCI_HTYPE_SVCCTX, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);
        svcctx->base.type = OCI_HTYPE_SVCCTX;
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->authhp, OCI_HTYPE_SESSION, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);

        /* set username and password to OCISession. */
        rv = OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                        RSTRING(vusername)->ptr, RSTRING(vusername)->len,
                        OCI_ATTR_USERNAME, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                        RSTRING(vpassword)->ptr, RSTRING(vpassword)->len,
                        OCI_ATTR_PASSWORD, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);

        /* attach to server and set to OCISvcCtx. */
        rv = OCIServerAttach(svcctx->srvhp, oci8_errhp,
                             NIL_P(vdbname) ? NULL : RSTRING(vdbname)->ptr,
                             NIL_P(vdbname) ? 0 : RSTRING(vdbname)->len, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->base.hp, OCI_HTYPE_SVCCTX, svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);

        /* attach to server and set to OCISvcCtx. */
        rv = OCISessionBegin(svcctx->base.hp, oci8_errhp, svcctx->authhp, OCI_CRED_RDBMS, mode);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->base.hp, OCI_HTYPE_SVCCTX, svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->logon_type = T_EXPLICIT;
    }
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
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    switch (svcctx->logon_type) {
    case T_IMPLICIT:
        rv = OCILogoff(svcctx->base.hp, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->base.type = 0;
        break;
    case T_EXPLICIT:
        rv = OCISessionEnd(svcctx->base.hp, oci8_errhp, svcctx->authhp, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIServerDetach(svcctx->srvhp, oci8_errhp, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        break;
    }
    oci8_base_free(&svcctx->base);
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
static VALUE oci8_password_change(VALUE self, VALUE username, VALUE opasswd, VALUE npasswd, VALUE mode)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    StringValue(username); /* 1 */
    StringValue(opasswd); /* 2 */
    StringValue(npasswd); /* 3 */
    Check_Type(mode, T_FIXNUM); /* 4 */

    rv = OCIPasswordChange(svcctx->base.hp, oci8_errhp,
                           RSTRING(username)->ptr, RSTRING(username)->len,
                           RSTRING(opasswd)->ptr, RSTRING(opasswd)->len,
                           RSTRING(npasswd)->ptr, RSTRING(npasswd)->len,
                           FIX2INT(mode));
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
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
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    ub4 flags;
    sword rv;

    rb_scan_args(argc, argv, "01", &vflags);
    Get_Int_With_Default(argc, 1, vflags, flags, OCI_DEFAULT); /* 1 */

    rv = OCITransCommit(svcctx->base.hp, oci8_errhp, flags);
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
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
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    ub4 flags;
    sword rv;

    rb_scan_args(argc, argv, "01", &vflags);
    Get_Int_With_Default(argc, 1, vflags, flags, OCI_DEFAULT); /* 1 */

    rv = OCITransRollback(svcctx->base.hp, oci8_errhp, flags);
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return self;
}

static VALUE oci8_svcctx_non_blocking_p(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sb1 non_blocking;
    sword rv;

    if (svcctx->srvhp == NULL) {
        rv = OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
    }
    rv = OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return non_blocking ? Qtrue : Qfalse;
}

static VALUE oci8_svcctx_set_non_blocking(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sb1 non_blocking;
    sword rv;

    if (svcctx->srvhp == NULL) {
        rv = OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
    }
    rv = OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    if ((RTEST(val) && !non_blocking) || (!RTEST(val) && non_blocking)) {
        /* toggle blocking / non-blocking. */
        rv = OCIAttrSet(svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
    }
    return val;
}

static VALUE oci8_server_version(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    OraText buf[1024];
    sword rv;

    rv = OCIServerVersion(svcctx->base.hp, oci8_errhp, buf, sizeof(buf), OCI_HTYPE_SVCCTX);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return rb_str_new2(buf);
}

#ifdef HAVE_OCISERVERRELEASE
static VALUE oci8_server_release(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    OraText buf[1024];
    ub4 version = 0;
    sword rv;

    rv = OCIServerRelease(svcctx->base.hp, oci8_errhp, buf, sizeof(buf), OCI_HTYPE_SVCCTX, &version);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return rb_ary_new3(2, INT2FIX(version), rb_str_new2(buf));
}
#endif

static VALUE oci8_break(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    rv = OCIBreak(svcctx->base.hp, oci8_errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(oci8_errhp, rv, NULL);
    return self;
}

#ifdef HAVE_OCIRESET
static VALUE oci8_reset(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    rv = OCIReset(svcctx->base.hp, oci8_errhp);
    if (rv != OCI_SUCCESS)
      oci8_raise(oci8_errhp, rv, NULL);
    return self;
}
#endif

VALUE Init_oci8_svcctx(void)
{
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_class);

    sym_SYSDBA = ID2SYM(rb_intern("SYSDBA"));
    sym_SYSOPER = ID2SYM(rb_intern("SYSOPER"));

    rb_define_private_method(cOCI8, "__initialize", oci8_svcctx_initialize, -1);
    rb_define_private_method(cOCI8, "__logoff", oci8_svcctx_logoff, 0);
    rb_define_private_method(cOCI8, "__passwordChange", oci8_password_change, 4);
    rb_define_private_method(cOCI8, "__commit", oci8_trans_commit, -1);
    rb_define_private_method(cOCI8, "__rollback", oci8_trans_rollback, -1);
    rb_define_private_method(cOCI8, "__non_blocking?", oci8_svcctx_non_blocking_p, 0);
    rb_define_private_method(cOCI8, "__non_blocking=", oci8_svcctx_set_non_blocking, 1);
    rb_define_method(cOCI8, "version", oci8_server_version, 0);
#ifdef HAVE_OCISERVERRELEASE
    rb_define_method(cOCI8, "release", oci8_server_release, 0);
#endif
    rb_define_private_method(cOCI8, "__break", oci8_break, 0);
#ifdef HAVE_OCIRESET
    rb_define_private_method(cOCI8, "__reset", oci8_reset, 0);
#endif

    return cOCI8;
}

static oci8_svcctx_t *oci8_get_svcctx(VALUE obj)
{
    oci8_base_t *base;
    Check_Handle(obj, cOCI8, base);
    if (base->type == 0) {
        rb_raise(rb_eTypeError, "invalid argument %s was freed already.", rb_class2name(CLASS_OF(obj)));
    }
    return (oci8_svcctx_t *)base;
}

OCISvcCtx *oci8_get_oci_svcctx(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);
    return svcctx->base.hp;
}

OCISession *oci8_get_oci_session(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);

    if (svcctx->authhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp));
    }
    return svcctx->authhp;
}
