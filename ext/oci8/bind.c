/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * bind.c
 *
 * Copyright (C) 2002-2011 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

#ifndef OCI_ATTR_MAXCHAR_SIZE
#define OCI_ATTR_MAXCHAR_SIZE 164
#endif

static ID id_bind_type;
static VALUE sym_length;
static VALUE sym_length_semantics;
static VALUE sym_char;
static VALUE sym_nchar;

static VALUE cOCI8BindTypeBase;

typedef struct {
    oci8_bind_t obind;
    sb4 bytelen;
    sb4 charlen;
    ub1 csfrm;
} oci8_bind_string_t;

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;
    return rb_external_str_new_with_enc(vstr->buf, vstr->size, oci8_encoding);
}

static void bind_string_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;

    OCI8StringValue(val);
    if (RSTRING_LEN(val) > obs->bytelen) {
        rb_raise(rb_eArgError, "too long String to set. (%ld for %d)", RSTRING_LEN(val), obs->bytelen);
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LEN(val);
}

static void bind_string_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE param)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    VALUE length;
    VALUE length_semantics;
    VALUE nchar;
    sb4 sz;

    Check_Type(param, T_HASH);
    length = rb_hash_aref(param, sym_length);
    length_semantics = rb_hash_aref(param, sym_length_semantics);
    nchar = rb_hash_aref(param, sym_nchar);

    sz = NUM2INT(length);
    if (sz < 0) {
        rb_raise(rb_eArgError, "invalid bind length %d", sz);
    }
    if (length_semantics == sym_char) {
        /* character semantics */
        obs->charlen = sz;
        obs->bytelen = sz = sz * oci8_nls_ratio;
        if (oci8_nls_ratio == 1) {
            /* sz must be bigger than charlen to suppress ORA-06502.
             * I don't know the reason...
             */
            sz *= 2;
        }
    } else {
        /* byte semantics */
        obs->bytelen = sz;
        obs->charlen = 0;
    }
    if (RTEST(nchar)) {
        obs->csfrm = SQLCS_NCHAR; /* bind as NCHAR/NVARCHAR2 */
    } else {
        obs->csfrm = SQLCS_IMPLICIT; /* bind as CHAR/VARCHAR2 */
    }
    if (sz == 0) {
        sz = 1; /* to avoid ORA-01459. */
    }
    sz += sizeof(sb4);
    obind->value_sz = sz;
    obind->alloc_sz = (sz + (sizeof(sb4) - 1)) & ~(sizeof(sb4) - 1);
}

static void bind_string_post_bind_hook(oci8_bind_t *obind)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;

    if (obs->charlen != 0) {
        chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&obs->charlen, 0, OCI_ATTR_MAXCHAR_SIZE, oci8_errhp),
               &obind->base);
    }
    chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&obs->csfrm, 0, OCI_ATTR_CHARSET_FORM, oci8_errhp),
           &obind->base);
}

static const oci8_bind_vtable_t bind_string_vtable = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_string_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    NULL,
    NULL,
    SQLT_LVC,
    bind_string_post_bind_hook,
};

/*
 * bind_raw
 */
static VALUE bind_raw_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;
    return rb_str_new(vstr->buf, vstr->size);
}

static void bind_raw_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;

    StringValue(val);
    if (RSTRING_LEN(val) > obs->bytelen) {
        rb_raise(rb_eArgError, "too long String to set. (%ld for %d)", RSTRING_LEN(val), obs->bytelen);
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LEN(val);
}

static const oci8_bind_vtable_t bind_raw_vtable = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_string_t)
    },
    bind_raw_get,
    bind_raw_set,
    bind_string_init,
    NULL,
    NULL,
    SQLT_LVB
};

/*
 * bind_binary_double
 */
static VALUE bind_binary_double_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return rb_float_new(*(double*)data);
}

static void bind_binary_double_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    /* val is converted to Float if it isn't Float. */
    *(double*)data = RFLOAT_VALUE(rb_Float(val));
}

static void bind_binary_double_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(double);
    obind->alloc_sz = sizeof(double);
}

#ifndef SQLT_BDOUBLE
#define SQLT_BDOUBLE 22
#endif
static const oci8_bind_vtable_t bind_binary_double_vtable = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_binary_double_get,
    bind_binary_double_set,
    bind_binary_double_init,
    NULL,
    NULL,
    SQLT_BDOUBLE
};

static VALUE oci8_bind_get(VALUE self)
{
    oci8_bind_t *obind = DATA_PTR(self);
    const oci8_bind_vtable_t *vptr = (const oci8_bind_vtable_t *)obind->base.vptr;
    ub4 idx = obind->curar_idx;
    void **null_structp = NULL;

    if (NIL_P(obind->tdo)) {
        if (obind->u.inds[idx] != 0)
            return Qnil;
    }
    return vptr->get(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp);
}

VALUE oci8_bind_get_data(VALUE self)
{
    oci8_bind_t *obind = DATA_PTR(self);

    if (obind->maxar_sz == 0) {
        obind->curar_idx = 0;
        return rb_funcall(self, oci8_id_get, 0);
    } else {
        volatile VALUE ary = rb_ary_new2(obind->curar_sz);
        ub4 idx;

        for (idx = 0; idx < obind->curar_sz; idx++) {
            obind->curar_idx = idx;
            rb_ary_store(ary, idx, rb_funcall(self, oci8_id_get, 0));
        }
        return ary;
    }
}

static VALUE oci8_bind_set(VALUE self, VALUE val)
{
    oci8_bind_t *obind = DATA_PTR(self);
    const oci8_bind_vtable_t *vptr = (const oci8_bind_vtable_t *)obind->base.vptr;
    ub4 idx = obind->curar_idx;

    if (NIL_P(val)) {
        if (NIL_P(obind->tdo)) {
            obind->u.inds[idx] = -1;
        } else {
            *(OCIInd*)obind->u.null_structs[idx] = -1;
        }
    } else {
        void **null_structp = NULL;

        if (NIL_P(obind->tdo)) {
            null_structp = NULL;
            obind->u.inds[idx] = 0;
        } else {
            null_structp = &obind->u.null_structs[idx];
            *(OCIInd*)obind->u.null_structs[idx] = 0;
        }
        vptr->set(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp, val);
    }
    return self;
}

void oci8_bind_set_data(VALUE self, VALUE val)
{
    oci8_bind_t *obind = DATA_PTR(self);

    if (obind->maxar_sz == 0) {
        obind->curar_idx = 0;
        rb_funcall(self, oci8_id_set, 1, val);
    } else {
        ub4 size;
        ub4 idx;
        Check_Type(val, T_ARRAY);

        size = RARRAY_LEN(val);
        if (size > obind->maxar_sz) {
            rb_raise(rb_eRuntimeError, "over the max array size");
        }
        for (idx = 0; idx < size; idx++) {
            obind->curar_idx = idx;
            rb_funcall(self, oci8_id_set, 1, RARRAY_PTR(val)[idx]);
        }
        obind->curar_sz = size;
    }
}

static VALUE oci8_bind_initialize(VALUE self, VALUE svc, VALUE val, VALUE length, VALUE max_array_size)
{
    oci8_bind_t *obind = DATA_PTR(self);
    const oci8_bind_vtable_t *bind_class = (const oci8_bind_vtable_t *)obind->base.vptr;
    ub4 cnt = 1;

    obind->tdo = Qnil;
    obind->maxar_sz = NIL_P(max_array_size) ? 0 : NUM2UINT(max_array_size);
    obind->curar_sz = 0;
    if (obind->maxar_sz > 0)
        cnt = obind->maxar_sz;
    bind_class->init(obind, svc, val, length);
    if (obind->alloc_sz > 0) {
        obind->valuep = xmalloc(obind->alloc_sz * cnt);
        memset(obind->valuep, 0, obind->alloc_sz * cnt);
    } else {
        obind->valuep = NULL;
    }
    if (NIL_P(obind->tdo)) {
        obind->u.inds = xmalloc(sizeof(sb2) * cnt);
        memset(obind->u.inds, -1, sizeof(sb2) * cnt);
    } else {
        obind->u.null_structs = xmalloc(sizeof(void *) * cnt);
        memset(obind->u.null_structs, 0, sizeof(void *) * cnt);
    }
    if (bind_class->init_elem != NULL) {
        bind_class->init_elem(obind, svc);
    }
    if (!NIL_P(val)) {
        oci8_bind_set_data(self, val);
    }
    return Qnil;
}

void oci8_bind_free(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    if (obind->valuep != NULL) {
        xfree(obind->valuep);
        obind->valuep = NULL;
    }
    if (obind->u.inds != NULL) {
        xfree(obind->u.inds);
        obind->u.inds = NULL;
    }
}

void oci8_bind_hp_obj_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;

    if (oho != NULL) {
        ub4 idx = 0;

        do {
            rb_gc_mark(oho[idx].obj);
        } while (++idx < obind->maxar_sz);
    }
}

void Init_oci8_bind(VALUE klass)
{
    cOCI8BindTypeBase = klass;
    id_bind_type = rb_intern("bind_type");
    sym_length = ID2SYM(rb_intern("length"));
    sym_length_semantics = ID2SYM(rb_intern("length_semantics"));
    sym_char = ID2SYM(rb_intern("char"));
    sym_nchar = ID2SYM(rb_intern("nchar"));

    rb_define_method(cOCI8BindTypeBase, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCI8BindTypeBase, "get", oci8_bind_get, 0);
    rb_define_method(cOCI8BindTypeBase, "set", oci8_bind_set, 1);

    /* register primitive data types. */
    oci8_define_bind_class("String", &bind_string_vtable);
    oci8_define_bind_class("RAW", &bind_raw_vtable);
    if (oracle_client_version >= ORAVER_10_1) {
        oci8_define_bind_class("BinaryDouble", &bind_binary_double_vtable);
    }
}

oci8_bind_t *oci8_get_bind(VALUE obj)
{
    oci8_base_t *base;
    Check_Handle(obj, cOCI8BindTypeBase, base);
    return (oci8_bind_t *)base;
}
