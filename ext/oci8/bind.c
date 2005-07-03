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
typedef struct {
    oci8_bind_t base;
} oci8_bind_string_t;

static void bind_string_free(oci8_base_t *base)
{
    oci8_bind_t *bind_base = (oci8_bind_t *)base;
    if (bind_base->valuep != NULL) {
        xfree(bind_base->valuep);
	bind_base->valuep = NULL;
    }
}

static VALUE bind_string_get(oci8_bind_t *base)
{
    return rb_str_new(base->valuep, base->rlen);
}

static void bind_string_set(oci8_bind_t *base, VALUE val)
{
    StringValue(val);
    if (RSTRING(val)->len > base->value_sz) {
        rb_raise(rb_eArgError, "too long String to set. (%d for %d)", RSTRING(val)->len, base->value_sz);
    }
    memcpy(base->valuep, RSTRING(val)->ptr, RSTRING(val)->len);
    base->rlen = RSTRING(val)->len;
}

static void bind_string_init(oci8_bind_t *base, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    sb4 sz = 0;

    if (NIL_P(length)) {
        if (NIL_P(*val)) {
            rb_raise(rb_eArgError, "value and length are both null.");
        }
        StringValue(*val);
        sz = RSTRING(*val)->len;
    } else {
      sz = NUM2INT(length);
    }
    if (sz <= 0) {
        rb_raise(rb_eArgError, "invalid bind length %d", sz);
    }
    base->valuep = xmalloc(sz);
    base->value_sz = sz;
}

static oci8_bind_class_t bind_string_class = {
    {
        NULL,
	bind_string_free,
	sizeof(oci8_bind_string_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    SQLT_CHR
};

/*
 * bind_raw
 */
static oci8_bind_class_t bind_raw_class = {
    {
        NULL,
	bind_string_free,
	sizeof(oci8_bind_string_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    SQLT_BIN
};

/*
 * bind_fixnum
 */
typedef struct {
    oci8_bind_t base;
    sword val;
} oci8_bind_fixnum_t;

static VALUE bind_fixnum_get(oci8_bind_t *base)
{
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    return INT2FIX(fixnum->val);
}

static void bind_fixnum_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    Check_Type(val, T_FIXNUM);
    fixnum->val = FIX2INT(val);
}

static void bind_fixnum_init(oci8_bind_t *base, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    base->valuep = &fixnum->val;
    base->value_sz = sizeof(fixnum->val);
}

static oci8_bind_class_t bind_fixnum_class = {
    {
        NULL,
	NULL,
	sizeof(oci8_bind_fixnum_t)
    },
    bind_fixnum_get,
    bind_fixnum_set,
    bind_fixnum_init,
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
    oci8_bind_float_t *flt = (oci8_bind_float_t *)base;

    return rb_float_new(flt->val);
}

static void bind_float_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_float_t *flt = (oci8_bind_float_t *)base;

    Check_Type(val, T_FLOAT);
    flt->val = RFLOAT(val)->value;
}

static void bind_float_init(oci8_bind_t *base, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_float_t *flt = (oci8_bind_float_t *)base;

    base->valuep = &flt->val;
    base->value_sz = sizeof(flt->val);
}

static oci8_bind_class_t bind_float_class = {
    {
        NULL,
	NULL,
	sizeof(oci8_bind_float_t)
    },
    bind_float_get,
    bind_float_set,
    bind_float_init,
    SQLT_FLT
};

static VALUE oci8_get_data(VALUE self)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    if (base->ind != 0)
        return Qnil;
    return bind_class->get(base);
}

static VALUE oci8_set_data(VALUE self, VALUE val)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    if (NIL_P(val)) {
        base->ind = -1;
    } else {
        bind_class->set(base, val);
        base->ind = 0;
    }
    return self;
}

static VALUE oci8_bind_initialize(VALUE self, VALUE val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    bind_class->init(base, &val, length, prec, scale);
    base->rlen = base->value_sz;
    base->ind = -1;
    if (!NIL_P(val)) {
        rb_funcall(self, id_set, 1, val);
    }
    return Qnil;
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

    rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCIBind, "get", oci8_get_data, 0);
    rb_define_method(cOCIBind, "set", oci8_set_data, 1);

    /* register primitive data types. */
    oci8_define_bind_class("String", &bind_string_class);
    oci8_define_bind_class("RAW", &bind_raw_class);
    oci8_define_bind_class("Fixnum", &bind_fixnum_class);
    oci8_define_bind_class("Float", &bind_float_class);
}

oci8_bind_t *oci8_get_bind(VALUE obj)
{
    oci8_base_t *base;
    Check_Handle(obj, cOCIBind, base);
    return (oci8_bind_t *)base;
}
