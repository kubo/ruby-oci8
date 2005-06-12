/*
 * attr.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

VALUE oci8_get_sb1_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    sb1 val;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIAttrGet(h->hp, h->type, &val, NULL, attrtype, h->errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(h->errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub2_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    ub2 val;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIAttrGet(h->hp, h->type, &val, NULL, attrtype, h->errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(h->errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_sb2_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    sb2 val;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIAttrGet(h->hp, h->type, &val, NULL, attrtype, h->errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(h->errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub4_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    ub4 val;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIAttrGet(h->hp, h->type, &val, NULL, attrtype, h->errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(h->errhp, rv, NULL);
#if SIZEOF_LONG > 4
    return INT2FIX(val);
#else
    return UINT2NUM(val);
#endif
}

VALUE oci8_get_string_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    text *val;
    ub4 size;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIAttrGet(h->hp, h->type, &val, &size, attrtype, h->errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(h->errhp, rv, NULL);
    return rb_str_new(val, size);
}

VALUE oci8_get_rowid_attr(VALUE self, ub4 attrtype)
{
    oci8_handle_t *h;
    OCIRowid *rowidhp;
    oci8_handle_t *rowidh;
    sword rv;

    Get_Handle(self, h); /* 0 */
    rv = OCIDescriptorAlloc(h->envh->hp, (void *)&rowidhp, OCI_DTYPE_ROWID, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(h->envh->hp, rv);
    }
    rv = OCIAttrGet(h->hp, h->type, rowidhp, 0, attrtype, h->errhp);
    if (rv != OCI_SUCCESS) {
        OCIDescriptorFree(rowidhp, OCI_DTYPE_ROWID);
        oci8_raise(h->errhp, rv, NULL);
    }
    rowidh = oci8_make_handle(OCI_DTYPE_ROWID, rowidhp, h->errhp, NULL, 0);
    return rowidh->self;
}
