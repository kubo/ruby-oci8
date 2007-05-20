/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

#define DEBUG_CORE_FILE 1
#ifdef DEBUG_CORE_FILE
#include <signal.h>
#endif

#ifdef RUNTIME_API_CHECK
rboci8_OCIRowidToChar_t rboci8_OCIRowidToChar;
#endif

oci8_base_class_t oci8_base_class = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

ID oci8_id_new;
ID oci8_id_get;
ID oci8_id_set;
ID oci8_id_keys;
int oci8_in_finalizer = 0;
static ID id_oci8_class;

static VALUE cOCIHandle;
static VALUE mOCI8BindType;
static VALUE cOCI8BindTypeBase;

static VALUE oci8_handle_initialize(VALUE self)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(CLASS_OF(self)));
}

void oci8_base_free(oci8_base_t *base)
{
    while (base->children != NULL) {
        oci8_base_free(base->children);
    }
    oci8_unlink_from_parent(base);
    if (base->klass->free != NULL)
        base->klass->free(base);
    if (base->type >= OCI_DTYPE_FIRST)
        OCIDescriptorFree(base->hp.ptr, base->type);
    else if (base->type >= OCI_HTYPE_FIRST)
        OCIHandleFree(base->hp.ptr, base->type);
    base->type = 0;
    base->hp.ptr = NULL;
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

    superklass = klass;
    while (!RTEST(rb_ivar_defined(superklass, id_oci8_class))) {
        superklass = RCLASS(superklass)->super;
        if (superklass == rb_cObject)
            rb_raise(rb_eRuntimeError, "private method `new' called for %s:Class", rb_class2name(klass));
    }
    obj = rb_ivar_get(superklass, id_oci8_class);
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

static void at_exit_func(VALUE val)
{
    oci8_in_finalizer = 1;
}

void
Init_oci8lib()
{
    VALUE cOCI8;

#ifdef RUNTIME_API_CHECK
#ifdef _WIN32
    rboci8_OCIRowidToChar = (rboci8_OCIRowidToChar_t)GetProcAddress(GetModuleHandle("OCI.DLL"), "OCIRowidToChar");
#else
#error RUNTIME_API_CHECK is not supported on this platform.
#endif
#endif

    id_oci8_class = rb_intern("__oci8_class__");
    oci8_id_new = rb_intern("new");
    oci8_id_get = rb_intern("get");
    oci8_id_set = rb_intern("set");
    oci8_id_keys = rb_intern("keys");
    rb_set_end_proc(at_exit_func, Qnil);

    Init_oci8_error();
    Init_oci8_env();

    /* OCIHandle class */
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    rb_define_alloc_func(cOCIHandle, oci8_s_allocate);
    rb_define_method(cOCIHandle, "initialize", oci8_handle_initialize, 0);
    rb_define_method(cOCIHandle, "free", oci8_handle_free, 0);

    /* OCI8 class */
    cOCI8 = Init_oci8();
    /* OCI8::BindType module */
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    /* OCI8::BindType::Base class */
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", cOCIHandle);

    /* Handle */
    Init_oci8_bind(cOCI8BindTypeBase);
    Init_oci8_stmt(cOCI8);

    /* register allocators */
    Init_oci8_rowid();
    Init_oci8_metadata(cOCI8);
    Init_oci8_lob(cOCI8);

    Init_ora_date();
    Init_oci_number(cOCI8);
    Init_oci_datetime();
    Init_oci_object(cOCI8);
#if BUILD_FOR_ORACLE_10_1
    Init_oci_xmldb();
#endif

#ifdef DEBUG_CORE_FILE
    signal(SIGSEGV, SIG_DFL);
#endif
}

VALUE oci8_define_class(const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class(name, cOCIHandle);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class_under(outer, name, cOCIHandle);
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

void oci8_link_to_parent(oci8_base_t *base, oci8_base_t *parent)
{
    if (base->parent != NULL) {
        oci8_unlink_from_parent(base);
    }
    if (parent->children == NULL) {
        parent->children = base;
    } else {
        base->next = parent->children;
        base->prev = parent->children->prev;
        parent->children->prev->next = base;
        parent->children->prev = base;
    }
    base->parent = parent;
}

void oci8_unlink_from_parent(oci8_base_t *base)
{
    if (base->parent == NULL) {
        return;
    }
    if (base->next == base) {
        base->parent->children = NULL;
    } else {
        if (base->parent->children == base) {
            base->parent->children = base->next;
        }
        base->next->prev = base->prev;
        base->prev->next = base->next;
        base->next = base;
        base->prev = base;
    }
    base->parent = NULL;
}
