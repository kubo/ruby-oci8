/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002-2015 Kubo Takehiro <kubo@jiubao.org>
 */

#include "oci8.h"
#ifdef HAVE_RUBY_THREAD_H
#include <ruby/thread.h>
#endif
#if defined(HAVE_PLTHOOK) && !defined(WIN32)
#include <dlfcn.h>
#include "plthook.h"
#endif

ID oci8_id_at_last_error;
ID oci8_id_get;
ID oci8_id_set;
#ifdef CHAR_IS_NOT_A_SHORTCUT_TO_ID
ID oci8_id_add_op;
ID oci8_id_sub_op;
ID oci8_id_mul_op;
ID oci8_id_div_op;
#endif
int oci8_in_finalizer = 0;
VALUE oci8_cOCIHandle;

#if defined __sun && defined __i386 && defined __GNUC__
/* When a main function is invisible from Oracle instant
 * client 11.2.0.3 for Solaris x86 (32-bit), OCIEnvCreate()
 * fails by unknown reasons. We export it from ruby-oci8 instead
 * of ruby itself.
 */
int main() { return 0; }
#endif

static VALUE mOCI8BindType;
static VALUE cOCI8BindTypeBase;

void oci8_base_free(oci8_base_t *base)
{
    while (base->children != NULL) {
        oci8_base_free(base->children);
    }
    oci8_unlink_from_parent(base);
    if (base->data_type->free != NULL)
        base->data_type->free(base);
    if (base->type >= OCI_DTYPE_FIRST) {
        OCIDescriptorFree(base->hp.ptr, base->type);
    } else if (base->type == OCI_HTYPE_BIND || base->type == OCI_HTYPE_DEFINE) {
        ; /* Do nothing. Bind handles and define handles are freed when
           * associating statement handles are freed.
           */
    } else if (base->type >= OCI_HTYPE_FIRST) {
        OCIHandleFree(base->hp.ptr, base->type);
    }
    base->type = 0;
    base->closed = 1;
    base->hp.ptr = NULL;
}

static void at_exit_func(VALUE val)
{
    oci8_in_finalizer = 1;
#ifdef HAVE_PLTHOOK
    oci8_shutdown_sockets();
#endif
}

static VALUE bind_base_alloc(VALUE klass)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(klass));
}

#if defined(HAVE_PLTHOOK) && !defined(WIN32) && !defined(__CYGWIN__)
static const char *find_libclntsh(void *handle)
{
    void *symaddr = dlsym(handle, "OCIEnvCreate");
    Dl_info info;
#ifdef __APPLE__
    const char *basename = "libclntsh.dylib";
#else
    const char *basename = "libclntsh.so";
#endif
    const char *p;

    if (symaddr == NULL) {
        return NULL;
    }
    if (dladdr(symaddr, &info) == 0) {
        return NULL;
    }
    if ((p = strrchr(info.dli_fname, '/')) == NULL) {
        return NULL;
    }
    if (strncmp(p + 1, basename, strlen(basename)) != 0) {
        return NULL;
    }
    return info.dli_fname;
}

/*
 * Symbol prefix depends on the platform.
 *  Linux x86_64 - no prefix
 *  Linux x86_32 - "_"
 *  macOS        - "@_"
 */
static const char *find_symbol_prefix(plthook_t *ph, size_t *len)
{
    unsigned int pos = 0;
    const char *name;
    void **addr;

    while (plthook_enum(ph, &pos, &name, &addr) == 0) {
        const char *p = strstr(name, "OCIEnvCreate");
        if (p != NULL) {
            *len = p - name;
            return name;
        }
    }
    return NULL;
}

/*
 * Fix PLT entries against function interposition.
 * See: http://www.rubydoc.info/github/kubo/ruby-oci8/file/docs/ldap-auth-and-function-interposition.md
 */
static void rebind_internal_symbols(void)
{
    const char *libfile;
    void *handle;
    int flags = RTLD_LAZY | RTLD_NOLOAD;
    plthook_t *ph;
    unsigned int pos = 0;
    const char *name;
    void **addr;
    const char *prefix;
    size_t prefix_len;

#ifdef RTLD_FIRST
    flags |= RTLD_FIRST; /* for macOS */
#endif

    libfile = find_libclntsh(RTLD_DEFAULT); /* normal case */
    if (libfile == NULL) {
        libfile = find_libclntsh(RTLD_NEXT); /* special case when OCIEnvCreate is hooked by LD_PRELOAD */
    }
    if (libfile == NULL) {
        return;
    }
    handle = dlopen(libfile, flags);
    if (handle == NULL) {
        return;
    }
    if (plthook_open(&ph, libfile) != 0) {
        dlclose(handle);
        return;
    }
    prefix = find_symbol_prefix(ph, &prefix_len);
    if (prefix == NULL) {
        dlclose(handle);
        plthook_close(ph);
        return;
    }
    while (plthook_enum(ph, &pos, &name, &addr) == 0) {
        void *funcaddr;
        if (prefix_len != 0) {
            if (strncmp(name, prefix, prefix_len) != 0) {
                continue;
            }
            name += prefix_len;
        }
        if (strncmp(name, "OCI", 3) == 0) {
            /* exclude functions starting with OCI not to prevent LD_PRELOAD hooking */
            continue;
        }
        funcaddr = dlsym(handle, name);
        if (funcaddr != NULL && *addr != funcaddr) {
            /* If libclntsh.so exports and imports same functions, their
             * PLT entries are forcedly modified to point to itself not
             * to use functions in other libraries.
             */
            *addr = funcaddr;
        }
    }
    plthook_close(ph);
    dlclose(handle);
}
#endif

#ifdef _WIN32
__declspec(dllexport)
#endif
void
Init_oci8lib()
{
    VALUE cOCI8;
    OCIEnv *envhp = NULL;
    OCIError *errhp;
    sword rv;

#ifdef RUNTIME_API_CHECK
    Init_oci8_apiwrap();
    if (oracle_client_version < ORAVER_10_1) {
        const char *oraver;
        const char *ruby_oci8_ver;
        if (oracle_client_version >= ORAVER_9_2) {
            oraver = "9iR2";
            ruby_oci8_ver = "2.1.x";
        } else if (oracle_client_version >= ORAVER_9_0) {
            oraver = "9iR1";
            ruby_oci8_ver = "2.1.x";
        } else if (oracle_client_version >= ORAVER_8_1) {
            oraver = "8i";
            ruby_oci8_ver = "2.0.x";
        } else {
            oraver = "8";
            ruby_oci8_ver = "2.0.x";
        }
        rb_raise(rb_eLoadError, "Ruby-oci8 %s doesn't support Oracle %s. Use ruby-oci8 %s instead.",
                 OCI8LIB_VERSION, oraver, ruby_oci8_ver);
    }

    if (have_OCIClientVersion) {
        sword major, minor, update, patch, port_update;
        OCIClientVersion(&major, &minor, &update, &patch, &port_update);
        oracle_client_version = ORAVERNUM(major, minor, update, patch, port_update);
    }
#endif
#if defined(HAVE_PLTHOOK) && !defined(WIN32) && !defined(__CYGWIN__)
    rebind_internal_symbols();
#endif

    oci8_id_at_last_error = rb_intern("@last_error");
    oci8_id_get = rb_intern("get");
    oci8_id_set = rb_intern("set");
#ifdef CHAR_IS_NOT_A_SHORTCUT_TO_ID
    oci8_id_add_op = rb_intern("+");
    oci8_id_sub_op = rb_intern("-");
    oci8_id_mul_op = rb_intern("*");
    oci8_id_div_op = rb_intern("/");
#endif
    rb_set_end_proc(at_exit_func, Qnil);

    Init_oci8_thread_util();
    Init_oci8_error();
    Init_oci8_env();

    /* OCIHandle class */
    Init_oci8_handle();

    /* OCI8 class */
    Init_oci8(&cOCI8);

    /* OCI8::ConnectionPool class */
    Init_oci8_connection_pool(cOCI8);

    /* OCI8::BindType module */
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    /* OCI8::BindType::Base class */
    cOCI8BindTypeBase = oci8_define_class_under(mOCI8BindType, "Base", &oci8_bind_data_type, bind_base_alloc);

    /* Handle */
    Init_oci8_bind(cOCI8BindTypeBase);
    Init_oci8_stmt(cOCI8);

    /* Encoding */
    Init_oci8_encoding(cOCI8);

    /* register allocators */
    Init_oci8_metadata(cOCI8);
    Init_oci8_lob(cOCI8);

    /* OCI8::Util */
    Init_oci8_util(cOCI8);

    /* allocate a temporary errhp to pass Init_oci_number() */
    rv = OCIEnvCreate(&envhp, oci8_env_mode, NULL, NULL, NULL, NULL, 0, NULL);
    if (rv != OCI_SUCCESS) {
        if (envhp != NULL) {
            oci8_env_free_and_raise(envhp, rv);
        } else {
            oci8_raise_init_error();
        }
    }
    rv = OCIHandleAlloc(envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(envhp, rv);
    Init_oci_number(cOCI8, errhp);
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
    OCIHandleFree(envhp, OCI_HTYPE_ENV);

    Init_ora_date();
    Init_oci_datetime();
    Init_oci_object(cOCI8);

#ifdef USE_WIN32_C
    Init_oci8_win32(cOCI8);
#endif
}

VALUE oci8_define_class(const char *name, const oci8_handle_data_type_t *data_type, VALUE (*alloc_func)(VALUE))
{
    VALUE parent_class = rb_eval_string(data_type->rb_data_type.parent->wrap_struct_name);
    VALUE klass = rb_define_class(name, parent_class);

    rb_define_alloc_func(klass, alloc_func);
    return klass;
}

VALUE oci8_define_class_under(VALUE outer, const char *name, const oci8_handle_data_type_t *data_type, VALUE (*alloc_func)(VALUE))
{
    VALUE parent_class = rb_eval_string(data_type->rb_data_type.parent->wrap_struct_name);
    VALUE klass = rb_define_class_under(outer, name, parent_class);

    rb_define_alloc_func(klass, alloc_func);
    return klass;
}

VALUE oci8_define_bind_class(const char *name, const oci8_bind_data_type_t *data_type, VALUE (*alloc_func)(VALUE))
{
    VALUE parent_class = rb_eval_string(data_type->base.rb_data_type.parent->wrap_struct_name);
    VALUE klass = rb_define_class_under(mOCI8BindType, name, parent_class);

    rb_define_alloc_func(klass, alloc_func);
    return klass;
}

void oci8_link_to_parent(oci8_base_t *base, oci8_base_t *parent)
{
    VALUE old_parent = Qundef;

    if (base->parent != NULL) {
        old_parent = base->parent->self;
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
    RB_OBJ_WRITTEN(parent->self, Qundef, base->self);
    RB_OBJ_WRITTEN(base->self, old_parent, parent->self);
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

static void oci8_unblock_func(void *user_data)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)user_data;
    OCIBreak(svcctx->base.hp.ptr, oci8_errhp);
}

typedef struct free_temp_lob_arg_t {
    oci8_svcctx_t *svcctx;
    OCISvcCtx *svchp;
    OCIError *errhp;
    OCILobLocator *lob;
} free_temp_lob_arg_t;

static void *free_temp_lob(void *user_data)
{
    free_temp_lob_arg_t *data = (free_temp_lob_arg_t *)user_data;
    sword rv = OCILobFreeTemporary(data->svchp, data->errhp, data->lob);

    data->svcctx->executing_thread = Qnil;
    return (void*)(VALUE)rv;
}

typedef struct protected_call_arg {
    void *(*func)(void *);
    void *data;
    oci8_svcctx_t *svcctx;
} protected_call_arg_t;

static VALUE protected_call(VALUE data)
{
    struct protected_call_arg *parg = (struct protected_call_arg*)data;
    VALUE rv;

    if (!NIL_P(parg->svcctx->executing_thread)) {
        rb_raise(rb_eRuntimeError, "executing in another thread");
    }
    RB_OBJ_WRITE(parg->svcctx->base.self, &parg->svcctx->executing_thread, rb_thread_current());
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
    rv = (VALUE)rb_thread_call_without_gvl(parg->func, parg->data, oci8_unblock_func, parg->svcctx);
#else
    rv = rb_thread_blocking_region((VALUE(*)(void*))parg->func, parg->data, oci8_unblock_func, parg->svcctx);
#endif
    if ((sword)rv == OCI_ERROR) {
        if (oci8_get_error_code(oci8_errhp) == 1013) {
            rb_raise(eOCIBreak, "Canceled by user request.");
        }
    }
    return rv;
}

sword oci8_call_without_gvl(oci8_svcctx_t *svcctx, void *(*func)(void *), void *data)
{
    OCIError *errhp = oci8_errhp;
    protected_call_arg_t parg;
    sword rv;
    int state;

    if (!NIL_P(svcctx->executing_thread)) {
        rb_raise(rb_eRuntimeError, "executing in another thread");
    }
    if (!svcctx->suppress_free_temp_lobs) {
        oci8_temp_lob_t *lob;
        while ((lob = svcctx->temp_lobs) != NULL) {
            svcctx->temp_lobs = lob->next;

            if (svcctx->non_blocking) {
                free_temp_lob_arg_t arg;

                arg.svcctx = svcctx;
                arg.svchp = svcctx->base.hp.svc;
                arg.errhp = errhp;
                arg.lob = lob->lob;

                parg.svcctx = svcctx;
                parg.func = free_temp_lob;
                parg.data = &arg;

                rb_protect(protected_call, (VALUE)&parg, &state);
                if (state) {
                    lob->next = svcctx->temp_lobs;
                    svcctx->temp_lobs = lob;
                    rb_jump_tag(state);
                }
            } else {
                OCILobFreeTemporary(svcctx->base.hp.svc, errhp, lob->lob);
            }
            OCIDescriptorFree(lob->lob, OCI_DTYPE_LOB);
            xfree(lob);
        }
    }

    if (svcctx->non_blocking) {
        parg.svcctx = svcctx;
        parg.func = func;
        parg.data = data;
        rv = (sword)rb_protect(protected_call, (VALUE)&parg, &state);
        if (state) {
            rb_jump_tag(state);
        }
        return rv;
    } else {
        return (sword)(VALUE)func(data);
    }
}

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

    chker2(OCIStmtPrepare2(arg->svcctx->base.hp.svc, &arg->stmtp, oci8_errhp, 
                           (text*)arg->sql_text, strlen(arg->sql_text), NULL, 0,
                           OCI_NTV_SYNTAX, OCI_DEFAULT),
           &arg->svcctx->base);
    for (pos = 0; pos < arg->num_define_vars; pos++) {
        arg->define_vars[pos].hp = NULL;
        chker3(OCIDefineByPos(arg->stmtp, (OCIDefine**)&arg->define_vars[pos].hp,
                              oci8_errhp, pos + 1, arg->define_vars[pos].valuep,
                              arg->define_vars[pos].value_sz,
                              arg->define_vars[pos].dty, arg->define_vars[pos].indp,
                              arg->define_vars[pos].alenp, NULL, OCI_DEFAULT),
               &arg->svcctx->base, arg->stmtp);
    }
    for (pos = 0; pos < arg->num_bind_vars; pos++) {
        arg->bind_vars[pos].hp = NULL;
        chker3(OCIBindByPos(arg->stmtp, (OCIBind**)&arg->bind_vars[pos].hp,
                            oci8_errhp, pos + 1, arg->bind_vars[pos].valuep,
                            arg->bind_vars[pos].value_sz, arg->bind_vars[pos].dty,
                            arg->bind_vars[pos].indp, arg->bind_vars[pos].alenp,
                            NULL, 0, NULL, OCI_DEFAULT),
               &arg->svcctx->base, arg->stmtp);
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
        chker3(rv, &arg->svcctx->base, arg->stmtp);
    }
    return (VALUE)rv;
}

static VALUE ensure_func(cb_arg_t *arg)
{
    if (arg->stmtp != NULL) {
        OCIStmtRelease(arg->stmtp, oci8_errhp, NULL, 0, OCI_DEFAULT);
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
            "libclntsh.sl.12.1",
            "libclntsh.sl.11.1",
            "libclntsh.sl.10.1",
            "libclntsh.sl.9.0",
            "libclntsh.sl.8.0",
#elif defined(__APPLE__)
            /* Mac OS X */
            "libclntsh.dylib.12.1",
            "libclntsh.dylib.11.1",
            "libclntsh.dylib.10.1",
#else
            /* Linux, Solaris and HP-UX(IA64) */
            "libclntsh.so.12.1",
            "libclntsh.so.11.1",
            "libclntsh.so.10.1",
            "libclntsh.so.9.0",
            "libclntsh.so.8.0",
#endif
        };
#define NUM_SONAMES (sizeof(sonames)/sizeof(sonames[0]))
        size_t idx;
        volatile VALUE err = rb_ary_new();

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
            const VALUE *arr = RARRAY_CONST_PTR(err);

            msg = rb_str_buf_new(NUM_SONAMES * 50);
            for (idx = 0; idx < NUM_SONAMES; idx++) {
                const char *errmsg = RSTRING_PTR(arr[idx]);
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
                rb_str_buf_append(msg, arr[idx]);
                rb_str_buf_cat2(msg, ";");
            }
            rb_exc_raise(rb_exc_new3(rb_eLoadError, msg));
        }
    }
    return dlsym(handle, symbol_name);
#endif /* defined _WIN32 */
}
#endif /* RUNTIME_API_CHECK */

void *oci8_check_typeddata(VALUE obj, const oci8_handle_data_type_t *data_type, int error_if_closed)
{
#ifdef HAVE_RB_DATA_TYPE_T_FUNCTION
    oci8_base_t *hp = Check_TypedStruct(obj, &data_type->rb_data_type);
#else
    oci8_base_t *hp;
    const char *expected_type_name = data_type->rb_data_type.wrap_struct_name;
    const rb_data_type_t *rb_data_type;
    const rb_data_type_t *expected_rb_data_type = &data_type->rb_data_type;

    if (TYPE(obj) != T_DATA || !rb_obj_is_kind_of(obj, oci8_cOCIHandle)) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected %s)",
                 rb_obj_classname(obj), expected_type_name);
    }
    hp = DATA_PTR(obj);
    rb_data_type = &hp->data_type->rb_data_type;
    while (rb_data_type != expected_rb_data_type) {
        if (rb_data_type == NULL) {
            rb_raise(rb_eTypeError, "wrong argument type %s (expected %s)",
                     rb_obj_classname(obj), expected_type_name);
        }
        rb_data_type = rb_data_type->parent;
    }
#endif
    if (error_if_closed && hp->closed) {
        rb_raise(eOCIException, "%s was already closed.",
                 rb_obj_classname(obj));
    }
    return hp;
}
