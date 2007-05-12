/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * bind.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

static ID id_bind_type;
static ID id_set;

static VALUE cOCIBind;

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;
    return rb_str_new(vstr->buf, vstr->size);
}

static void bind_string_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;

    StringValue(val);
    if (RSTRING_LEN(val) > obind->value_sz - sizeof(vstr->size)) {
        rb_raise(rb_eArgError, "too long String to set. (%ld for %d)", RSTRING_LEN(val), obind->value_sz - sizeof(vstr->size));
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LEN(val);
}

static void bind_string_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    sb4 sz;
    if (NIL_P(length)) {
        if (NIL_P(*val)) {
            rb_raise(rb_eArgError, "value and length are both null.");
        }
        StringValue(*val);
        sz = RSTRING_LEN(*val);
    } else {
        sz = NUM2INT(length);
    }
    if (sz <= 0) {
        rb_raise(rb_eArgError, "invalid bind length %d", sz);
    }
    sz += sizeof(sb4);
    obind->value_sz = sz;
    obind->alloc_sz = (sz + (sizeof(sb4) - 1)) & ~(sizeof(sb4) - 1);
}

static oci8_bind_class_t bind_string_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    NULL,
    NULL,
    NULL,
    SQLT_LVC
};

/*
 * bind_raw
 */
static oci8_bind_class_t bind_raw_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    NULL,
    NULL,
    NULL,
    SQLT_LVB
};

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

static void bind_long_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
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

    if (piece == OCI_FIRST_PIECE) {
        bl->obj = Qnil;
    } else if (bl->alen > 0) {
        if (!RTEST(bl->obj)) {
            bl->obj = rb_str_buf_new(bl->alen);
        }
        rb_str_buf_cat(bl->obj, bl->buf, bl->alen);
    }
    *valuepp = bl->buf;
    *alenpp = &bl->alen;
    *indpp = &obind->u.inds[idx];
    bl->alen = obind->alloc_sz - bind_long_offset;
    obind->u.inds[idx] = 0;
}

static oci8_bind_class_t bind_long_class = {
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
    SQLT_CHR
};

/*
 * bind_long_raw
 */
static oci8_bind_class_t bind_long_raw_class = {
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
    SQLT_BIN
};

/*
 * bind_fixnum
 */
static VALUE bind_fixnum_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return LONG2NUM(*(long*)data);
}

static void bind_fixnum_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    Check_Type(val, T_FIXNUM);
    *(long*)data = FIX2LONG(val);
}

static void bind_fixnum_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    obind->value_sz = sizeof(long);
    obind->alloc_sz = sizeof(long);
}

static oci8_bind_class_t bind_fixnum_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_fixnum_get,
    bind_fixnum_set,
    bind_fixnum_init,
    NULL,
    NULL,
    NULL,
    SQLT_INT
};

/*
 * bind_float
 */
static VALUE bind_float_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return rb_float_new(*(double*)data);
}

static void bind_float_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    Check_Type(val, T_FLOAT);
    *(double*)data = RFLOAT(val)->value;
}

static void bind_float_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    obind->value_sz = sizeof(double);
    obind->alloc_sz = sizeof(double);
}

static oci8_bind_class_t bind_float_class = {
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
    SQLT_FLT
};

static inline VALUE oci8_get_data_at(oci8_bind_class_t *obc, oci8_bind_t *obind, ub4 idx)
{
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

static VALUE oci8_get_data(VALUE self)
{
    oci8_bind_t *obind = DATA_PTR(self);
    oci8_bind_class_t *obc = (oci8_bind_class_t *)obind->base.klass;

    if (obind->maxar_sz == 0) {
        return oci8_get_data_at(obc, obind, 0);
    } else {
        volatile VALUE ary = rb_ary_new2(obind->curar_sz);
        ub4 idx;

        for (idx = 0; idx < obind->curar_sz; idx++) {
            rb_ary_store(ary, idx, oci8_get_data_at(obc, obind, idx));
        }
        return ary;
    }
}

static inline void oci8_set_data_at(oci8_bind_class_t *obc, oci8_bind_t *obind, ub4 idx, VALUE val)
{

    if (NIL_P(val)) {
        if (NIL_P(obind->tdo)) {
            obind->u.inds[idx] = -1;
        } else {
            *(OCIInd*)obind->u.null_structs[idx] = -1;
        }
    } else {
        void *null_struct = NULL;

        if (NIL_P(obind->tdo)) {
            null_struct = NULL;
            obind->u.inds[idx] = 0;
        } else {
            null_struct = obind->u.null_structs[idx];
            *(OCIInd*)null_struct = 0;
        }
        obc->set(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_struct, val);
    }
}

static VALUE oci8_set_data(VALUE self, VALUE val)
{
    oci8_bind_t *obind = DATA_PTR(self);
    oci8_bind_class_t *obc = (oci8_bind_class_t *)obind->base.klass;

    if (obind->maxar_sz == 0) {
        oci8_set_data_at(obc, obind, 0, val);
    } else {
        ub4 size;
        ub4 idx;
        Check_Type(val, T_ARRAY);

        size = RARRAY_LEN(val);
        if (size > obind->maxar_sz) {
            rb_raise(rb_eRuntimeError, "over the max array size");
        }
        for (idx = 0; idx < size; idx++) {
            oci8_set_data_at(obc, obind, idx, RARRAY_PTR(val)[idx]);
        }
        obind->curar_sz = size;
    }
    return self;
}

static VALUE oci8_bind_initialize(VALUE self, VALUE svc, VALUE val, VALUE length, VALUE max_array_size)
{
    oci8_bind_t *obind = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)obind->base.klass;
    ub4 cnt = 1;

    obind->tdo = Qnil;
    obind->maxar_sz = NIL_P(max_array_size) ? 0 : NUM2UINT(max_array_size);
    obind->curar_sz = 0;
    if (obind->maxar_sz > 0)
        cnt = obind->maxar_sz;
    bind_class->init(obind, svc, &val, length);
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
        rb_funcall(self, id_set, 1, val);
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
    cOCIBind = klass;
    id_bind_type = rb_intern("bind_type");
    id_set = rb_intern("set");

    rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCIBind, "get", oci8_get_data, 0);
    rb_define_method(cOCIBind, "set", oci8_set_data, 1);

    /* register primitive data types. */
    oci8_define_bind_class("String", &bind_string_class);
    oci8_define_bind_class("RAW", &bind_raw_class);
    oci8_define_bind_class("Long", &bind_long_class);
    oci8_define_bind_class("LongRaw", &bind_long_raw_class);
    oci8_define_bind_class("Fixnum", &bind_fixnum_class);
    oci8_define_bind_class("Float", &bind_float_class);
}

oci8_bind_t *oci8_get_bind(VALUE obj)
{
    oci8_base_t *base;
    Check_Handle(obj, cOCIBind, base);
    return (oci8_bind_t *)base;
}
