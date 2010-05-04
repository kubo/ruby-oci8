/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * bind.c
 *
 * Copyright (C) 2002-2010 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

#ifndef OCI_ATTR_MAXCHAR_SIZE
#define OCI_ATTR_MAXCHAR_SIZE 164
#endif

static ID id_bind_type;
static VALUE sym_length;
static VALUE sym_char_semantics;

static VALUE cOCI8BindTypeBase;

typedef struct {
    oci8_bind_t obind;
    sb4 bytelen;
    sb4 charlen;
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
    VALUE char_semantics;
    sb4 sz;

    Check_Type(param, T_HASH);
    length = rb_hash_aref(param, sym_length);
    char_semantics = rb_hash_aref(param, sym_char_semantics);

    sz = NUM2INT(length);
    if (sz < 0) {
        rb_raise(rb_eArgError, "invalid bind length %d", sz);
    }
    if (RTEST(char_semantics)) {
        obs->charlen = sz;
        obs->bytelen = sz = sz * oci8_nls_ratio;
        if (oci8_nls_ratio == 1) {
            /* sz must be bigger than charlen to suppress ORA-06502.
             * I don't know the reason...
             */
            sz *= 2;
        }
    } else {
        obs->bytelen = sz;
        obs->charlen = 0;
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

    if (oracle_client_version >= ORAVER_9_0 && obs->charlen != 0) {
        oci_lc(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&obs->charlen, 0, OCI_ATTR_MAXCHAR_SIZE, oci8_errhp));
    }
}

static const oci8_bind_class_t bind_string_class = {
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

static const oci8_bind_class_t bind_raw_class = {
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
    NULL,
    NULL,
    SQLT_LVB
};

#ifdef USE_DYNAMIC_FETCH /* don't use DYNAMIC_FETCH. It doesn't work well... */
/*
 * bind_long
 */
typedef struct {
    VALUE obj;
    ub4 alen;
    char buf[1];
} bind_long_t;
#define bind_long_offset ((size_t)((bind_long_t*)0)->buf)

static void bind_long_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t*)base;
    ub4 idx = 0;

    if (obind->valuep == NULL)
        return;
    do {
        bind_long_t *bl = (bind_long_t *)((char*)obind->valuep + obind->alloc_sz * idx);
        rb_gc_mark(bl->obj);
    } while (++idx < obind->maxar_sz);
}

static VALUE bind_long_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    bind_long_t *bl = (bind_long_t *)data;
    return RTEST(bl->obj) ? rb_str_dup(bl->obj) : Qnil;
}

static void bind_long_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    bind_long_t *bl = (bind_long_t *)data;
    bl->obj = rb_str_dup(val);
}

static void bind_long_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    sb4 sz = 0;

    if (!NIL_P(length)) {
        sz = NUM2INT(length);
    }
    if (sz < 4000) {
        sz = 4000;
    }
    sz += bind_long_offset;
    obind->value_sz = INT_MAX;
    obind->alloc_sz = (sz + (sizeof(VALUE) - 1)) & ~(sizeof(VALUE) - 1);
}

static void bind_long_init_elem(oci8_bind_t *obind, VALUE svc)
{
    ub4 idx = 0;

    do {
        bind_long_t *bl = (bind_long_t *)((char*)obind->valuep + obind->alloc_sz * idx);
        bl->obj = Qnil;
    } while (++idx < obind->maxar_sz);
}

static ub1 bind_long_in(oci8_bind_t *obind, ub4 idx, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp)
{
    bind_long_t *bl = (bind_long_t *)((char*)obind->valuep + obind->alloc_sz * idx);

    *alenpp = &bl->alen;
    *indpp = &obind->u.inds[idx];
    if (NIL_P(bl->obj)) {
        *valuepp = NULL;
        bl->alen = 0;
        obind->u.inds[idx] = -1;
    } else {
        StringValue(bl->obj);
        *valuepp = RSTRING_PTR(bl->obj);
        bl->alen = RSTRING_LEN(bl->obj);
        obind->u.inds[idx] = 0;
    }
    return OCI_ONE_PIECE;
}

static void bind_long_out(oci8_bind_t *obind, ub4 idx, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp)
{
    bind_long_t *bl = (bind_long_t *)((char*)obind->valuep + obind->alloc_sz * idx);

    switch (piece) {
    case OCI_NEXT_PIECE:
    case OCI_LAST_PIECE:
        if (bl->alen > 0) {
            if (!RTEST(bl->obj)) {
                bl->obj = rb_str_buf_new(bl->alen);
            }
            rb_str_buf_cat(bl->obj, bl->buf, bl->alen);
        }
        break;
    default:
        /* OCI_FIRST_PIECE is passed at the first call according to manuals.
         * But OCI_ONE_PIECE is passed on Oracle 8 and 8i on Windows...
         */
        bl->obj = Qnil;
    }
    *valuepp = bl->buf;
    *alenpp = &bl->alen;
    *indpp = &obind->u.inds[idx];
    bl->alen = obind->alloc_sz - bind_long_offset;
    obind->u.inds[idx] = 0;
}

static const oci8_bind_class_t bind_long_class = {
    {
        bind_long_mark,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
    bind_long_init_elem,
    bind_long_in,
    bind_long_out,
    NULL,
    SQLT_CHR
};

/*
 * bind_long_raw
 */
static const oci8_bind_class_t bind_long_raw_class = {
    {
        bind_long_mark,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
    bind_long_init_elem,
    bind_long_in,
    bind_long_out,
    NULL,
    SQLT_BIN
};
#endif /* USE_DYNAMIC_FETCH */

/*
 * bind_float
 */
static VALUE bind_float_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return rb_float_new(*(double*)data);
}

static void bind_float_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    /* val is converted to Float if it isn't Float. */
    *(double*)data = RFLOAT_VALUE(rb_Float(val));
}

static void bind_float_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(double);
    obind->alloc_sz = sizeof(double);
}

static const oci8_bind_class_t bind_float_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_float_get,
    bind_float_set,
    bind_float_init,
    NULL,
    NULL,
    NULL,
    NULL,
    SQLT_FLT
};

#ifndef SQLT_BDOUBLE
#define SQLT_BDOUBLE 22
#endif
static const oci8_bind_class_t bind_binary_double_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_float_get,
    bind_float_set,
    bind_float_init,
    NULL,
    NULL,
    NULL,
    NULL,
    SQLT_BDOUBLE
};

static VALUE oci8_bind_get(VALUE self)
{
    oci8_bind_t *obind = DATA_PTR(self);
    const oci8_bind_class_t *obc = (const oci8_bind_class_t *)obind->base.klass;
    ub4 idx = obind->curar_idx;
    void **null_structp = NULL;

    if (NIL_P(obind->tdo)) {
        if (obind->u.inds[idx] != 0)
            return Qnil;
    } else {
        null_structp = &obind->u.null_structs[idx];
        if (*(OCIInd*)*null_structp != 0)
            return Qnil;
    }
    return obc->get(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp);
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
    const oci8_bind_class_t *obc = (const oci8_bind_class_t *)obind->base.klass;
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
        }
        obc->set(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp, val);
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
    const oci8_bind_class_t *bind_class = (const oci8_bind_class_t *)obind->base.klass;
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
    sym_char_semantics = ID2SYM(rb_intern("char_semantics"));

    rb_define_method(cOCI8BindTypeBase, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCI8BindTypeBase, "get", oci8_bind_get, 0);
    rb_define_method(cOCI8BindTypeBase, "set", oci8_bind_set, 1);

    /* register primitive data types. */
    oci8_define_bind_class("String", &bind_string_class);
    oci8_define_bind_class("RAW", &bind_raw_class);
#ifdef USE_DYNAMIC_FETCH
    oci8_define_bind_class("Long", &bind_long_class);
    oci8_define_bind_class("LongRaw", &bind_long_raw_class);
#endif /* USE_DYNAMIC_FETCH */
    oci8_define_bind_class("Float", &bind_float_class);
    if (oracle_client_version >= ORAVER_10_1) {
        oci8_define_bind_class("BinaryDouble", &bind_binary_double_class);
    }
}

oci8_bind_t *oci8_get_bind(VALUE obj)
{
    oci8_base_t *base;
    Check_Handle(obj, cOCI8BindTypeBase, base);
    return (oci8_bind_t *)base;
}
