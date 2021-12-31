/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * metadata.c
 *
 * Copyright (C) 2006-2016 Kubo Takehiro <kubo@jiubao.org>
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
static ID id_at_obj_link;

#define TO_METADATA(obj) ((oci8_metadata_t *)oci8_check_typeddata((obj), &oci8_metadata_base_data_type, 1))

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

static void oci8_metadata_free(oci8_base_t *base)
{
    base->type = 0; /* to prevent OCIDescriptorFree */
}

const oci8_handle_data_type_t oci8_metadata_base_data_type = {
    {
        "OCI8::Metadata::Base",
        {
            (RUBY_DATA_FUNC)oci8_metadata_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_metadata_free,
    sizeof(oci8_metadata_t),
};

VALUE oci8_metadata_create(OCIParam *parmhp, VALUE svc, VALUE parent)
{
    oci8_metadata_t *md;
    oci8_base_t *p = oci8_check_typeddata(parent, &oci8_handle_data_type, 1);
    ub1 ptype;
    ub4 size;
    VALUE klass;
    VALUE obj;

    chker2(OCIAttrGet(parmhp, OCI_DTYPE_PARAM, &ptype, &size, OCI_ATTR_PTYPE, oci8_errhp), p);
    klass = rb_hash_aref(ptype_to_class, INT2FIX(ptype));
    if (NIL_P(klass))
        rb_raise(rb_eRuntimeError, "unknown parameter type %d", ptype);
    obj = rb_obj_alloc(klass);
    if (!RTEST(rb_obj_is_kind_of(obj, cOCI8MetadataBase))) {
        rb_raise(rb_eRuntimeError, "invalid class in PTYPE_TO_CLASS");
    }
    md = TO_METADATA(obj);
    md->base.type = OCI_DTYPE_PARAM;
    md->base.hp.prm = parmhp;
    RB_OBJ_WRITE(obj, &md->svc, svc);

    if (p->type == OCI_HTYPE_STMT) {
        md->is_implicit = 1;
    } else {
        md->is_implicit = 0;
    }
    oci8_link_to_parent(&md->base, p);
    RB_OBJ_WRITTEN(obj, Qundef, p->self);
    return obj;
}

static VALUE metadata_s_register_ptype(VALUE klass, VALUE ptype)
{
    rb_hash_aset(ptype_to_class, ptype, klass);
    rb_hash_aset(class_to_ptype, klass, ptype);
    return Qnil;
}

/*
 * call-seq:
 *   __param(attr_type) -> metadata information or nil
 *
 * Gets the value of the attribute specified by +attr_type+
 * as an instance of an OCI8::Metadata::Base's subclass.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is not a
 * metadata, it causes a segmentation fault.
 */
static VALUE metadata_get_param(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = TO_METADATA(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIParam *value;
    ub4 size = sizeof(value);

    Check_Type(idx, T_FIXNUM);
    /* Is it remote call? */
    chker2(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &value, &size, FIX2INT(idx), oci8_errhp),
           &svcctx->base);
    if (size != sizeof(OCIParam *)) {
        rb_raise(rb_eRuntimeError, "Invalid attribute size. expect %d, but %d", (sb4)sizeof(OCIParam *), size);
    }
    if (value == NULL) {
        return Qnil;
    }
    return oci8_metadata_create(value, md->svc, self);
}

static VALUE metadata_get_param_at(VALUE self, VALUE idx)
{
    oci8_metadata_t *md = TO_METADATA(self);
    OCIParam *value;

    chker2(OCIParamGet(md->base.hp.ptr, md->base.type, oci8_errhp, (dvoid *)&value, FIX2INT(idx)),
           &md->base);
    return oci8_metadata_create(value, md->svc, self);
}

static VALUE metadata_get_con(VALUE self)
{
    oci8_metadata_t *md = TO_METADATA(self);
    return md->svc;
}

static VALUE metadata_is_implicit_p(VALUE self)
{
    oci8_metadata_t *md = TO_METADATA(self);
    return md->is_implicit ? Qtrue : Qfalse;
}

VALUE oci8_do_describe(VALUE self, void *objptr, ub4 objlen, ub1 objtype, VALUE klass, VALUE check_public)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    OCIParam *parmhp;
    VALUE type;
    VALUE obj;
    oci8_base_t *desc;
    int rv;

    /* make a describe handle object */
    obj = rb_obj_alloc(oci8_cOCIHandle);
    desc = DATA_PTR(obj);
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&desc->hp.dschp, OCI_HTYPE_DESCRIBE, 0, 0);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    desc->type = OCI_HTYPE_DESCRIBE;

    type = rb_hash_aref(class_to_ptype, klass);
    if (RTEST(check_public)) {
        sb4 val = -1;
        /* size of OCI_ATTR_DESC_PUBLIC is undocumented. */
        chker2(OCIAttrSet(desc->hp.dschp, OCI_HTYPE_DESCRIBE, &val, 0, OCI_ATTR_DESC_PUBLIC, oci8_errhp),
               &svcctx->base);
    }
    chker2(OCIDescribeAny_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, objptr, objlen,
                             objtype, OCI_DEFAULT, (ub1)FIX2INT(type), desc->hp.dschp),
           &svcctx->base);
    oci8_link_to_parent(desc, &svcctx->base);
    chker2(OCIAttrGet(desc->hp.dschp, OCI_HTYPE_DESCRIBE, &parmhp, 0, OCI_ATTR_PARAM, oci8_errhp),
           &svcctx->base);
    return oci8_metadata_create(parmhp, self, obj);
}

/*
 * call-seq:
 *   __describe(name, klass, check_public)
 *
 *  @param [String] name  object name
 *  @param [subclass of OCI8::Metadata::Base] klass
 *  @param [Boolean] check_public  +true+ to look up the object as a public synonym when
 *                                 the object does not exist in the current schema and
 *                                 the name includes no dots.
 *  @return [subclass of OCI8::Metadata::Base]
 *
 * @private
 */
static VALUE oci8_describe(VALUE self, VALUE name, VALUE klass, VALUE check_public)
{
    char *str;
    int idx, len;
    VALUE metadata;
    VALUE obj_link = Qnil;

    OCI8SafeStringValue(name);
    if (RSTRING_LEN(name) == 0) {
        rb_raise(rb_eArgError, "empty string is set.");
    }
    str = RSTRING_PTR(name);
    len = RSTRING_LENINT(name);
    for (idx = 0; idx < len; idx++) {
        if (str[idx] == '@') {
            obj_link = rb_enc_str_new(str + idx + 1, len - idx - 1, oci8_encoding);
            break;
        }
    }
    metadata = oci8_do_describe(self, str, len, OCI_OTYPE_NAME, klass, check_public);
    rb_ivar_set(metadata, id_at_obj_link, obj_link);
    return metadata;
}

static VALUE metadata_get_type_metadata(VALUE self, VALUE klass)
{
    oci8_metadata_t *md = TO_METADATA(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIRef *ref = NULL;

    /* remote call */
    chker2(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp),
           &svcctx->base);
    return oci8_do_describe(md->svc, ref, 0, OCI_OTYPE_REF, klass, Qfalse);
}

static VALUE metadata_get_tdo_id(VALUE self)
{
    oci8_metadata_t *md = TO_METADATA(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(md->svc);
    OCIRef *tdo_ref = NULL;
    void *tdo;

    chker2(OCIAttrGet_nb(svcctx, md->base.hp.ptr, md->base.type, &tdo_ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp),
           &svcctx->base);
    if (tdo_ref == NULL)
        return Qnil;
    chker2(OCIObjectPin_nb(svcctx, oci8_envhp, oci8_errhp, tdo_ref, 0, OCI_PIN_ANY, OCI_DURATION_SESSION, OCI_LOCK_NONE, &tdo),
           &svcctx->base);
    chker2(OCIObjectUnpin(oci8_envhp, oci8_errhp, tdo),
           &svcctx->base);
#if SIZEOF_LONG == SIZEOF_VOIDP
    return ((VALUE)tdo | (VALUE)1);
#else
    return LL2NUM(((LONG_LONG)tdo) >>1);
#endif
}

static VALUE oci8_metadata_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_metadata_base_data_type);
}

void Init_oci8_metadata(VALUE cOCI8)
{
    mOCI8Metadata = rb_define_module_under(cOCI8, "Metadata");
    cOCI8MetadataBase = oci8_define_class_under(mOCI8Metadata, "Base", &oci8_metadata_base_data_type, oci8_metadata_alloc);
    rb_global_variable(&ptype_to_class);
    rb_global_variable(&class_to_ptype);
    ptype_to_class = rb_hash_new();
    class_to_ptype = rb_hash_new();
    id_at_obj_link = rb_intern("@obj_link");

    rb_define_singleton_method(cOCI8MetadataBase, "register_ptype", metadata_s_register_ptype, 1);
    rb_define_private_method(cOCI8MetadataBase, "__param", metadata_get_param, 1);
    rb_define_private_method(cOCI8MetadataBase, "__param_at", metadata_get_param_at, 1);
    rb_define_private_method(cOCI8MetadataBase, "__con", metadata_get_con, 0);
    rb_define_private_method(cOCI8MetadataBase, "__is_implicit?", metadata_is_implicit_p, 0);

    rb_define_private_method(cOCI8, "__describe", oci8_describe, 3);
    rb_define_private_method(cOCI8MetadataBase, "__type_metadata", metadata_get_type_metadata, 1);
    rb_define_method(cOCI8MetadataBase, "tdo_id", metadata_get_tdo_id, 0);

    rb_define_class_under(mOCI8Metadata, "Type", cOCI8MetadataBase);
}
