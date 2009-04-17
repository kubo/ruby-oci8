/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * metadata.c
 *
 * Copyright (C) 2006-2009 KUBO Takehiro <kubo@jiubao.org>
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
    ub1 is_implicit;
} oci8_metadata_t;

static void oci8_metadata_mark(oci8_base_t *base)
{
    oci8_metadata_t *md = (oci8_metadata_t *)base;
    if (base->parent != NULL)
        rb_gc_mark(base->parent->self);
    rb_gc_mark(md->svc);
}

VALUE oci8_metadata_create(OCIParam *parmhp, VALUE svc, VALUE parent)
{
    oci8_metadata_t *md;
    oci8_base_t *p;
    ub1 ptype;
    ub4 size;
    VALUE klass;
    VALUE obj;

    Check_Handle(parent, oci8_cOCIHandle, p);

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

    if (p->type == OCI_HTYPE_STMT) {
        md->is_implicit = 1;
    } else {
        md->is_implicit = 0;
    }
    oci8_link_to_parent(&md->base, p);
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, 0, FIX2INT(idx), oci8_errhp));
    return INT2FIX(value);
}

static VALUE metadata_get_ub4(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 value = 0;
    ub4 size = sizeof(value);

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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
    oci8_metadata_t *md = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    text *value;
    ub4 size;

    /* remote call sometimes? */
    oci_lc(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &value, &size, FIX2INT(idx), oci8_errhp));
    return rb_external_str_new_with_enc(TO_CHARPTR(value), size, oci8_encoding);
}

static VALUE metadata_get_oradate(VALUE self, VALUE idx)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 *value;
    ub4 size = 7;
    static VALUE cOraDate = Qnil;
    VALUE obj;

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
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

    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size >= 22) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect less than 22, but %d", size);
    }
    memset(&on, 0, sizeof(on));
    on.OCINumberPart[0] = size;
    memcpy(&on.OCINumberPart[1], value, size);
    return oci8_make_integer(&on, oci8_errhp);
}

static VALUE metadata_get_param(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIParam *value;
    ub4 size = sizeof(value);

    /* remote call? */
    oci_lc(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &value, &size, FIX2INT(idx), oci8_errhp));
    if (size != sizeof(OCIParam *)) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect %d, but %d", (sb4)sizeof(OCIParam *), size);
    }
    return oci8_metadata_create(value, md->svc, self);
}

static VALUE metadata_get_param_at(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = DATA_PTR(self);
    OCIParam *value;

    oci_lc(OCIParamGet(md->base.hp.ptr, md->base.type, oci8_errhp, (dvoid *)&value, FIX2INT(idx)));
    return oci8_metadata_create(value, md->svc, self);
}

static VALUE metadata_get_charset_name(VALUE self, VALUE charset_id)
{
    oci8_metadata_t *md = DATA_PTR(self);

    return oci8_charset_id2name(md->svc, charset_id);
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

static VALUE oci8_do_describe(VALUE self, void *objptr, ub4 objlen, ub1 objtype, VALUE klass, VALUE check_public)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    OCIParam *parmhp;
    VALUE type;
    VALUE obj;
    oci8_base_t *desc;

    /* make a describe handle object */
    obj = rb_obj_alloc(oci8_cOCIHandle);
    desc = DATA_PTR(obj);
    oci_lc(OCIHandleAlloc(oci8_envhp, (dvoid *)&desc->hp.dschp, OCI_HTYPE_DESCRIBE, 0, 0));
    desc->type = OCI_HTYPE_DESCRIBE;

    type = rb_hash_aref(class_to_ptype, klass);
    if (RTEST(check_public)) {
        sb4 val = -1;
        /* size of OCI_ATTR_DESC_PUBLIC is undocumented. */
        oci_lc(OCIAttrSet(desc->hp.dschp, OCI_HTYPE_DESCRIBE, &val, 0, OCI_ATTR_DESC_PUBLIC, oci8_errhp));
    }
    oci_lc(OCIDescribeAny_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, objptr, objlen,
                             objtype, OCI_DEFAULT, (ub1)FIX2INT(type), desc->hp.dschp));
    oci_lc(OCIAttrGet(desc->hp.dschp, OCI_HTYPE_DESCRIBE, &parmhp, 0, OCI_ATTR_PARAM, oci8_errhp));
    return oci8_metadata_create(parmhp, self, obj);
}

static VALUE oci8_describe(VALUE self, VALUE name, VALUE klass, VALUE check_public)
{
    OCI8SafeStringValue(name);
    if (RSTRING_LEN(name) == 0) {
        rb_raise(rb_eArgError, "empty string is set.");
    }
    return oci8_do_describe(self, RSTRING_PTR(name), RSTRING_LEN(name), OCI_OTYPE_NAME, klass, check_public);
}

static VALUE metadata_get_type_metadata(VALUE self, VALUE klass)
{
    oci8_metadata_t *md = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIRef *ref = NULL;

    /* remote call */
    oci_lc(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp));
    return oci8_do_describe(md->svc, ref, 0, OCI_OTYPE_REF, klass, Qfalse);
}

static VALUE metadata_get_tdo_id(VALUE self)
{
    oci8_metadata_t *md = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIRef *tdo_ref = NULL;
    void *tdo;

    oci_lc(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &tdo_ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp));
    if (tdo_ref == NULL)
        return Qnil;
    oci_lc(OCIObjectPin_nb(svcctx, oci8_envhp, oci8_errhp, tdo_ref, 0, OCI_PIN_ANY, OCI_DURATION_SESSION, OCI_LOCK_NONE, &tdo));
    oci_lc(OCIObjectUnpin(oci8_envhp, oci8_errhp, tdo));
#if SIZEOF_LONG == SIZEOF_VOIDP
    return ((VALUE)tdo | (VALUE)1);
#else
    return LL2NUM(((LONG_LONG)tdo) >>1);
#endif
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
    rb_define_method(cOCI8MetadataBase, "tdo_id", metadata_get_tdo_id, 0);
}
