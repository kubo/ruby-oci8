/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * transaction.c - part of ruby-oci8
 *         implement the methods of OCI8::Transaction
 *
 * Copyright (C) 2019 Kubo Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
#include "xa.h"

static VALUE cOCI8Trans;
static VALUE cOCI8Xid;

#define TO_TRANS(obj) oci8_check_typeddata((obj), &oci8_trans_data_type, 1)

static size_t xid_memsize(const void *ptr)
{
    return sizeof(XID);
}

static const rb_data_type_t xid_data_type = {
    "OCI8::Xid",
    {NULL, RUBY_DEFAULT_FREE, xid_memsize,},
};

static VALUE xid_alloc(VALUE klass)
{
    XID *xid;
    return TypedData_Make_Struct(klass, XID, &xid_data_type, xid);
}

static VALUE xid_initialize(VALUE self, VALUE format_id, VALUE gtrid, VALUE bqual)
{
    XID *xid = RTYPEDDATA_DATA(self);
    long fmtid = NUM2LONG(format_id);
    long gtrid_length;
    long bqual_length;

    /* check gtrid */
    StringValue(gtrid);
    gtrid_length = RSTRING_LEN(gtrid);
    if (gtrid_length < 1 || MAXGTRIDSIZE < gtrid_length) {
        rb_raise(rb_eArgError, "Invalid length of global transaction id: %ld", gtrid_length);
    }
    /* check bqual */
    StringValue(bqual);
    bqual_length = RSTRING_LEN(bqual);
    if (bqual_length < 1 || MAXBQUALSIZE < bqual_length) {
        rb_raise(rb_eArgError, "Invalid length of branch qualifier: %ld", bqual_length);
    }

    xid->formatID = fmtid;
    xid->gtrid_length = gtrid_length;
    xid->bqual_length = bqual_length;
    memcpy(xid->data, RSTRING_PTR(gtrid), gtrid_length);
    memcpy(xid->data + gtrid_length, RSTRING_PTR(bqual), bqual_length);
    return LONG2NUM(xid->formatID);
}

static VALUE xid_get_format_id(VALUE self)
{
    XID *xid = RTYPEDDATA_DATA(self);
    return LONG2NUM(xid->formatID);
}

static VALUE xid_get_gtrid(VALUE self)
{
    XID *xid = RTYPEDDATA_DATA(self);
    VALUE obj = rb_str_new(xid->data, xid->gtrid_length);
    if (OBJ_TAINTED(self)) {
        OBJ_TAINT(obj);
    }
    return obj;
}

static VALUE xid_get_bqual(VALUE self)
{
    XID *xid = RTYPEDDATA_DATA(self);
    VALUE obj = rb_str_new(xid->data + xid->gtrid_length, xid->bqual_length);
    if (OBJ_TAINTED(self)) {
        OBJ_TAINT(obj);
    }
    return obj;
}

static VALUE xid_inspect(VALUE self)
{
    XID *xid = RTYPEDDATA_DATA(self);
    char gtrid[128];
    char bqual[128];
    int i;
#define TO_HEX(n) ((n) < 9 ? (n) + '0' : (n) - 10 + 'a')

    for (i = 0; i < xid->gtrid_length; i++) {
        char x = xid->data[i];
        gtrid[i * 2 + 0] = TO_HEX((x >> 4) & 0x0F);
        gtrid[i * 2 + 1] = TO_HEX((x >> 0) & 0x0F);
    }
    for (i = 0; i < xid->bqual_length; i++) {
        char x = xid->data[xid->gtrid_length + i];
        bqual[i * 2 + 0] = TO_HEX((x >> 4) & 0x0F);
        bqual[i * 2 + 1] = TO_HEX((x >> 0) & 0x0F);
    }
    return rb_sprintf("<%s: %ld, %.*s, %.*s>",
                      rb_obj_classname(self), xid->formatID,
                      (int)xid->gtrid_length * 2, gtrid,
                      (int)xid->bqual_length * 2, bqual);
}

const oci8_handle_data_type_t oci8_trans_data_type = {
    {
        "OCI8::TransHandle",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    NULL,
    sizeof(oci8_base_t),
};

static VALUE oci8_trans_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_trans_data_type);
}

static VALUE oci8_trans_initialize(VALUE self)
{
    oci8_base_t *trans = TO_TRANS(self);
    sword rv = OCIHandleAlloc(oci8_envhp, &trans->hp.ptr, OCI_HTYPE_TRANS, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    trans->type = OCI_HTYPE_TRANS;
    return self;
}

static VALUE oci8_trans_get_name(VALUE self)
{
    oci8_base_t *trans = TO_TRANS(self);
    char *value;
    ub4 size = 0;
    chkerr(OCIAttrGet(trans->hp.ptr, trans->type, &value, &size, OCI_ATTR_TRANS_NAME, oci8_errhp));
    if (size == 0) {
        return Qnil;
    } else {
        return rb_str_new(value, size);
    }
}

static VALUE oci8_trans_get_timeout(VALUE self)
{
    oci8_base_t *trans = TO_TRANS(self);
    ub4 value;
    ub4 size = 0;
    chkerr(OCIAttrGet(trans->hp.ptr, trans->type, &value, &size, OCI_ATTR_TRANS_TIMEOUT, oci8_errhp));
    return UINT2NUM(value);
}

static VALUE oci8_trans_get_xid(VALUE self)
{
    oci8_base_t *trans = TO_TRANS(self);
    void **value;
    ub4 size = 0;
    XID *xid;
    VALUE obj;

    chkerr(OCIAttrGet(trans->hp.ptr, trans->type, &value, &size, OCI_ATTR_XID, oci8_errhp));
    obj = TypedData_Make_Struct(cOCI8Xid, XID, &xid_data_type, xid);
    memcpy(xid, value, sizeof(XID));
    return obj;
}

static VALUE oci8_trans_set_name(VALUE self, VALUE name)
{
    oci8_base_t *trans = TO_TRANS(self);
    void *val = NULL;
    ub4 size = 0;

    if (!NIL_P(name)) {
        SafeStringValue(name);
        val = RSTRING_PTR(name);
        size = RSTRING_LEN(name);
    }
    chkerr(OCIAttrSet(trans->hp.ptr, trans->type, val, size, OCI_ATTR_TRANS_NAME, oci8_errhp));
    return self;
}

static VALUE oci8_trans_set_timeout(VALUE self, VALUE val)
{
    oci8_base_t *trans = TO_TRANS(self);
    ub4 value = NUM2UINT(val);
    chkerr(OCIAttrSet(trans->hp.ptr, trans->type, &value, sizeof(value), OCI_ATTR_TRANS_TIMEOUT, oci8_errhp));
    return self;
}

static VALUE oci8_trans_set_xid(VALUE self, VALUE val)
{
    oci8_base_t *trans = TO_TRANS(self);
    XID *xid;

    if (!RTEST(rb_obj_is_kind_of(val, cOCI8Xid))) {
        rb_raise(rb_eTypeError, "expect OCI8::Xid but %s", rb_class2name(CLASS_OF(val)));
    }
    xid = RTYPEDDATA_DATA(val);
    chkerr(OCIAttrSet(trans->hp.ptr, trans->type, xid, sizeof(XID), OCI_ATTR_XID, oci8_errhp));
    return self;
}

void Init_oci8_transaction(VALUE cOCI8)
{
    cOCI8Xid = rb_define_class_under(cOCI8, "Xid", rb_cObject);
    rb_define_alloc_func(cOCI8Xid, xid_alloc);
    rb_define_method(cOCI8Xid, "initialize", xid_initialize, 3);
    rb_define_method(cOCI8Xid, "format_id", xid_get_format_id, 0);
    rb_define_method(cOCI8Xid, "gtrid", xid_get_gtrid, 0);
    rb_define_method(cOCI8Xid, "bqual", xid_get_bqual, 0);
    rb_define_method(cOCI8Xid, "inspect", xid_inspect, 0);

    cOCI8Trans = oci8_define_class_under(cOCI8, "TransHandle", &oci8_trans_data_type, oci8_trans_alloc);
    rb_define_method(cOCI8Trans, "initialize", oci8_trans_initialize, 0);
    rb_define_method(cOCI8Trans, "name", oci8_trans_get_name, 0);
    rb_define_method(cOCI8Trans, "timeout", oci8_trans_get_timeout, 0);
    rb_define_method(cOCI8Trans, "xid", oci8_trans_get_xid, 0);
    rb_define_method(cOCI8Trans, "name=", oci8_trans_set_name, 1);
    rb_define_method(cOCI8Trans, "timeout=", oci8_trans_set_timeout, 1);
    rb_define_method(cOCI8Trans, "xid=", oci8_trans_set_xid, 1);
}
