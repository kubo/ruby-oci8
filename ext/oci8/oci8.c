/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
*/
#include "oci8.h"

#define DEBUG_CORE_FILE 1
#ifdef DEBUG_CORE_FILE
#include <signal.h>
#endif

oci8_base_class_t oci8_base_class = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

static ID id_oci8_class;

static VALUE cOCI8Base;
static VALUE mOCI8BindType;
static VALUE cOCI8BindTypeBase;

static VALUE oci8_handle_initialize(VALUE self)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(CLASS_OF(self)));
}

void oci8_base_free(oci8_base_t *base)
{
    if (base->klass->free != NULL)
        base->klass->free(base);
    if (base->type >= OCI_DTYPE_FIRST)
        OCIDescriptorFree(base->hp, base->type);
    else if (base->type >= OCI_HTYPE_FIRST)
        OCIHandleFree(base->hp, base->type);
    base->type = 0;
    base->hp = NULL;
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
    free(base);
}

static VALUE oci8_s_allocate(VALUE klass)
{
    oci8_base_t *base;
    oci8_base_class_t *base_class;
    VALUE superklass;
    VALUE obj;

    obj = rb_ivar_get(klass, id_oci8_class);
    superklass = klass;
    while (NIL_P(obj)) {
        superklass = RCLASS(superklass)->super;
	if (superklass == rb_cObject)
	    rb_raise(rb_eRuntimeError, "private method `new' called for %s:Class", rb_class2name(klass));
        obj = rb_ivar_get(superklass, id_oci8_class);
    }
    base_class = DATA_PTR(obj);

    base = xmalloc(base_class->size);
    memset(base, 0, base_class->size);

    obj = Data_Wrap_Struct(klass, oci8_handle_mark, oci8_handle_cleanup, base);
    base->self = obj;
    base->klass = base_class;
    return obj;
}

void
Init_oci8lib()
{
    VALUE cOCI8;

    id_oci8_class = rb_intern("__oci8_class__");

    Init_oci8_const();
    Init_oci8_error();
    Init_oci8_env();

    /* OCI8 class */
    cOCI8Base = rb_define_class("OCI8Base", rb_cObject);
    cOCI8 = Init_oci8_svcctx();
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", cOCI8Base);

    rb_define_alloc_func(cOCI8Base, oci8_s_allocate);

    rb_define_method(cOCI8Base, "initialize", oci8_handle_initialize, 0);
    rb_define_method(cOCI8Base, "free", oci8_handle_free, 0);


    /* Handle */
    Init_oci8_bind(cOCI8BindTypeBase);
    Init_oci8_stmt();

    /* register allocators */
    Init_oci8_rowid();
    Init_oci8_param();
    Init_oci8_lob(cOCI8);

    Init_ora_date();
    Init_ora_number();
    Init_oci_number(cOCI8);
    Init_oci_datetime();

#ifdef DEBUG_CORE_FILE
    signal(SIGSEGV, SIG_DFL);
#endif
}

VALUE oci8_define_class(const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class(name, cOCI8Base);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class_under(outer, name, cOCI8Base);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_bind_class(const char *name, oci8_bind_class_t *bind_class)
{
    VALUE klass = rb_define_class_under(mOCI8BindType, name, cOCI8BindTypeBase);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, bind_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}
