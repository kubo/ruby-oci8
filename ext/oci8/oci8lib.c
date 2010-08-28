/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002-2009 KUBO Takehiro <kubo@jiubao.org>
 */

#include "oci8.h"

#define DEBUG_CORE_FILE 1
#ifdef DEBUG_CORE_FILE
#include <signal.h>
#endif

ID oci8_id_new;
ID oci8_id_get;
ID oci8_id_set;
ID oci8_id_keys;
ID oci8_id_oci8_class;
int oci8_in_finalizer = 0;
VALUE oci8_cOCIHandle;


static VALUE mOCI8BindType;
static VALUE cOCI8BindTypeBase;

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

static void at_exit_func(VALUE val)
{
    oci8_in_finalizer = 1;
}

void
Init_oci8lib()
{
    VALUE cOCI8;
    OCIEnv *envhp;
    OCIError *errhp;
    sword rv;

#ifdef RUNTIME_API_CHECK
    Init_oci8_apiwrap();
    if (have_OCIClientVersion) {
        sword major, minor, update, patch, port_update;
        OCIClientVersion(&major, &minor, &update, &patch, &port_update);
        oracle_client_version = ORAVERNUM(major, minor, update, patch, port_update);
    }
#endif

    oci8_id_new = rb_intern("new");
    oci8_id_get = rb_intern("get");
    oci8_id_set = rb_intern("set");
    oci8_id_keys = rb_intern("keys");
    oci8_id_oci8_class = rb_intern("__oci8_class__");
    rb_set_end_proc(at_exit_func, Qnil);

    Init_oci8_error();
    Init_oci8_env();

    /* OCIHandle class */
    Init_oci8_handle();

    /* OCI8 class */
    cOCI8 = Init_oci8();
    /* OCI8::BindType module */
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    /* OCI8::BindType::Base class */
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", oci8_cOCIHandle);

    /* Handle */
    Init_oci8_bind(cOCI8BindTypeBase);
    Init_oci8_stmt(cOCI8);

    /* Encoding */
    Init_oci8_encoding(cOCI8);

    /* register allocators */
    Init_oci8_metadata(cOCI8);
    Init_oci8_lob(cOCI8);

    /* allocate a temporary errhp to pass Init_oci_number() */
    if (have_OCIEnvCreate) {
        rv = OCIEnvCreate(&envhp, oci8_env_mode, NULL, NULL, NULL, NULL, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_raise_init_error();
        }
    } else {
        rv = OCIInitialize(oci8_env_mode, NULL, NULL, NULL, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_raise_init_error();
        }
        rv = OCIEnvInit(&envhp, OCI_DEFAULT, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_raise_init_error();
        }
    }
    rv = OCIHandleAlloc(envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(envhp, rv);
    Init_oci_number(cOCI8, errhp);
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
    if (have_OCIEnvCreate) {
        OCIHandleFree(envhp, OCI_HTYPE_ENV);
    } else {
        /* Delayed OCIEnv initialization cannot be used on Oracle 8.0. */
        oci8_global_envhp = envhp;
    }

    Init_ora_date();
    Init_oci_datetime();
    Init_oci_object(cOCI8);
    Init_oci_xmldb();

#ifdef USE_WIN32_C
    Init_oci8_win32(cOCI8);
#endif

#ifdef DEBUG_CORE_FILE
    signal(SIGSEGV, SIG_DFL);
#endif

    if (have_OCIEnvCreate && oci8_global_envhp != NULL) {
        rb_raise(rb_eRuntimeError, "Internal Error: OCIEnv should not be initialized here.");
    }
}

VALUE oci8_define_class(const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class(name, oci8_cOCIHandle);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, oci8_id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *base_class)
{
    VALUE klass = rb_define_class_under(outer, name, oci8_cOCIHandle);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, base_class);
    rb_ivar_set(klass, oci8_id_oci8_class, obj);
    return klass;
}

VALUE oci8_define_bind_class(const char *name, const oci8_bind_class_t *bind_class)
{
    VALUE klass = rb_define_class_under(mOCI8BindType, name, cOCI8BindTypeBase);
    VALUE obj = Data_Wrap_Struct(rb_cObject, 0, 0, (void*)bind_class);
    rb_ivar_set(klass, oci8_id_oci8_class, obj);
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

#ifdef HAVE_TYPE_RB_BLOCKING_FUNCTION_T

#if 0
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
#else
static void oci8_unblock_func(void *user_data)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)user_data;
    OCIBreak(svcctx->base.hp.ptr, oci8_errhp);
}
#endif

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
#else /* HAVE_TYPE_RB_BLOCKING_FUNCTION_T */

/* ruby 1.8 */
sword oci8_blocking_region(oci8_svcctx_t *svcctx, rb_blocking_function_t func, void *data)
{
    struct timeval tv;
    sword rv;

    if (!NIL_P(svcctx->executing_thread)) {
        rb_raise(rb_eRuntimeError /* FIXME */, "executing in another thread");
    }
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
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
#endif /* HAVE_TYPE_RB_BLOCKING_FUNCTION_T */

typedef struct {
    oci8_svcctx_t *svcctx;
    const char *sql_text;
    ub4 num_define_vars;
    oci8_exec_sql_var_t *define_vars;
    ub4 num_bind_vars;
    oci8_exec_sql_var_t *bind_vars;
    int raise_on_error;
    OCIStmt *stmtp;
} cb_arg_t;

static VALUE exec_sql(cb_arg_t *arg);
static VALUE ensure_func(cb_arg_t *arg);

/*
 * utility function to execute a single SQL statement
 */
sword oci8_exec_sql(oci8_svcctx_t *svcctx, const char *sql_text, ub4 num_define_vars, oci8_exec_sql_var_t *define_vars, ub4 num_bind_vars, oci8_exec_sql_var_t *bind_vars, int raise_on_error)
{
    cb_arg_t arg;

    oci8_check_pid_consistency(svcctx);
    arg.svcctx = svcctx;
    arg.sql_text = sql_text;
    arg.num_define_vars = num_define_vars;
    arg.define_vars = define_vars;
    arg.num_bind_vars = num_bind_vars;
    arg.bind_vars = bind_vars;
    arg.raise_on_error = raise_on_error;
    arg.stmtp = NULL;
    return (sword)rb_ensure(exec_sql, (VALUE)&arg, ensure_func, (VALUE)&arg);
}

static VALUE exec_sql(cb_arg_t *arg)
{
    ub4 pos;
    sword rv;

    rv = OCIHandleAlloc(oci8_envhp, (dvoid*)&arg->stmtp, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    oci_lc(OCIStmtPrepare(arg->stmtp, oci8_errhp, (text*)arg->sql_text,
                          strlen(arg->sql_text), OCI_NTV_SYNTAX, OCI_DEFAULT));
    for (pos = 0; pos < arg->num_define_vars; pos++) {
        arg->define_vars[pos].hp = NULL;
        oci_lc(OCIDefineByPos(arg->stmtp, (OCIDefine**)&arg->define_vars[pos].hp,
                              oci8_errhp, pos + 1, arg->define_vars[pos].valuep,
                              arg->define_vars[pos].value_sz,
                              arg->define_vars[pos].dty, arg->define_vars[pos].indp,
                              arg->define_vars[pos].alenp, NULL, OCI_DEFAULT));
    }
    for (pos = 0; pos < arg->num_bind_vars; pos++) {
        arg->bind_vars[pos].hp = NULL;
        oci_lc(OCIBindByPos(arg->stmtp, (OCIBind**)&arg->bind_vars[pos].hp,
                            oci8_errhp, pos + 1, arg->bind_vars[pos].valuep,
                            arg->bind_vars[pos].value_sz, arg->bind_vars[pos].dty,
                            arg->bind_vars[pos].indp, arg->bind_vars[pos].alenp,
                            NULL, 0, NULL, OCI_DEFAULT));
    }
    rv = OCIStmtExecute_nb(arg->svcctx, arg->svcctx->base.hp.svc, arg->stmtp, oci8_errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
    if (rv == OCI_ERROR) {
        if (oci8_get_error_code(oci8_errhp) == 1000) {
            /* run GC to close unreferred cursors
             * when ORA-01000 (maximum open cursors exceeded).
             */
            rb_gc();
            rv = OCIStmtExecute_nb(arg->svcctx, arg->svcctx->base.hp.svc, arg->stmtp, oci8_errhp, 1, 0, NULL, NULL, OCI_DEFAULT);
        }
    }
    if (arg->raise_on_error) {
        oci_lc(rv);
    }
    return (VALUE)rv;
}

static VALUE ensure_func(cb_arg_t *arg)
{
    if (arg->stmtp != NULL) {
        OCIHandleFree(arg->stmtp, OCI_HTYPE_STMT);
    }
    return Qnil;
}

#if defined RUNTIME_API_CHECK

#ifndef _WIN32
#include <dlfcn.h>
#endif

void *oci8_find_symbol(const char *symbol_name)
{
#if defined _WIN32
    /* Windows */
    static HMODULE hModule = NULL;

    if (hModule == NULL) {
        hModule = LoadLibrary("OCI.DLL");
        if (hModule == NULL) {
            char message[512];
            int error = GetLastError();
            char *p;

            memset(message, 0, sizeof(message));
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, sizeof(message), NULL);
            for (p = message; *p; p++) {
                if (*p == '\n' || *p == '\r')
                    *p = ' ';
            }
            rb_raise(rb_eLoadError, "OCI.DLL: %d(%s)", error, message);
        }
    }
    return GetProcAddress(hModule, symbol_name);
#else
    /* UNIX */
    static void *handle = NULL;

    if (handle == NULL) {
        static const char * const sonames[] = {
#if defined(__CYGWIN__)
            /* Windows(Cygwin) */
            "OCI.DLL",
#elif defined(_AIX)
            /* AIX */
            "libclntsh.a(shr.o)",
#elif defined(__hppa)
            /* HP-UX(PA-RISC) */
            "libclntsh.sl.11.1",
            "libclntsh.sl.10.1",
            "libclntsh.sl.9.0",
            "libclntsh.sl.8.0",
#elif defined(__APPLE__)
            /* Mac OS X */
            "libclntsh.dylib.11.1",
            "libclntsh.dylib.10.1",
#else
            /* Linux, Solaris and HP-UX(IA64) */
            "libclntsh.so.11.1",
            "libclntsh.so.10.1",
            "libclntsh.so.9.0",
            "libclntsh.so.8.0",
#endif
        };
#define NUM_SONAMES (sizeof(sonames)/sizeof(sonames[0]))
        size_t idx;
        VALUE err = rb_ary_new();

#ifdef _AIX
#define DLOPEN_FLAG (RTLD_LAZY|RTLD_GLOBAL|RTLD_MEMBER)
#else
#define DLOPEN_FLAG (RTLD_LAZY|RTLD_GLOBAL)
#endif
        for (idx = 0; idx < NUM_SONAMES; idx++) {
            handle = dlopen(sonames[idx], DLOPEN_FLAG);
            if (handle != NULL) {
                break;
            }
            rb_ary_push(err, rb_locale_str_new_cstr(dlerror()));
        }
        if (handle == NULL) {
            VALUE msg;

            msg = rb_str_buf_new(NUM_SONAMES * 50);
            for (idx = 0; idx < NUM_SONAMES; idx++) {
                const char *errmsg = RSTRING_PTR(RARRAY_PTR(err)[idx]);
                if (idx != 0) {
                    rb_str_buf_cat2(msg, " ");
                }
                if (strstr(errmsg, sonames[idx]) == NULL) {
                    /* prepend "soname: " if soname is not found in
                     * the error message.
                     */
                    rb_str_buf_cat2(msg, sonames[idx]);
                    rb_str_buf_cat2(msg, ": ");
                }
                rb_str_buf_append(msg, RARRAY_PTR(err)[idx]);
                rb_str_buf_cat2(msg, ";");
            }
            rb_exc_raise(rb_exc_new3(rb_eLoadError, msg));
        }
    }
    return dlsym(handle, symbol_name);
#endif /* defined _WIN32 */
}
#endif /* RUNTIME_API_CHECK */
