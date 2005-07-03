/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * attr.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype)
{
    sb1 val;
    sword rv;

    rv = OCIAttrGet(base->hp, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype)
{
    ub2 val;
    sword rv;

    rv = OCIAttrGet(base->hp, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype)
{
    sb2 val;
    sword rv;

    rv = OCIAttrGet(base->hp, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(val);
}

VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype)
{
    ub4 val;
    sword rv;

    rv = OCIAttrGet(base->hp, base->type, &val, NULL, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
#if SIZEOF_LONG > 4
    return INT2FIX(val);
#else
    return UINT2NUM(val);
#endif
}

VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype)
{
    text *val;
    ub4 size;
    sword rv;

    rv = OCIAttrGet(base->hp, base->type, &val, &size, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return rb_str_new(val, size);
}
