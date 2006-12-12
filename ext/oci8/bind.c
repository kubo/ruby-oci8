/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * bind.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

static ID id_bind_type;
static ID id_set;

static VALUE cOCIBind;

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_t *base)
{
    oci8_vstr_t *vstr = base->valuep;
    return rb_str_new(vstr->buf, vstr->size);
}

static void bind_string_set(oci8_bind_t *base, VALUE val)
{
    oci8_vstr_t *vstr = base->valuep;

    StringValue(val);
    if (RSTRING_LEN(val) > base->value_sz - sizeof(vstr->size)) {
        rb_raise(rb_eArgError, "too long String to set. (%d for %d)", RSTRING_LEN(val), base->value_sz - sizeof(vstr->size));
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LEN(val);
}

static void bind_string_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length)
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
    base->value_sz = sizeof(sb4) + sz;
    base->alloc_sz = sizeof(sb4) + sz;
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
    bind_long_t *bl = ((oci8_bind_t*)base)->valuep;
    rb_gc_mark(bl->obj);
}

static VALUE bind_long_get(oci8_bind_t *base)
{
    bind_long_t *bl = ((oci8_bind_t*)base)->valuep;
    return RTEST(bl->obj) ? rb_str_dup(bl->obj) : Qnil;
}

static void bind_long_set(oci8_bind_t *base, VALUE val)
{
    bind_long_t *bl = ((oci8_bind_t*)base)->valuep;
    bl->obj = rb_str_dup(val);
}

static void bind_long_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length)
{
    sb4 sz = 0;

    if (!NIL_P(length)) {
        sz = NUM2INT(length);
    }
    if (sz < 4000) {
        sz = 4000;
    }
    base->value_sz = INT_MAX;
    base->alloc_sz = sz + bind_long_offset;
}

static void bind_long_init_elem(oci8_bind_t *base, VALUE svc)
{
    bind_long_t *bl = base->valuep;
    bl->obj = Qnil;
}

static ub1 bind_long_in(oci8_bind_t *base, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp)
{
    bind_long_t *bl = base->valuep;

    *alenpp = &bl->alen;
    *indpp = &base->ind;
    if (NIL_P(bl->obj)) {
        *valuepp = NULL;
        bl->alen = 0;
        base->ind = -1;
    } else {
        StringValue(bl->obj);
        *valuepp = RSTRING_PTR(bl->obj);
        bl->alen = RSTRING_LEN(bl->obj);
        base->ind = 0;
    }
    return OCI_ONE_PIECE;
}

static void bind_long_out(oci8_bind_t *base, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp)
{
    bind_long_t *bl = base->valuep;

    if (piece == OCI_FIRST_PIECE) {
        bl->obj = Qnil;
    } else {
        if (!RTEST(bl->obj)) {
            bl->obj = rb_str_buf_new(bl->alen);
        }
        rb_str_buf_cat(bl->obj, bl->buf, bl->alen);
    }
    *valuepp = bl->buf;
    *alenpp = &bl->alen;
    *indpp = &base->ind;
    bl->alen = base->alloc_sz - bind_long_offset;
    base->ind = 0;
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
typedef struct {
    oci8_bind_t base;
    long val;
} oci8_bind_fixnum_t;

static VALUE bind_fixnum_get(oci8_bind_t *base)
{
    return LONG2NUM(*(long*)base->valuep);
}

static void bind_fixnum_set(oci8_bind_t *base, VALUE val)
{
    Check_Type(val, T_FIXNUM);
    *(long*)base->valuep = FIX2LONG(val);
}

static void bind_fixnum_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length)
{
    base->value_sz = sizeof(long);
    base->alloc_sz = sizeof(long);
}

static oci8_bind_class_t bind_fixnum_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_fixnum_t)
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
typedef struct {
    oci8_bind_t base;
    double val;
} oci8_bind_float_t;

static VALUE bind_float_get(oci8_bind_t *base)
{
    return rb_float_new(*(double*)base->valuep);
}

static void bind_float_set(oci8_bind_t *base, VALUE val)
{
    Check_Type(val, T_FLOAT);
    *(double*)base->valuep = RFLOAT(val)->value;
}

static void bind_float_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length)
{
    base->value_sz = sizeof(double);
    base->alloc_sz = sizeof(double);
}

static oci8_bind_class_t bind_float_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_float_t)
    },
    bind_float_get,
    bind_float_set,
    bind_float_init,
    NULL,
    NULL,
    NULL,
    SQLT_FLT
};

static VALUE oci8_get_data(VALUE self)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    if (RTEST(base->tdo)) {
        if (*(OCIInd*)base->null_struct != 0)
            return Qnil;
    } else {
        if (base->ind != 0)
            return Qnil;
    }
    return bind_class->get(base);
}

static VALUE oci8_set_data(VALUE self, VALUE val)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    if (NIL_P(val)) {
        if (RTEST(base->tdo)) {
            *(OCIInd*)base->null_struct = -1;
        } else {
            base->ind = -1;
        }
    } else {
        bind_class->set(base, val);
        if (RTEST(base->tdo)) {
            *(OCIInd*)base->null_struct = 0;
        } else {
            base->ind = 0;
        }
    }
    return self;
}

static VALUE oci8_bind_initialize(VALUE self, VALUE svc, VALUE val, VALUE length)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    base->next = base;
    base->prev = base;
    bind_class->init(base, svc, &val, length);
    if (base->alloc_sz > 0) {
        base->valuep = xmalloc(base->alloc_sz);
        memset(base->valuep, 0, base->alloc_sz);
    } else {
        base->valuep = NULL;
    }
    if (bind_class->init_elem != NULL) {
        bind_class->init_elem(base, svc);
    }
    base->ind = -1;
    if (!NIL_P(val)) {
        rb_funcall(self, id_set, 1, val);
    }
    return Qnil;
}

void oci8_bind_free(oci8_base_t *base)
{
    oci8_bind_t *bind = (oci8_bind_t *)base;
    bind->next->prev = bind->prev;
    bind->prev->next = bind->next;
    bind->next = bind->prev = bind;
    if (bind->valuep != NULL) {
        xfree(bind->valuep);
        bind->valuep = NULL;
    }
}

void oci8_bind_handle_mark(oci8_base_t *base)
{
    oci8_bind_handle_t *bind_handle = (oci8_bind_handle_t *)base;
    rb_gc_mark(bind_handle->obj);
}

VALUE oci8_bind_handle_get(oci8_bind_t *bind)
{
    oci8_bind_handle_t *bind_handle = (oci8_bind_handle_t *)bind;
    return bind_handle->obj;
}

void Init_oci8_bind(VALUE klass)
{
    cOCIBind = klass;
    id_bind_type = rb_intern("bind_type");
    id_set = rb_intern("set");

    rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 3);
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
