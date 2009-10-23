/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * ocihandle.c
 *
 * Copyright (C) 2009 KUBO Takehiro <kubo@jiubao.org>
 *
 * implement OCIHandle
 *
 */
#include "oci8.h"

static oci8_base_class_t oci8_base_class = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

static VALUE oci8_handle_initialize(VALUE self)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(CLASS_OF(self)));
}

static VALUE oci8_handle_free(VALUE self)
{
    oci8_base_t *base = DATA_PTR(self);

    oci8_base_free(base);
    return self;
}

static void oci8_handle_mark(oci8_base_t *base)
{
    if (base->klass->mark != NULL)
        base->klass->mark(base);
}

static void oci8_handle_cleanup(oci8_base_t *base)
{
    oci8_base_free(base);
    xfree(base);
}

static VALUE oci8_s_allocate(VALUE klass)
{
    oci8_base_t *base;
    const oci8_base_class_t *base_class;
    VALUE superklass;
    VALUE obj;

    superklass = klass;
    while (!RTEST(rb_ivar_defined(superklass, oci8_id_oci8_class))) {
        superklass = RCLASS_SUPER(superklass);
        if (superklass == rb_cObject)
            rb_raise(rb_eRuntimeError, "private method `new' called for %s:Class", rb_class2name(klass));
    }
    obj = rb_ivar_get(superklass, oci8_id_oci8_class);
    base_class = DATA_PTR(obj);

    base = xmalloc(base_class->size);
    memset(base, 0, base_class->size);

    obj = Data_Wrap_Struct(klass, oci8_handle_mark, oci8_handle_cleanup, base);
    base->self = obj;
    base->klass = base_class;
    base->parent = NULL;
    base->next = base;
    base->prev = base;
    base->children = NULL;
    return obj;
}

void Init_oci8_handle(void)
{
    VALUE obj;

    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    rb_define_alloc_func(oci8_cOCIHandle, oci8_s_allocate);
    rb_define_method(oci8_cOCIHandle, "initialize", oci8_handle_initialize, 0);
    rb_define_method(oci8_cOCIHandle, "free", oci8_handle_free, 0);
    obj = Data_Wrap_Struct(rb_cObject, 0, 0, &oci8_base_class);
    rb_ivar_set(oci8_cOCIHandle, oci8_id_oci8_class, obj);
}
