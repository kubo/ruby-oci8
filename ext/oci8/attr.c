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
    return rb_external_str_new_with_enc(TO_CHARPTR(val), size, oci8_encoding);
}

#define MAX_ROWID_LEN 128

typedef struct {
    oci8_base_t *base;
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
        oci8_exec_sql_var_t define_var;
        oci8_exec_sql_var_t bind_var;

        /* search a connection from the handle */
        svc = base;
        while (svc->type != OCI_HTYPE_SVCCTX) {
            svc = svc->parent;
            if (svc == NULL) {
                rb_raise(rb_eRuntimeError, "No connection is found!!");
            }
        }
        /* :strval */
        define_var.valuep = buf;
        define_var.value_sz = sizeof(buf);
        define_var.dty = SQLT_CHR;
        define_var.indp = NULL;
        define_var.alenp = &buflen;
        /* :rowid */
        bind_var.valuep = &arg->ridp;
        bind_var.value_sz = sizeof(void *);
        bind_var.dty = SQLT_RDD;
        bind_var.indp = NULL;
        bind_var.alenp = NULL;
        /* convert the rowid descriptor to a string value by querying Oracle server. */
        oci8_exec_sql((oci8_svcctx_t*)svc, "SELECT :rid FROM dual", 1, &define_var, 1, &bind_var, 1);
        if (buflen == 0) {
            return Qnil;
        }
    }
    return rb_external_str_new_with_enc(buf, buflen, rb_usascii_encoding());
}

static VALUE rowid_ensure(rowid_arg_t *arg)
{
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
    return rb_ensure(get_rowid_attr, (VALUE)&arg, rowid_ensure, (VALUE)&arg);
}
