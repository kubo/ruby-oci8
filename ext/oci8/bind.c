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

typedef struct {
    sb4 size;
    char buf[1];
} vstr_t;

/*
 * bind_string
 */
typedef struct {
    oci8_bind_t base;
} oci8_bind_string_t;

static void bind_string_free(oci8_base_t *base)
{
    oci8_bind_t *bind_base = (oci8_bind_t *)base;
    oci8_bind_free(base);
    if (bind_base->valuep != NULL) {
        xfree(bind_base->valuep);
        bind_base->valuep = NULL;
    }
}

static VALUE bind_string_get(oci8_bind_t *base)
{
    vstr_t *vstr = base->valuep;
    return rb_str_new(vstr->buf, vstr->size);
}

static void bind_string_set(oci8_bind_t *base, VALUE val)
{
    vstr_t *vstr = base->valuep;

    StringValue(val);
    if (RSTRING(val)->len > base->value_sz - sizeof(vstr->size)) {
        rb_raise(rb_eArgError, "too long String to set. (%d for %d)", RSTRING(val)->len, base->value_sz - sizeof(vstr->size));
    }
    memcpy(vstr->buf, RSTRING(val)->ptr, RSTRING(val)->len);
    vstr->size = RSTRING(val)->len;
}

static void bind_string_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    sb4 sz = 0;
    vstr_t *vstr;

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
    base->valuep = xmalloc(sizeof(vstr->size) + sz);
    base->value_sz = sizeof(vstr->size) + sz;
    base->use_rlen = 0;
    vstr = base->valuep;
    vstr->size = sz;
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
        bind_string_free,
        sizeof(oci8_bind_string_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    NULL,
    NULL,
    SQLT_LVB
};

/*
 * bind_long
 */
typedef struct {
    oci8_bind_t base;
    VALUE obj;
    ub4 bufsiz;
    char *buf;
} oci8_bind_long_t;

static void bind_long_free(oci8_base_t *base)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t *)base;
    oci8_bind_free(base);
    if (bind_long->buf != NULL) {
        xfree(bind_long->buf);
        bind_long->buf = NULL;
    }
}

static void bind_long_mark(oci8_base_t *base)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t *)base;
    rb_gc_mark(bind_long->obj);
}

static VALUE bind_long_get(oci8_bind_t *base)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t*)base;
    return rb_str_dup(bind_long->obj);
}

static void bind_long_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t*)base;
    bind_long->obj = rb_str_dup(val);
}

static void bind_long_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t*)base;
    sb4 sz = 0;

    if (!NIL_P(length)) {
        sz = NUM2INT(length);
    }
    if (sz < 4000) {
        sz = 4000;
    }
    base->value_sz = INT_MAX;
    base->use_rlen = 0;
    bind_long->bufsiz = sz;
    bind_long->buf = xmalloc(sz);
    bind_long->obj = Qnil;
}

static ub1 bind_long_in(oci8_bind_t *base, ub1 piece)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t*)base;
    if (NIL_P(bind_long->obj)) {
        base->valuep = NULL;
        base->len.alen = 0;
        base->ind = -1;
    } else {
        StringValue(bind_long->obj);
        base->valuep = RSTRING(bind_long->obj)->ptr;
        base->len.alen = RSTRING(bind_long->obj)->len;
        base->ind = 0;
    }
    return OCI_ONE_PIECE;
}

static void bind_long_out(oci8_bind_t *base, ub1 piece)
{
    oci8_bind_long_t *bind_long = (oci8_bind_long_t*)base;

    if (piece == OCI_FIRST_PIECE) {
        bind_long->obj = Qnil;
    } else {
        if (NIL_P(bind_long->obj)) {
            bind_long->obj = rb_str_buf_new(base->len.alen);
        }
        rb_str_buf_cat(bind_long->obj, base->valuep, base->len.alen);
    }
    base->valuep = bind_long->buf;
    base->len.alen = bind_long->bufsiz;
}

static oci8_bind_class_t bind_long_class = {
    {
        bind_long_mark,
        bind_string_free,
        sizeof(oci8_bind_long_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
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
        bind_long_free,
        sizeof(oci8_bind_long_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
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
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    return LONG2NUM(fixnum->val);
}

static void bind_fixnum_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    Check_Type(val, T_FIXNUM);
    fixnum->val = FIX2LONG(val);
}

static void bind_fixnum_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_fixnum_t *fixnum = (oci8_bind_fixnum_t *)base;

    base->valuep = &fixnum->val;
    base->value_sz = sizeof(fixnum->val);
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

static void bind_float_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_float_t *flt = (oci8_bind_float_t *)base;

    base->valuep = &flt->val;
    base->value_sz = sizeof(flt->val);
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

static VALUE oci8_bind_initialize(VALUE self, VALUE svc, VALUE val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_t *base = DATA_PTR(self);
    oci8_bind_class_t *bind_class = (oci8_bind_class_t *)base->base.klass;

    base->next = base;
    base->prev = base;
    base->use_rlen = 1;
    bind_class->init(base, svc, &val, length, prec, scale);
    base->len.rlen = base->value_sz;
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

    rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 5);
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
