/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002-2008 KUBO Takehiro <kubo@jiubao.org>
 */

#ifdef __linux__ /* for RTLD_DEFAULT */
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include "oci8.h"

#define DEBUG_CORE_FILE 1
#ifdef DEBUG_CORE_FILE
#include <signal.h>
#endif

static oci8_base_class_t oci8_base_class = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

ID oci8_id_new;
ID oci8_id_get;
ID oci8_id_set;
ID oci8_id_keys;
int oci8_in_finalizer = 0;
VALUE oci8_cOCIHandle;

static ID id_oci8_class;

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
    const oci8_base_class_t *base_class;
    VALUE superklass;
    VALUE obj;

    superklass = klass;
    while (!RTEST(rb_ivar_defined(superklass, id_oci8_class))) {
        superklass = RCLASS_SUPER(superklass);
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
    VALUE obj;

#ifdef RUNTIME_API_CHECK
    Init_oci8_apiwrap();
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
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    rb_define_alloc_func(oci8_cOCIHandle, oci8_s_allocate);
    rb_define_method(oci8_cOCIHandle, "initialize", oci8_handle_initialize, 0);
    rb_define_method(oci8_cOCIHandle, "free", oci8_handle_free, 0);
    obj = Data_Wrap_Struct(rb_cObject, 0, 0, &oci8_base_class);
    rb_ivar_set(oci8_cOCIHandle, id_oci8_class, obj);

    /* OCI8 class */
    cOCI8 = Init_oci8();
    /* OCI8::BindType module */
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    /* OCI8::BindType::Base class */
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", oci8_cOCIHandle);

    /* Handle */
    Init_oci8_bind(cOCI8BindTypeBase);
    Init_oci8_stmt(cOCI8);

    /* register allocators */
    Init_oci8_metadata(cOCI8);
    Init_oci8_lob(cOCI8);

    Init_ora_date();
    Init_oci_number(cOCI8);
    Init_oci_datetime();
    Init_oci_object(cOCI8);
    Init_oci_xmldb();

#ifdef DEBUG_CORE_FILE
    signal(SIGSEGV, SIG_DFL);
#endif
}

VALUE oci8_define_class(const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class(name, oci8_cOCIHandle);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class_under(outer, name, oci8_cOCIHandle);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_bind_class(const char *name, const oci8_bind_class_t *bind_class)
{
    VALUE klass = rb_define_class_under(mOCI8BindType, name, cOCI8BindTypeBase);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, (void*)bind_class);
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

#ifdef RUBY_VM
typedef struct {
    dvoid *hndlp;
    OCIError *errhp;
} ocibreak_arg_t;

static VALUE call_OCIBreak(void *user_data)
{
    ocibreak_arg_t *arg = (ocibreak_arg_t *)user_data;
    OCIBreak(arg->hndlp, arg->errhp);
    return Qnil;
}

static void oci8_unblock_func(void *user_data)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)user_data;
    if (svcctx->base.hp.ptr != NULL) {
        ocibreak_arg_t arg;
        arg.hndlp = svcctx->base.hp.ptr;
        arg.errhp = oci8_errhp;
        rb_thread_blocking_region(call_OCIBreak, &arg, NULL, NULL);
    }
}

/* ruby 1.9 */
sword oci8_blocking_region(oci8_svcctx_t *svcctx, rb_blocking_function_t func, void *data)
{
    if (svcctx->non_blocking) {
        sword rv;

        if (!NIL_P(svcctx->executing_thread)) {
            rb_raise(rb_eRuntimeError /* FIXME */, "executing in another thread");
        }
        svcctx->executing_thread = rb_thread_current();
        rv = (sword)rb_thread_blocking_region(func, data, oci8_unblock_func, svcctx);
        svcctx->executing_thread = Qnil;
        if (rv == OCI_ERROR) {
            if (oci8_get_error_code(oci8_errhp) == 1013) {
                rb_raise(eOCIBreak, "Canceled by user request.");
            }
        }
        return rv;
    } else {
        return (sword)func(data);
    }
}
#else /* RUBY_VM */

/* ruby 1.8 */
sword oci8_blocking_region(oci8_svcctx_t *svcctx, rb_blocking_function_t func, void *data)
{
    struct timeval tv;
    sword rv;

    if (!NIL_P(svcctx->executing_thread)) {
        rb_raise(rb_eRuntimeError /* FIXME */, "executing in another thread");
    }
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    svcctx->executing_thread = rb_thread_current();
    while ((rv = func(data)) == OCI_STILL_EXECUTING) {
        rb_thread_wait_for(tv);
        if (tv.tv_usec < 500000)
            tv.tv_usec <<= 1;
    }
    if (rv == OCI_ERROR) {
        if (oci8_get_error_code(oci8_errhp) == 1013) {
            if (have_OCIReset)
                OCIReset(svcctx->base.hp.ptr, oci8_errhp);
            svcctx->executing_thread = Qnil;
            rb_raise(eOCIBreak, "Canceled by user request.");
        }
    }
    svcctx->executing_thread = Qnil;
    return rv;
}
#endif /* RUBY_VM */

#if defined RUNTIME_API_CHECK
void *oci8_find_symbol(const char *symbol_name)
{
#if defined _WIN32
    static HMODULE hModule = NULL;
    if (hModule == NULL) {
        hModule = GetModuleHandle("OCI.DLL");
    }
    return GetProcAddress(hModule, symbol_name);
#elif defined RTLD_DEFAULT
    return dlsym(RTLD_DEFAULT, symbol_name);
#else
#error RUNTIME_API_CHECK is not supported on this platform.
#endif
}
#endif /* RUNTIME_API_CHECK */
