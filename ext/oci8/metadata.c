/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * metadata.c
 * 
 * Copyright (C) 2006-2007 KUBO Takehiro <kubo@jiubao.org>
 *
 * implement private methods of classes in OCI8::Metadata module.
 *
 * public methods are implemented by oci8/metadata.rb.
 */
#include "oci8.h"

static VALUE mOCI8Metadata;
VALUE cOCI8MetadataBase;
static VALUE ptype_to_class;
static VALUE class_to_ptype;

typedef struct {
    oci8_base_t base;
    VALUE svc;
    VALUE desc;
    ub1 is_implicit;
} oci8_metadata_t;

static void oci8_metadata_mark(oci8_base_t *base)
{
    oci8_metadata_t *md = (oci8_metadata_t *)base;
    rb_gc_mark(md->svc);
    rb_gc_mark(md->desc);
}

VALUE oci8_metadata_create(OCIParam *parmhp, VALUE svc, VALUE desc)
{
    oci8_metadata_t *md;
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
    md = DATA_PTR(obj);
    md->base.type = OCI_DTYPE_PARAM;
    md->base.hp.prm = parmhp;
    md->svc = svc;
    md->desc = desc;
    md->is_implicit = RTEST(desc) ? 0 : 1;
    oci8_link_to_parent(&md->base, (oci8_base_t*)DATA_PTR(svc));
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
    ub1 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 1) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 1, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_ub2(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 2) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 2, but %d", size);
    }
    return INT2FIX(value);
}

/* get ub2 without size check. */
static VALUE metadata_get_ub2_nc(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value = 0;

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, 0, FIX2INT(idx), oci8_errhp));
    return INT2FIX(value);
}

static VALUE metadata_get_ub4(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
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
    sb1 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 1) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 1, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_sb2(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    sb2 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != 2) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect 2, but %d", size);
    }
    return INT2FIX(value);
}

static VALUE metadata_get_sb4(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    sb4 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    return rb_str_new(TO_CHARPTR(value), size);
}

static VALUE metadata_get_oradate(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 *value;
    ub4 size = 7;
    static VALUE cOraDate = Qnil;
    VALUE obj;

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
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
    OCINumber on;

    oci_lc(OCIAttrGet(base->hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size >= 22) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect less than 22, but %d", size);
    }
    memset(&on, 0, sizeof(on));
    on.OCINumberPart[0] = size;
    memcpy(&on.OCINumberPart[1], value, size);
    return oci8_make_integer(&on);
}

static VALUE metadata_get_param(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = DATA_PTR(self);
    OCIParam *value;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(md->base.hp.ptr, OCI_DTYPE_PARAM, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != sizeof(OCIParam *)) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect %d, but %d", sizeof(OCIParam *), size);
    }
    return oci8_metadata_create(value, md->svc, md->desc);
}

static VALUE metadata_get_param_at(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = DATA_PTR(self);
    OCIParam *value;

    oci_lc(OCIParamGet(md->base.hp.ptr, OCI_DTYPE_PARAM, oci8_errhp, (dvoid *)&value, FIX2INT(idx)));
    return oci8_metadata_create(value, md->svc, md->desc);
}

static VALUE metadata_get_charset_name(VALUE self, VALUE charset_id)
{
    char buf[OCI_NLS_MAXBUFSZ];
    sword rv;

    Check_Type(charset_id, T_FIXNUM);
    rv = OCINlsCharSetIdToName(oci8_envhp, TO_ORATEXT(buf), sizeof(buf), FIX2INT(charset_id));
    if (rv != OCI_SUCCESS) {
        return Qnil;
    }
    return rb_str_new2(buf);
}

static VALUE metadata_get_con(VALUE self)
{
    oci8_metadata_t *md = DATA_PTR(self);
    return md->svc;
}

static VALUE metadata_is_implicit_p(VALUE self)
{
    oci8_metadata_t *md = DATA_PTR(self);
    return md->is_implicit ? Qtrue : Qfalse;
}

static void oci8_desc_free(OCIDescribe *dschp)
{
    if (dschp != NULL)
        OCIHandleFree(dschp, OCI_HTYPE_DESCRIBE);
}

static VALUE oci8_do_describe(VALUE self, void *objptr, ub4 objlen, ub1 objtype, VALUE klass, VALUE check_public)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    OCIDescribe *dschp;
    OCIParam *parmhp;
    VALUE type;
    sword rv;
    VALUE desc;

    type = rb_hash_aref(class_to_ptype, klass);
    oci_lc(OCIHandleAlloc(oci8_envhp, (dvoid *)&dschp, OCI_HTYPE_DESCRIBE, 0, 0));
    /* dschp is freed when GC runs. */
    desc = Data_Wrap_Struct(rb_cObject, NULL, oci8_desc_free, dschp);
    if (RTEST(check_public)) {
        sb4 val = -1;
        /* size of OCI_ATTR_DESC_PUBLIC is undocumented. */
        oci_lc(OCIAttrSet(dschp, OCI_HTYPE_DESCRIBE, &val, 0, OCI_ATTR_DESC_PUBLIC, oci8_errhp));
    }
    oci_rc2(rv, svcctx, OCIDescribeAny(svcctx->base.hp.svc, oci8_errhp, objptr, objlen,
                                       objtype, OCI_DEFAULT, FIX2INT(type), dschp));
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

static VALUE oci8_describe(VALUE self, VALUE name, VALUE klass, VALUE check_public)
{
    StringValue(name);
    if (RSTRING_LEN(name) == 0) {
        rb_raise(rb_eArgError, "empty string is set.");
    }
    return oci8_do_describe(self, RSTRING_PTR(name), RSTRING_LEN(name), OCI_OTYPE_NAME, klass, check_public);
}

static VALUE metadata_get_type_metadata(VALUE self, VALUE klass)
{
    oci8_metadata_t *md = DATA_PTR(self);
    OCIRef *ref = NULL;

    oci_lc(OCIAttrGet(md->base.hp.ptr, OCI_DTYPE_PARAM, &ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp));
    return oci8_do_describe(md->svc, ref, 0, OCI_OTYPE_REF, klass, Qfalse);
}

oci8_base_class_t oci8_metadata_class = {
    oci8_metadata_mark,
    NULL,
    sizeof(oci8_metadata_t),
};

void Init_oci8_metadata(VALUE cOCI8)
{
    mOCI8Metadata = rb_define_module_under(cOCI8, "Metadata");
    cOCI8MetadataBase = oci8_define_class_under(mOCI8Metadata, "Base", &oci8_metadata_class);
    ptype_to_class = rb_hash_new();
    class_to_ptype = rb_hash_new();
    rb_global_variable(&ptype_to_class);
    rb_global_variable(&class_to_ptype);

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
    rb_define_private_method(cOCI8MetadataBase, "__charset_name", metadata_get_charset_name, 1);
    rb_define_private_method(cOCI8MetadataBase, "__con", metadata_get_con, 0);
    rb_define_private_method(cOCI8MetadataBase, "__is_implicit?", metadata_is_implicit_p, 0);

    rb_define_private_method(cOCI8, "__describe", oci8_describe, 3);
    rb_define_private_method(cOCI8MetadataBase, "__type_metadata", metadata_get_type_metadata, 1);
}
