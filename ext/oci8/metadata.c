/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * metadata.c
 * 
 * Copyright (C) 2006 KUBO Takehiro <kubo@jiubao.org>
 *
 * implement private methods of classes in OCI8::Metadata module.
 *
 * public methods are implemented by oci8/metadata.rb.
 */
#include "oci8.h"

static VALUE mOCI8Metadata;
static VALUE cOCI8MetadataBase;
static VALUE ptype_to_class;
static VALUE class_to_ptype;
static ID id_at_is_implicit;
static ID id_at_con;
static ID id___desc__;

VALUE oci8_metadata_create(OCIParam *parmhp, VALUE svc, VALUE desc)
{
    oci8_base_t *base;
    ub1 ptype;
    ub4 size;
    VALUE klass;
    VALUE obj;

    oci_lc(OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &ptype, &size, OCI_ATTR_PTYPE, oci8_errhp));
    klass = rb_hash_aref(ptype_to_class, INT2FIX(ptype));
    if (NIL_P(klass))
        rb_raise(rb_eRuntimeError, "unknown parameter type %d", ptype);
    obj = rb_obj_alloc(klass);
    if (!RTEST(rb_obj_is_kind_of(obj, cOCI8MetadataBase))) {
        rb_raise(rb_eRuntimeError, "invalid class in PTYPE_TO_CLASS");
    }
    base = DATA_PTR(obj);
    base->hp = parmhp;
    rb_ivar_set(obj, id_at_is_implicit, RTEST(desc) ? Qfalse : Qtrue);
    rb_ivar_set(obj, id_at_con, svc);
    rb_ivar_set(obj, id___desc__, desc);
    return obj;
}

static VALUE metadata_s_register_ptype(VALUE klass, VALUE ptype)
{
    rb_hash_aset(ptype_to_class, ptype, klass);
    rb_hash_aset(class_to_ptype, klass, ptype);
    return Qnil;
}

static VALUE metadata_get_ub1(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 1) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 1, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_ub2(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 2) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 2, but %d", size);
    }
    return INT2FIX(value);
}

/* get ub2 without size check. */
static VALUE metadata_get_ub2_nc(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value;

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, 0, FIX2INT(idx), oci8_errhp));
    return INT2FIX(value);
}

static VALUE metadata_get_ub4(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 4) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 4, but %d", size);
    }
#if SIZEOF_LONG > 4
    return INT2FIX(value);
#else
    return UINT2NUM(value);
#endif
}

static VALUE metadata_get_sb1(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    sb1 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 1) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 1, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_sb2(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    sb2 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 2) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 2, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_sb4(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    sb4 value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 4) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 4, but %d", size);
    }
#if SIZEOF_LONG > 4
    return INT2FIX(value);
#else
    return INT2NUM(value);
#endif
}

static VALUE metadata_get_text(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    text *value;
    ub4 size;

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    return rb_str_new(value, size);
}

static VALUE metadata_get_oradate(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 *value;
    ub4 size = 7;
    static VALUE cOraDate = Qnil;
    VALUE obj;

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 7) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 7, but %d", size);
    }
    if (NIL_P(cOraDate))
        cOraDate = rb_eval_string("OraDate");
    obj = rb_funcall(cOraDate, oci8_id_new, 0);
    memcpy(DATA_PTR(obj), value, 7);
    return obj;
}

static VALUE metadata_get_oraint(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 *value;
    ub4 size = 21;
    unsigned char buf[ORA_NUMBER_BUF_SIZE];
    size_t len;

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size >= 22) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect less than 22, but %d", size);
    }
    ora_number_to_str(buf, &len, (ora_number_t*)value, size);
    return rb_cstr2inum(buf, 10);
}

static VALUE metadata_get_param(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    OCIParam *value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != sizeof(OCIParam *)) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect %d, but %d", sizeof(OCIParam *), size);
    }
    return oci8_metadata_create(value, rb_ivar_get(self, id_at_con), rb_ivar_get(self, id___desc__));
}

static VALUE metadata_get_param_at(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    OCIParam *value;

    oci_lc(OCIParamGet(base->hp, OCI_DTYPE_PARAM, oci8_errhp, (dvoid *)&value, FIX2INT(idx)));
    return oci8_metadata_create(value, rb_ivar_get(self, id_at_con), rb_ivar_get(self, id___desc__));
}

static void oci8_desc_free(OCIDescribe *dschp)
{
    if (dschp != NULL)
        OCIHandleFree(dschp, OCI_HTYPE_DESCRIBE);
}

static VALUE oci8_describe(VALUE self, VALUE name, VALUE klass, VALUE check_public)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    OCIDescribe *dschp;
    OCIParam *parmhp;
    VALUE type;
    sword rv;
    VALUE desc;

    StringValue(name);
    if (RSTRING(name)->len == 0) {
        rb_raise(rb_eArgError, "empty string is set.");
    }

    type = rb_hash_aref(class_to_ptype, klass);
    oci_lc(OCIHandleAlloc(oci8_envhp, (dvoid *)&dschp, OCI_HTYPE_DESCRIBE, 0, 0));
    /* dschp is freed when GC runs. */
    desc = Data_Wrap_Struct(rb_cObject, NULL, oci8_desc_free, dschp);
    if (RTEST(check_public)) {
        sb4 val = -1;
        /* size of OCI_ATTR_DESC_PUBLIC is undocumented. */
        oci_lc(OCIAttrSet(dschp, OCI_HTYPE_DESCRIBE, &val, 0, OCI_ATTR_DESC_PUBLIC, oci8_errhp));
    }
    oci_rc2(rv, svcctx, OCIDescribeAny(svcctx->base.hp, oci8_errhp, 
                                       RSTRING(name)->ptr, RSTRING(name)->len,
                                       OCI_OTYPE_NAME, OCI_DEFAULT, FIX2INT(type),
                                       dschp));
    if (rv == OCI_ERROR) {
        if (oci8_get_error_code(oci8_errhp) == 4043) {
            /* ORA-04043: object xxxx does not exist */
            OCIHandleFree(dschp, OCI_HTYPE_DESCRIBE);
            DATA_PTR(desc) = NULL;
            return Qnil;
        }
    }
    if (rv != OCI_SUCCESS) {
        OCIHandleFree(dschp, OCI_HTYPE_DESCRIBE);
        DATA_PTR(desc) = NULL;
        oci8_raise(oci8_errhp, rv, NULL);
    }
    rv = OCIAttrGet(dschp, OCI_HTYPE_DESCRIBE, &parmhp, 0, OCI_ATTR_PARAM, oci8_errhp);
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return oci8_metadata_create(parmhp, self, desc);
}

void Init_oci8_metadata(VALUE cOCI8)
{
    mOCI8Metadata = rb_define_module_under(cOCI8, "Metadata");
    cOCI8MetadataBase = oci8_define_class_under(mOCI8Metadata, "Base", &oci8_base_class);
    ptype_to_class = rb_hash_new();
    class_to_ptype = rb_hash_new();
    rb_global_variable(&ptype_to_class);
    rb_global_variable(&class_to_ptype);
    id_at_is_implicit = rb_intern("@is_implicit");
    id_at_con = rb_intern("@con");
    id___desc__ = rb_intern("__desc__");

    rb_define_singleton_method(cOCI8MetadataBase, "register_ptype", metadata_s_register_ptype, 1);
    rb_define_private_method(cOCI8MetadataBase, "__ub1", metadata_get_ub1, 1);
    rb_define_private_method(cOCI8MetadataBase, "__ub2", metadata_get_ub2, 1);
    rb_define_private_method(cOCI8MetadataBase, "__ub2_nc", metadata_get_ub2_nc, 1);
    rb_define_private_method(cOCI8MetadataBase, "__ub4", metadata_get_ub4, 1);
    rb_define_private_method(cOCI8MetadataBase, "__sb1", metadata_get_sb1, 1);
    rb_define_private_method(cOCI8MetadataBase, "__sb2", metadata_get_sb2, 1);
    rb_define_private_method(cOCI8MetadataBase, "__sb4", metadata_get_sb4, 1);
    rb_define_private_method(cOCI8MetadataBase, "__text", metadata_get_text, 1);
    rb_define_private_method(cOCI8MetadataBase, "__oradate", metadata_get_oradate, 1);
    rb_define_private_method(cOCI8MetadataBase, "__oraint", metadata_get_oraint, 1);
    rb_define_private_method(cOCI8MetadataBase, "__param", metadata_get_param, 1);
    rb_define_private_method(cOCI8MetadataBase, "__param_at", metadata_get_param_at, 1);

    rb_define_private_method(cOCI8, "__describe", oci8_describe, 3);
}
