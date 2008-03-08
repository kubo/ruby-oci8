/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * attr.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype)
{
    sb1 val;
    sword rv;

    rv = OCIAttrGet(base->hp.ptr, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype)
{
    ub2 val;
    sword rv;

    rv = OCIAttrGet(base->hp.ptr, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype)
{
    sb2 val;
    sword rv;

    rv = OCIAttrGet(base->hp.ptr, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype)
{
    ub4 val;
    sword rv;

    rv = OCIAttrGet(base->hp.ptr, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
#if SIZEOF_LONG > 4
    return LONG2FIX(val);
#else
    return ULONG2NUM(val);
#endif
}

VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype)
{
    text *val;
    ub4 size;
    sword rv;

    rv = OCIAttrGet(base->hp.ptr, base->type, &val, &size, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return rb_str_new(TO_CHARPTR(val), size);
}

#define MAX_ROWID_LEN 128

typedef struct {
    oci8_base_t *base;
    ub4 attrtype;
    OCIRowid *ridp;
    OCIStmt *stmtp;
} rowid_arg_t;

static VALUE get_rowid_attr(rowid_arg_t *arg)
{
    oci8_base_t *base = arg->base;
    ub4 attrtype = arg->attrtype;
    char buf[MAX_ROWID_LEN];
    ub2 buflen;
    sword rv;

	/* get a rowid descriptor from OCIHandle */
    rv = OCIDescriptorAlloc(oci8_envhp, (dvoid*)&arg->ridp, OCI_DTYPE_ROWID, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    rv = OCIAttrGet(base->hp.ptr, base->type, arg->ridp, 0, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    /* convert the rowid descriptor to a string. */
    if (have_OCIRowidToChar) {
        /* If OCIRowidToChar is available, use it. */
        buflen = MAX_ROWID_LEN;
        rv = OCIRowidToChar(arg->ridp, TO_ORATEXT(buf), &buflen, oci8_errhp);
        if (rv != OCI_SUCCESS) {
            oci8_raise(oci8_errhp, rv, NULL);
        }
    } else {
        /* If OCIRowidToChar is not available, convert it on
         * Oracle Server.
         */
        oci8_base_t *svc;
        OCIBind *bind1 = NULL;
        OCIBind *bind2 = NULL;
        sword rv;

        /* search a connection from the handle */
        svc = base;
        while (svc->type != OCI_HTYPE_SVCCTX) {
            svc = svc->parent;
            if (svc == NULL) {
                rb_raise(rb_eRuntimeError, "No connection is found!!");
            }
        }
        /*
         * equivalent code:
         *   cursor = conn.parse("BEGIN :1 := :2; END;")
         *   cursor.bind(1, nil, String, MAX_ROWID_LEN)
         *   cursor.bind(2, rowid, OCIRowid)
         *   cursor.exec()
         *   cursor[1] # => rowid's string representation
         */
#define ROWIDTOCHAR_SQL "BEGIN :1 := :2; END;"
        buflen = 0;
        rv = OCIHandleAlloc(oci8_envhp, (dvoid*)&arg->stmtp, OCI_HTYPE_STMT, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_env_raise(oci8_envhp, rv);
        }
        oci_lc(OCIStmtPrepare(arg->stmtp, oci8_errhp, (text*)ROWIDTOCHAR_SQL, strlen(ROWIDTOCHAR_SQL), OCI_NTV_SYNTAX, OCI_DEFAULT));
        oci_lc(OCIBindByPos(arg->stmtp, &bind1, oci8_errhp, 1, buf, MAX_ROWID_LEN, SQLT_STR, NULL, &buflen, 0, 0, 0, OCI_DEFAULT));
        oci_lc(OCIBindByPos(arg->stmtp, &bind2, oci8_errhp, 2, (dvoid*)&arg->ridp, sizeof(void*), SQLT_RDD, NULL, NULL, 0, 0, 0, OCI_DEFAULT));
        oci_lc(OCIStmtExecute_nb((oci8_svcctx_t*)svc, svc->hp.svc, arg->stmtp, oci8_errhp, 1, 0, NULL, NULL, OCI_DEFAULT));
    }
    return rb_str_new(buf, buflen);
}

static VALUE rowid_ensure(rowid_arg_t *arg)
{
    if (arg->stmtp != NULL) {
        OCIHandleFree(arg->stmtp, OCI_HTYPE_STMT);
    }
    if (arg->ridp != NULL) {
        OCIDescriptorFree(arg->ridp, OCI_DTYPE_ROWID);
    }
    return Qnil;
}

VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype)
{
    rowid_arg_t arg;
    arg.base = base;
    arg.attrtype = attrtype;
    arg.ridp = NULL;
    arg.stmtp = NULL;
    return rb_ensure(get_rowid_attr, (VALUE)&arg, rowid_ensure, (VALUE)&arg);
}
