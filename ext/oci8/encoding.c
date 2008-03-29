/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * encoding.c - part of ruby-oci8
 *
 * Copyright (C) 2008 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

#ifndef OCI_NLS_MAXBUFSZ
#define OCI_NLS_MAXBUFSZ 100
#endif

/* type of callback function's argument */
typedef struct {
    oci8_svcctx_t *svcctx;
    OCIStmt *stmtp;
    union {
        VALUE name;
        int csid;
    } u;
} cb_arg_t;

/* Oracle charset id -> Oracle charset name */
static VALUE csid2name;
VALUE oci8_charset_id2name(VALUE svc, VALUE csid);
static VALUE charset_id2name_func(cb_arg_t *arg);
static VALUE ensure_func(cb_arg_t *arg);

#ifdef HAVE_TYPE_RB_ENCODING
/* Oracle charset name -> Oracle charset id */
static ID id_upcase;
static VALUE csname2id;
static VALUE oci8_charset_name2id(VALUE svc, VALUE name);
static VALUE charset_name2id_func(cb_arg_t *arg);
#endif /* HAVE_TYPE_RB_ENCODING */

VALUE oci8_charset_id2name(VALUE svc, VALUE csid)
{
    VALUE name = rb_hash_aref(csid2name, csid);

    if (!NIL_P(name)) {
        return name;
    }
    Check_Type(csid, T_FIXNUM);
    if (have_OCINlsCharSetIdToName) {
        /* Oracle 9iR2 or upper */
        char buf[OCI_NLS_MAXBUFSZ];
        sword rv;

        rv = OCINlsCharSetIdToName(oci8_envhp, TO_ORATEXT(buf), sizeof(buf), FIX2INT(csid));
        if (rv != OCI_SUCCESS) {
            return Qnil;
        }
        name = rb_str_new2(buf);
    } else {
        /* Oracle 9iR1 or lower */
        cb_arg_t arg;
        arg.svcctx = oci8_get_svcctx(svc);
        arg.stmtp = NULL;
        arg.u.csid = FIX2INT(csid);
        name = rb_ensure(charset_id2name_func, (VALUE)&arg, ensure_func, (VALUE)&arg);
        if (NIL_P(name)) {
            return Qnil;
        }
    }
    OBJ_FREEZE(name);
    rb_hash_aset(csid2name, csid, name);
#ifdef HAVE_TYPE_RB_ENCODING
    rb_hash_aset(csname2id, name, csid);
#endif /* HAVE_TYPE_RB_ENCODING */
    return name;
}

/* convert chaset id to charset name by querying Oracle server.
 * This routine is used only when the Oracle client version is 9iR1 or lower.
 */
static VALUE charset_id2name_func(cb_arg_t *arg)
{
    char buf[OCI_NLS_MAXBUFSZ];
    ub2 buflen;
    OCIBind *bind1 = NULL;
    OCIBind *bind2 = NULL;
    sword rv;
    const char *sql = "BEGIN :name := nls_charset_name(:csid); END;";

    buflen = 0;
    rv = OCIHandleAlloc(oci8_envhp, (dvoid*)&arg->stmtp, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    oci_lc(OCIStmtPrepare(arg->stmtp, oci8_errhp, (text*)sql, strlen(sql), OCI_NTV_SYNTAX, OCI_DEFAULT));
    oci_lc(OCIBindByPos(arg->stmtp, &bind1, oci8_errhp, 1, buf, OCI_NLS_MAXBUFSZ, SQLT_CHR, NULL, &buflen, 0, 0, 0, OCI_DEFAULT));
    oci_lc(OCIBindByPos(arg->stmtp, &bind2, oci8_errhp, 2, (dvoid*)&arg->u.csid, sizeof(int), SQLT_INT, NULL, NULL, 0, 0, 0, OCI_DEFAULT));
    oci_lc(OCIStmtExecute_nb(arg->svcctx, arg->svcctx->base.hp.svc, arg->stmtp, oci8_errhp, 1, 0, NULL, NULL, OCI_DEFAULT));
    return rb_str_new(buf, buflen);
}

static VALUE ensure_func(cb_arg_t *arg)
{
    if (arg->stmtp != NULL) {
        OCIHandleFree(arg->stmtp, OCI_HTYPE_STMT);
    }
    return Qnil;
}

#ifdef HAVE_TYPE_RB_ENCODING

static VALUE oci8_charset_name2id(VALUE svc, VALUE name)
{
    VALUE csid = rb_hash_aref(csname2id, StringValue(name));

    if (!NIL_P(csid)) {
        return csid;
    }
    name = rb_funcall(name, id_upcase, 0);
    if (have_OCINlsCharSetNameToId) {
        /* Oracle 9iR2 or upper */
        ub2 rv;

        rv = OCINlsCharSetNameToId(oci8_envhp, RSTRING_ORATEXT(name));
        if (rv == 0) {
            return Qnil;
        }
        csid = INT2FIX(rv);
    } else {
        /* Oracle 9iR1 or lower */
        cb_arg_t arg;
        arg.svcctx = oci8_get_svcctx(svc);
        arg.stmtp = NULL;
        arg.u.name = name;
        csid = rb_ensure(charset_name2id_func, (VALUE)&arg, ensure_func, (VALUE)&arg);
        if (NIL_P(csid)) {
            return Qnil;
        }
    }
    rb_hash_aset(csid2name, csid, name);
    rb_hash_aset(csname2id, name, csid);
    return csid;
}

/* convert chaset name to charset id by querying Oracle server.
 * This routine is used only when the Oracle client version is 9iR1 or lower.
 */
static VALUE charset_name2id_func(cb_arg_t *arg)
{
    int csid;
    sb2 ind = 0; /* null indicator */
    OCIBind *bind1 = NULL;
    OCIBind *bind2 = NULL;
    sword rv;
    const char *sql = "BEGIN :csid := nls_charset_id(:name); END;";

    rv = OCIHandleAlloc(oci8_envhp, (dvoid*)&arg->stmtp, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    oci_lc(OCIStmtPrepare(arg->stmtp, oci8_errhp, (text*)sql, strlen(sql), OCI_NTV_SYNTAX, OCI_DEFAULT));
    oci_lc(OCIBindByPos(arg->stmtp, &bind1, oci8_errhp, 1, (dvoid*)&csid, sizeof(int), SQLT_INT, &ind, NULL, 0, 0, 0, OCI_DEFAULT));
    oci_lc(OCIBindByPos(arg->stmtp, &bind2, oci8_errhp, 2, RSTRING_PTR(arg->u.name), RSTRING_LEN(arg->u.name), SQLT_CHR, NULL, 0, 0, 0, 0, OCI_DEFAULT));
    oci_lc(OCIStmtExecute_nb(arg->svcctx, arg->svcctx->base.hp.svc, arg->stmtp, oci8_errhp, 1, 0, NULL, NULL, OCI_DEFAULT));
    return ind ? Qnil : INT2FIX(csid);
}

#endif /* HAVE_TYPE_RB_ENCODING */

void Init_oci8_encoding(VALUE cOCI8)
{
    csid2name = rb_hash_new();
    rb_global_variable(&csid2name);

#ifdef HAVE_TYPE_RB_ENCODING
    id_upcase = rb_intern("upcase");
    csname2id = rb_hash_new();
    rb_global_variable(&csname2id);
#endif /* HAVE_TYPE_RB_ENCODING */

#if 0
    rb_define_method(cOCI8, "charset_name2id", oci8_charset_name2id, 1);
    rb_define_method(cOCI8, "charset_id2name", oci8_charset_id2name, 1);
#endif
}
