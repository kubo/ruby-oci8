/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * attr.c
 *
 * Copyright (C) 2002-2012 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype, OCIStmt *stmtp)
{
    ub2 val;

    chker3(OCIAttrGet(base->hp.ptr, base->type, &val, NULL, attrtype, oci8_errhp),
           base, stmtp);
    return INT2FIX(val);
}

#define MAX_ROWID_LEN 128

typedef struct {
    oci8_base_t *base;
    OCIStmt *stmtp;
    ub4 attrtype;
    OCIRowid *ridp;
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
    chker3(OCIAttrGet(base->hp.ptr, base->type, arg->ridp, 0, attrtype, oci8_errhp),
           base, arg->stmtp);
    /* convert the rowid descriptor to a string. */
    buflen = MAX_ROWID_LEN;
    chker3(OCIRowidToChar(arg->ridp, TO_ORATEXT(buf), &buflen, oci8_errhp),
           base, arg->stmtp);
    return rb_external_str_new_with_enc(buf, buflen, rb_usascii_encoding());
}

static VALUE rowid_ensure(rowid_arg_t *arg)
{
    if (arg->ridp != NULL) {
        OCIDescriptorFree(arg->ridp, OCI_DTYPE_ROWID);
    }
    return Qnil;
}

VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype, OCIStmt *stmtp)
{
    rowid_arg_t arg;
    arg.base = base;
    arg.stmtp = stmtp;
    arg.attrtype = attrtype;
    arg.ridp = NULL;
    return rb_ensure(get_rowid_attr, (VALUE)&arg, rowid_ensure, (VALUE)&arg);
}
