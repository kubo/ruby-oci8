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

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_handle_t *bh)
{
    return rb_str_new(bh->valuep, bh->rlen);
}

static void bind_string_set(oci8_bind_handle_t *bh, VALUE val)
{
    StringValue(val);
    if (RSTRING(val)->len > bh->value_sz) {
        rb_raise(rb_eArgError, "too long String to set. (%d for %d)", RSTRING(val)->len, bh->value_sz);
    }
    memcpy(bh->valuep, RSTRING(val)->ptr, RSTRING(val)->len);
    bh->rlen = RSTRING(val)->len;
}

static void bind_string_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
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
    bh->valuep = xmalloc(sz);
    bh->value_sz = sz;
}

static void bind_string_free(oci8_bind_handle_t *bh)
{
    if (bh->valuep != NULL)
        xfree(bh->valuep);
}

static oci8_bind_type_t bind_string = {
    bind_string_get,
    bind_string_set,
    bind_string_init,
    bind_string_free,
    SQLT_CHR,
};

/*
 * bind_raw
 */
static oci8_bind_type_t bind_raw = {
    bind_string_get,
    bind_string_set,
    bind_string_init,
    bind_string_free,
    SQLT_BIN,
};

/*
 * bind_fixnum
 */
static VALUE bind_fixnum_get(oci8_bind_handle_t *bh)
{
    return INT2FIX(bh->value.sw);
}

static void bind_fixnum_set(oci8_bind_handle_t *bh, VALUE val)
{
    Check_Type(val, T_FIXNUM);
    bh->value.sw = FIX2INT(val);
}

static void bind_fixnum_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    bh->valuep = &bh->value.sw;
    bh->value_sz = sizeof(bh->value.sw);
}

static oci8_bind_type_t bind_fixnum = {
    bind_fixnum_get,
    bind_fixnum_set,
    bind_fixnum_init,
    NULL,
    SQLT_INT,
};

/*
 * bind_float
 */
static VALUE bind_float_get(oci8_bind_handle_t *bh)
{
    return rb_float_new(bh->value.dbl);
}

static void bind_float_set(oci8_bind_handle_t *bh, VALUE val)
{
    Check_Type(val, T_FLOAT);
    bh->value.dbl = RFLOAT(val)->value;
}

static void bind_float_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    bh->valuep = &bh->value.dbl;
    bh->value_sz = sizeof(bh->value.dbl);
}

static oci8_bind_type_t bind_float = {
    bind_float_get,
    bind_float_set,
    bind_float_init,
    NULL,
    SQLT_FLT,
};

static VALUE oci8_get_data(VALUE self)
{
    oci8_bind_handle_t *bh;

    Data_Get_Struct(self, oci8_bind_handle_t, bh);
    if (bh->ind != 0)
        return Qnil;
    return bh->bind_type->get(bh);
}

static VALUE oci8_set_data(VALUE self, VALUE val)
{
    oci8_bind_handle_t *bh;

    Data_Get_Struct(self, oci8_bind_handle_t, bh);
    if (NIL_P(val)) {
        bh->ind = -1;
    } else {
        bh->bind_type->set(bh, val);
        bh->ind = 0;
    }
    return self;
}

static VALUE oci8_bind_s_allocate(VALUE klass)
{
    oci8_bind_handle_t *bh;
    VALUE obj;
    VALUE bind_type;

    if (klass == cOCIBind) {
        rb_raise(rb_eRuntimeError, "Could not create an abstract class OCIBind");
    }
    obj = Data_Make_Struct(klass, oci8_bind_handle_t, oci8_handle_mark, oci8_handle_cleanup, bh);
    bh->self = obj;
    bh->ind = -1;
    bh->obj = Qnil;
    bind_type = rb_ivar_get(klass, id_bind_type);
    while (NIL_P(bind_type)) {
        klass = RCLASS(klass)->super;
        bind_type = rb_ivar_get(klass, id_bind_type);
    }
    bh->bind_type = DATA_PTR(bind_type);
    return obj;
}

static VALUE oci8_bind_initialize(VALUE self, VALUE val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_handle_t *bh = DATA_PTR(self);

    bh->bind_type->init(bh, &val, length, prec, scale);
    bh->rlen = bh->value_sz;
    if (!NIL_P(val)) {
        /* don't call oci8_set_data() directly.
         * #set(val) may be overwritten by subclass.
         */
        rb_funcall(self, id_set, 1, val);
    }
    return Qnil;
}

void Init_oci8_bind(void)
{
    id_bind_type = rb_intern("bind_type");
    id_set = rb_intern("set");

    rb_define_alloc_func(cOCIBind, oci8_bind_s_allocate);
    rb_define_method(cOCIBind, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCIBind, "get", oci8_get_data, 0);
    rb_define_method(cOCIBind, "set", oci8_set_data, 1);

    /* register primitive data types. */
    oci8_register_bind_type("String", &bind_string);
    oci8_register_bind_type("RAW", &bind_raw);
    oci8_register_bind_type("Fixnum", &bind_fixnum);
    oci8_register_bind_type("Float", &bind_float);
}

void oci8_register_bind_type(const char *name, oci8_bind_type_t *bind_type)
{
    VALUE klass = rb_define_class_under(mOCI8BindType, name, cOCIBind);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, bind_type);
    rb_ivar_set(klass, id_bind_type, obj);
}
