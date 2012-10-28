/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2011 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* getpid() */
#endif

#ifdef WIN32
#ifndef getpid
extern rb_pid_t rb_w32_getpid(void);
#define getpid() rb_w32_getpid()
#endif
#endif

#ifndef OCI_ATTR_CLIENT_IDENTIFIER
#define OCI_ATTR_CLIENT_IDENTIFIER 278
#endif
#ifndef OCI_ATTR_MODULE
#define OCI_ATTR_MODULE 366
#endif
#ifndef OCI_ATTR_ACTION
#define OCI_ATTR_ACTION 367
#endif
#ifndef OCI_ATTR_CLIENT_INFO
#define OCI_ATTR_CLIENT_INFO 368
#endif

#define OCI8_STATE_SESSION_BEGIN_WAS_CALLED 0x01
#define OCI8_STATE_SERVER_ATTACH_WAS_CALLED 0x02
#define OCI8_STATE_CPOOL 0x04

static VALUE cOCI8;
static VALUE cSession;
static VALUE cServer;
static ID id_at_session_handle;
static ID id_at_server_handle;

static VALUE dummy_env_method_missing(int argc, VALUE *argv, VALUE self)
{
    VALUE obj = rb_cv_get(cOCI8, "@@environment_handle");
    VALUE method_id, args;

    if (self == obj) {
        oci8_base_t *base;
        obj = rb_obj_alloc(oci8_cOCIHandle);
        base = DATA_PTR(obj);
        base->type = OCI_HTYPE_ENV;
        base->hp.ptr = oci8_envhp;
        base->self = Qnil;
        rb_cv_set(cOCI8, "@@environment_handle", obj);
    }

    rb_scan_args(argc, argv, "1*", &method_id, &args);
    Check_Type(method_id, T_SYMBOL);
    return rb_apply(obj, SYM2ID(method_id), args);
}

typedef struct oci8_svcctx_associate {
    oci8_base_t base;
    oci8_svcctx_t *svcctx;
} oci8_svcctx_associate_t;

static void oci8_svcctx_associate_free(oci8_base_t *base)
{
    base->type = 0;
    base->hp.ptr = NULL;
}

static oci8_base_vtable_t oci8_svcctx_associate_vtable = {
    NULL,
    oci8_svcctx_associate_free,
    sizeof(oci8_svcctx_associate_t),
};

static void copy_session_handle(oci8_svcctx_t *svcctx)
{
    VALUE obj = rb_ivar_get(svcctx->base.self, id_at_session_handle);
    oci8_base_t *base;

    Check_Handle(obj, cSession, base);
    base->type = OCI_HTYPE_SESSION;
    base->hp.usrhp = svcctx->usrhp;
}

static void copy_server_handle(oci8_svcctx_t *svcctx)
{
    VALUE obj = rb_ivar_get(svcctx->base.self, id_at_server_handle);
    oci8_base_t *base;

    Check_Handle(obj, cServer, base);
    base->type = OCI_HTYPE_SERVER;
    base->hp.srvhp = svcctx->srvhp;
}

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;
    oci8_temp_lob_t *lob;

    lob = svcctx->temp_lobs;
    while (lob != NULL) {
        oci8_temp_lob_t *lob_next = lob->next;

        OCIDescriptorFree(lob->lob, OCI_DTYPE_LOB);
        xfree(lob);
        lob = lob_next;
    }
    svcctx->temp_lobs = NULL;

    if (svcctx->logoff_strategy != NULL) {
        const oci8_logoff_strategy_t *strategy = svcctx->logoff_strategy;
        void *data = strategy->prepare(svcctx);
        int rv;
        svcctx->base.type = 0;
        svcctx->logoff_strategy = NULL;
        rv = oci8_run_native_thread(strategy->execute, data);
        if (rv != 0) {
            errno = rv;
#ifdef WIN32
            rb_sys_fail("_beginthread");
#else
            rb_sys_fail("pthread_create");
#endif
        }
    }
    svcctx->base.type = 0;
}

static void oci8_svcctx_init(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;
    VALUE obj;

    svcctx->executing_thread = Qnil;
    /* set session handle */
    obj = rb_obj_alloc(cSession);
    rb_ivar_set(base->self, id_at_session_handle, obj);
    oci8_link_to_parent(DATA_PTR(obj), base);
    /* set server handle */
    obj = rb_obj_alloc(cServer);
    rb_ivar_set(base->self, id_at_server_handle, obj);
    oci8_link_to_parent(DATA_PTR(obj), base);

    svcctx->pid = getpid();
    svcctx->is_autocommit = 0;
#ifdef NATIVE_THREAD_WITH_GVL
    svcctx->non_blocking = 1;
#endif
    svcctx->long_read_len = INT2FIX(65535);
}

static oci8_base_vtable_t oci8_svcctx_vtable = {
    NULL,
    oci8_svcctx_free,
    sizeof(oci8_svcctx_t),
    oci8_svcctx_init,
};

static VALUE oracle_client_vernum; /* Oracle client version number */
static ID id_at_prefetch_rows;
static ID id_set_prefetch_rows;

static VALUE oci8_s_oracle_client_vernum(VALUE klass)
{
    return oracle_client_vernum;
}

static VALUE oci8_s_set_property(VALUE klass, VALUE name, VALUE val)
{
    const char *name_str;

    Check_Type(name, T_SYMBOL);
    name_str = rb_id2name(SYM2ID(name));
    if (strcmp(name_str, "float_conversion_type") == 0) {
        const char *val_str;
        Check_Type(val, T_SYMBOL);
        val_str = rb_id2name(SYM2ID(val));
        if (strcmp(val_str, "ruby") == 0) {
            oci8_float_conversion_type_is_ruby = 1;
        } else if (strcmp(val_str, "oracle") == 0) {
            oci8_float_conversion_type_is_ruby = 0;
        } else {
            rb_raise(rb_eArgError, "float_conversion_type's value should be either :ruby or :oracle.");
        }
    }
    return Qnil;
}

/*
 * call-seq:
 *   OCI8.error_message(message_no) -> string
 *
 * Get the Oracle error message specified by message_no.
 * Its language depends on NLS_LANGUAGE.
 *
 * Note: This method is unavailable if the Oracle client version is 8.0.
 *
 * example:
 *   # When NLS_LANG is AMERICAN_AMERICA.AL32UTF8
 *   OCI8.error_message(1) # => "ORA-00001: unique constraint (%s.%s) violated"
 *   
 *   # When NLS_LANG is FRENCH_FRANCE.AL32UTF8
 *   OCI8.error_message(1) # => "ORA-00001: violation de contrainte unique (%s.%s)"
 *
 */
static VALUE oci8_s_error_message(VALUE klass, VALUE msgid)
{
    return oci8_get_error_message(NUM2UINT(msgid), NULL);
}

#define CONN_STR_REGEX "/^([^(\\s|\\@)]*)\\/([^(\\s|\\@)]*)(?:\\@(\\S+))?(?:\\s+as\\s+(\\S*)\\s*)?$/i"
void oci8_do_parse_connect_string(VALUE conn_str, VALUE *user, VALUE *pass, VALUE *dbname, VALUE *mode)
{
    static VALUE re = Qnil;
    if (NIL_P(re)) {
        re = rb_eval_string(CONN_STR_REGEX);
        rb_global_variable(&re);
    }
    OCI8SafeStringValue(conn_str);
    if (RTEST(rb_reg_match(re, conn_str))) {
        *user = rb_reg_nth_match(1, rb_backref_get());
        *pass = rb_reg_nth_match(2, rb_backref_get());
        *dbname = rb_reg_nth_match(3, rb_backref_get());
        *mode = rb_reg_nth_match(4, rb_backref_get());
        if (RSTRING_LEN(*user) == 0 && RSTRING_LEN(*pass) == 0) {
            /* external credential */
            *user = Qnil;
            *pass = Qnil;
        }
        if (!NIL_P(*mode)) {
            *mode = ID2SYM(rb_to_id(rb_funcall(*mode, rb_intern("upcase"), 0)));
        }
    } else {
        rb_raise(rb_eArgError, "invalid connect string \"%s\" (expect \"username/password[@(tns_name|//host[:port]/service_name)][ as (sysdba|sysoper)]\"", RSTRING_PTR(conn_str));
    }
}

/*
 * call-seq:
 *   parse_connect_string(string) -> [username, password, dbname, privilege]
 *
 * Extracts +username+, +password+, +dbname+ and +privilege+ from the specified +string+.
 *
 * example:
 *   "scott/tiger" -> ["scott", "tiger", nil, nil],
 *   "scott/tiger@oradb.example.com" -> ["scott", "tiger", "oradb.example.com", nil]
 *   "sys/change_on_install as sysdba" -> ["sys", "change_on_install", nil, :SYSDBA]
 *
 */
static VALUE oci8_parse_connect_string(VALUE self, VALUE conn_str)
{
    VALUE user;
    VALUE pass;
    VALUE dbname;
    VALUE mode;

    oci8_do_parse_connect_string(conn_str, &user, &pass, &dbname, &mode);
    return rb_ary_new3(4, user, pass, dbname, mode);
}

/*
 * Logoff strategy for sessions connected by OCILogon.
 */
typedef struct {
    OCISvcCtx *svchp;
    OCISession *usrhp;
    OCIServer *srvhp;
} simple_logoff_arg_t;

static void *simple_logoff_prepare(oci8_svcctx_t *svcctx)
{
    simple_logoff_arg_t *sla = xmalloc(sizeof(simple_logoff_arg_t));
    sla->svchp = svcctx->base.hp.svc;
    sla->usrhp = svcctx->usrhp;
    sla->srvhp = svcctx->srvhp;
    svcctx->usrhp = NULL;
    svcctx->srvhp = NULL;
    return sla;
}

static void *simple_logoff_execute(void *arg)
{
    simple_logoff_arg_t *sla = (simple_logoff_arg_t *)arg;
    OCIError *errhp = oci8_errhp;
    sword rv;

    OCITransRollback(sla->svchp, errhp, OCI_DEFAULT);
    rv = OCILogoff(sla->svchp, errhp);
    xfree(sla);
    return (void*)(VALUE)rv;
}

static const oci8_logoff_strategy_t simple_logoff = {
    simple_logoff_prepare,
    simple_logoff_execute,
};

/*
 * Logoff strategy for sessions connected by OCIServerAttach and OCISessionBegin.
 */

typedef struct {
    OCISvcCtx *svchp;
    OCISession *usrhp;
    OCIServer *srvhp;
    unsigned char state;
} complex_logoff_arg_t;

static void *complex_logoff_prepare(oci8_svcctx_t *svcctx)
{
    complex_logoff_arg_t *cla = xmalloc(sizeof(complex_logoff_arg_t));
    cla->svchp = svcctx->base.hp.svc;
    cla->usrhp = svcctx->usrhp;
    cla->srvhp = svcctx->srvhp;
    cla->state = svcctx->state;
    svcctx->usrhp = NULL;
    svcctx->srvhp = NULL;
    svcctx->state = 0;
    return cla;
}

static void *complex_logoff_execute(void *arg)
{
    complex_logoff_arg_t *cla = (complex_logoff_arg_t *)arg;
    OCIError *errhp = oci8_errhp;
    sword rv = OCI_SUCCESS;

    OCITransRollback(cla->svchp, errhp, OCI_DEFAULT);

    if (cla->state & OCI8_STATE_SESSION_BEGIN_WAS_CALLED) {
        rv = OCISessionEnd(cla->svchp, oci8_errhp, cla->usrhp, OCI_DEFAULT);
        cla->state &= ~OCI8_STATE_SESSION_BEGIN_WAS_CALLED;
    }
    if (cla->state & OCI8_STATE_SERVER_ATTACH_WAS_CALLED) {
        rv = OCIServerDetach(cla->srvhp, oci8_errhp, OCI_DEFAULT);
        cla->state &= ~OCI8_STATE_SERVER_ATTACH_WAS_CALLED;
    }
    if (cla->usrhp != NULL) {
        OCIHandleFree(cla->usrhp, OCI_HTYPE_SESSION);
    }
    if (cla->srvhp != NULL) {
        OCIHandleFree(cla->srvhp, OCI_HTYPE_SERVER);
    }
    if (cla->svchp != NULL) {
        OCIHandleFree(cla->svchp, OCI_HTYPE_SVCCTX);
    }
    xfree(cla);
    return (void*)(VALUE)rv;
}

static const oci8_logoff_strategy_t complex_logoff = {
    complex_logoff_prepare,
    complex_logoff_execute,
};

/*
 * call-seq:
 *   logon2(username, password, dbname, mode) -> connection
 *
 * <b>internal use only</b>
 *
 * Creates a simple logon session by the OCI function OCILogon2().
 */
static VALUE oci8_logon2(VALUE self, VALUE username, VALUE password, VALUE dbname, VALUE mode)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    ub4 logon2_mode;

    if (svcctx->logoff_strategy != NULL) {
        rb_raise(rb_eRuntimeError, "Could not reuse the session.");
    }

    /* check arugmnets */
    OCI8SafeStringValue(username);
    OCI8SafeStringValue(password);
    if (!NIL_P(dbname)) {
        OCI8SafeStringValue(dbname);
    }
    logon2_mode = NUM2UINT(mode);

    /* logon */
    svcctx->base.type = OCI_HTYPE_SVCCTX;
    chker2(OCILogon2_nb(svcctx, oci8_envhp, oci8_errhp, &svcctx->base.hp.svc,
                        RSTRING_ORATEXT(username), RSTRING_LEN(username),
                        RSTRING_ORATEXT(password), RSTRING_LEN(password),
                        NIL_P(dbname) ? NULL : RSTRING_ORATEXT(dbname),
                        NIL_P(dbname) ? 0 : RSTRING_LEN(dbname), logon2_mode),
           &svcctx->base);
    svcctx->logoff_strategy = &simple_logoff;

    if (logon2_mode & OCI_LOGON2_CPOOL) {
        svcctx->state |= OCI8_STATE_CPOOL;
    }

    /* setup the session handle */
    chker2(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->usrhp, 0, OCI_ATTR_SESSION, oci8_errhp),
           &svcctx->base);
    copy_session_handle(svcctx);

    /* setup the server handle */
    chker2(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp),
           &svcctx->base);
    copy_server_handle(svcctx);

    return Qnil;
}

/*
 * call-seq:
 *   allocate_handles()
 *
 * <b>internal use only</b>
 *
 * Allocates a service context handle, a session handle and a
 * server handle to use explicit attach and begin-session calls.
 */
static VALUE oci8_allocate_handles(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;

    if (svcctx->logoff_strategy != NULL) {
        rb_raise(rb_eRuntimeError, "Could not reuse the session.");
    }
    svcctx->logoff_strategy = &complex_logoff;
    svcctx->state = 0;

    /* allocate a service context handle */
    rv = OCIHandleAlloc(oci8_envhp, &svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    svcctx->base.type = OCI_HTYPE_SVCCTX;

    /* alocalte a session handle */
    rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->usrhp, OCI_HTYPE_SESSION, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    copy_session_handle(svcctx);

    /* alocalte a server handle */
    rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    copy_server_handle(svcctx);
    return self;
}

/*
 * call-seq:
 *   server_attach(dbname, mode)
 *
 * <b>internal use only</b>
 *
 * Attachs to the server by the OCI function OCIServerAttach().
 */
static VALUE oci8_server_attach(VALUE self, VALUE dbname, VALUE attach_mode)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    ub4 mode = NUM2UINT(attach_mode);

    if (svcctx->logoff_strategy != &complex_logoff) {
        rb_raise(rb_eRuntimeError, "Use this method only for the service context handle created by OCI8#server_handle().");
    }
    if (svcctx->state & OCI8_STATE_SERVER_ATTACH_WAS_CALLED) {
        rb_raise(rb_eRuntimeError, "Could not use this method twice.");
    }

    /* check arguments */
    if (!NIL_P(dbname)) {
        OCI8SafeStringValue(dbname);
    }

    /* attach to the server */
    chker2(OCIServerAttach_nb(svcctx, svcctx->srvhp, oci8_errhp,
                              NIL_P(dbname) ? NULL : RSTRING_ORATEXT(dbname),
                              NIL_P(dbname) ? 0 : RSTRING_LEN(dbname),
                              mode),
           &svcctx->base);
    chker2(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX,
                      svcctx->srvhp, 0, OCI_ATTR_SERVER,
                      oci8_errhp),
           &svcctx->base);
    svcctx->state |= OCI8_STATE_SERVER_ATTACH_WAS_CALLED;
    if (mode & OCI_CPOOL) {
        svcctx->state |= OCI8_STATE_CPOOL;
    }
    return self;
}

/*
 * call-seq:
 *   session_begin(cred, mode)
 *
 * <b>internal use only</b>
 *
 * Begins the session by the OCI function OCISessionBegin().
 */
static VALUE oci8_session_begin(VALUE self, VALUE cred, VALUE mode)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);

    if (svcctx->logoff_strategy != &complex_logoff) {
        rb_raise(rb_eRuntimeError, "Use this method only for the service context handle created by OCI8#server_handle().");
    }
    if (svcctx->state & OCI8_STATE_SESSION_BEGIN_WAS_CALLED) {
        rb_raise(rb_eRuntimeError, "Could not use this method twice.");
    }

    /* check arguments */
    Check_Type(cred, T_FIXNUM);
    Check_Type(mode, T_FIXNUM);

    /* begin session */
    chker2(OCISessionBegin_nb(svcctx, svcctx->base.hp.ptr, oci8_errhp,
                              svcctx->usrhp, FIX2UINT(cred),
                              FIX2UINT(mode)),
           &svcctx->base);
    chker2(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX,
                      svcctx->usrhp, 0, OCI_ATTR_SESSION,
                      oci8_errhp),
           &svcctx->base);
    svcctx->state |= OCI8_STATE_SESSION_BEGIN_WAS_CALLED;
    return Qnil;
}

/*
 * call-seq:
 *   logoff
 *
 * Disconnects from the Oracle server. The uncommitted transaction is
 * rollbacked.
 */
static VALUE oci8_svcctx_logoff(VALUE self)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)DATA_PTR(self);

    while (svcctx->base.children != NULL) {
        oci8_base_free(svcctx->base.children);
    }
    if (svcctx->logoff_strategy != NULL) {
        const oci8_logoff_strategy_t *strategy = svcctx->logoff_strategy;
        void *data = strategy->prepare(svcctx);
        svcctx->base.type = 0;
        svcctx->logoff_strategy = NULL;
        chker2(oci8_call_without_gvl(svcctx, strategy->execute, data), &svcctx->base);
    }
    return Qtrue;
}

/*
 * call-seq:
 *   commit
 *
 * Commits the transaction.
 */
static VALUE oci8_commit(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    chker2(OCITransCommit_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT), &svcctx->base);
    return self;
}

/*
 * call-seq:
 *   rollback
 *
 * Rollbacks the transaction.
 */
static VALUE oci8_rollback(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    chker2(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT), &svcctx->base);
    return self;
}

/*
 * call-seq:
 *   non_blocking? -> true or false
 *
 * Returns +true+ if the connection is in non-blocking mode, +false+
 * otherwise.
 *
 * See also #non_blocking=.
 */
static VALUE oci8_non_blocking_p(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
#ifdef NATIVE_THREAD_WITH_GVL
    return svcctx->non_blocking ? Qtrue : Qfalse;
#else
    sb1 non_blocking;

    chker2(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp), &svcctx->base);
    return non_blocking ? Qtrue : Qfalse;
#endif
}

/*
 * call-seq:
 *   non_blocking = true or false
 *
 * Sets +true+ to enable non-blocking mode, +false+ otherwise.
 * The default setting depends on the ruby version and ruby-oci8
 * version.
 *
 * When the connection is in blocking mode (non_blocking = false),
 * SQL executions block not only the thread, but also the ruby
 * process. It makes the whole application stop while a SQL execution
 * needs long time.
 *
 * When in non-blocking mode (non_blocking = true), SQL executions
 * block only the thread. It does't prevent other threads.
 * A SQL execution which blocks a thread can be canceled by
 * OCI8#break.
 *
 * === ruby 1.9
 * The default setting is +true+ if the ruby-oci8 version is 2.0.3 or
 * upper, +false+ otherwise.
 *
 * Ruby-oci8 makes the connection non-blocking by releasing ruby
 * interpreter's GVL (Global VM Lock or Giant VM Lock) while OCI
 * functions which may need more than one network round trips are in
 * execution.
 *
 * === ruby 1.8
 * The default setting is +false+.
 *
 * Ruby-oci8 makes the connection non-blocking by polling the return
 * values of OCI functions. When an OCI function returns
 * OCI_STILL_EXECUTING, the thread sleeps for 10 milli seconds to make
 * a time for other threads to run. The sleep time is doubled up to
 * 640 milli seconds as the function returns the same value.
 *
 */
static VALUE oci8_set_non_blocking(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
#ifdef NATIVE_THREAD_WITH_GVL
    svcctx->non_blocking = RTEST(val);
#else
    sb1 non_blocking;

    if (svcctx->state & OCI8_STATE_CPOOL) {
        rb_raise(rb_eRuntimeError, "Could not set non-blocking mode to a connection allocated from OCI8::ConnectionPool.");
    }
    chker2(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp), &svcctx->base);
    if ((RTEST(val) && !non_blocking) || (!RTEST(val) && non_blocking)) {
        /* toggle blocking / non-blocking. */
        chker2(OCIAttrSet(svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp), &svcctx->base);
    }
#endif
    return val;
}

/*
 * call-seq:
 *   autocommit? -> true or false
 *
 * Returns +true+ if the connection is in autocommit mode, +false+
 * otherwise. The default value is +false+.
 */
static VALUE oci8_autocommit_p(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    return svcctx->is_autocommit ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   autocommit = true or false
 *
 * Sets the autocommit mode. The default value is +false+.
 */
static VALUE oci8_set_autocommit(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    svcctx->is_autocommit = RTEST(val);
    return val;
}

/*
 * call-seq:
 *   long_read_len -> fixnum
 *
 * Gets the maximum length in bytes to fetch a LONG or LONG RAW
 * column. The default value is 65535.
 *
 * If the actual data length is longer than long_read_len,
 * "ORA-01406: fetched column value was truncated" is raised.
 *
 * Note: long_read_len is also used for XMLTYPE data type in 2.0.
 */
static VALUE oci8_long_read_len(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    return svcctx->long_read_len;
}

/*
 * call-seq:
 *   long_read_len = fixnum
 *
 * Sets the maximum length in bytes to fetch a LONG or LONG RAW
 * column.
 *
 * See also #long_read_len
 *
 */
static VALUE oci8_set_long_read_len(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    Check_Type(val, T_FIXNUM);
    svcctx->long_read_len = val;
    return val;
}

/*
 * call-seq:
 *   break
 *
 * Cancels the executing SQL.
 *
 * See also #non_blocking=.
 */
static VALUE oci8_break(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);

    if (NIL_P(svcctx->executing_thread)) {
        return Qfalse;
    }
#ifndef NATIVE_THREAD_WITH_GVL
    chker2(OCIBreak(svcctx->base.hp.ptr, oci8_errhp), &svcctx->base);
#endif
    rb_thread_wakeup(svcctx->executing_thread);
    return Qtrue;
}

/*
 * call-seq:
 *   prefetch_rows = number
 *
 * Sets the prefetch rows size. The default value is one.
 * When a select statement is executed, the OCI library allocate
 * prefetch buffer to reduce the number of network round trips by
 * retrieving specified number of rows in one round trip.
 *
 * Note: Active record adaptors set 100 by default.
 */
static VALUE oci8_set_prefetch_rows(VALUE self, VALUE val)
{
    rb_ivar_set(self, id_at_prefetch_rows, val);
    return val;
}

/*
 * call-seq:
 *   oracle_server_vernum -> an integer
 *
 * <b>(new in 2.0.1)</b>
 *
 * Returns a numerical format of the Oracle server version.
 *
 * See also: #oracle_server_version
 */
static VALUE oci8_oracle_server_vernum(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    char buf[100];
    ub4 version;

    chker2(OCIServerRelease(svcctx->base.hp.ptr, oci8_errhp, (text*)buf, sizeof(buf), (ub1)svcctx->base.type, &version), &svcctx->base);
    return UINT2NUM(version);
}

/*
 * call-seq:
 *   ping -> true or false
 *
 * <b>(new in 2.0.2)</b>
 *
 * Makes a round trip call to the server to confirm that the connection and
 * the server are active.
 *
 * OCI8#ping also can be used to flush all the pending OCI client-side calls
 * to the server if any exist.
 *
 * === Oracle 10.2 client or upper
 * A dummy round trip call is made by a newly added OCI function
 * OCIPing[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14250/oci16msc007.htm#sthref3540] in Oracle 10.2.
 *
 * === Oracle 10.1 client or lower
 * A simple PL/SQL block "BEGIN NULL; END;" is executed to make a round trip call.
 */
static VALUE oci8_ping(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    sword rv;

    if (have_OCIPing_nb) {
        /* Oracle 10.2 or upper */
        rv = OCIPing_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT);
    } else {
        /* Oracle 10.1 or lower */
        rv = oci8_exec_sql(svcctx, "BEGIN NULL; END;", 0U, NULL, 0U, NULL, 0);
    }
    return rv == OCI_SUCCESS ? Qtrue : FALSE;
}

/*
 * call-seq:
 *   client_identifier = string or nil
 *
 * <b>(new in 2.0.3)</b>
 *
 * Sets the client ID. This information is stored in the V$SESSION
 * view.
 *
 * === Oracle 9i client or upper
 *
 * This doesn't perform network round trips. The change is reflected
 * to the server by the next round trip such as OCI8#exec, OCI8#ping,
 * etc.
 *
 * === Oracle 8i client or lower
 *
 * This executes the following PL/SQL block internally.
 * The change is reflected immediately by a network round trip.
 *
 *   BEGIN
 *     DBMS_SESSION.SET_IDENTIFIER(:client_id);
 *   END;
 *
 * See {Oracle Manual: Oracle Database PL/SQL Packages and Types Reference}[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14258/d_sessio.htm#i996935]
 *
 */
static VALUE oci8_set_client_identifier(VALUE self, VALUE val)
{
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }

    if (oracle_client_version >= ORAVERNUM(9, 2, 0, 3, 0) || size > 0) {
        if (size > 0 && ptr[0] == ':') {
            rb_raise(rb_eArgError, "client identifier should not start with ':'.");
        }
        chker2(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, (dvoid*)ptr,
                          size, OCI_ATTR_CLIENT_IDENTIFIER, oci8_errhp),
               DATA_PTR(self));
    } else {
        /* Workaround for Bug 2449486 */
        oci8_exec_sql_var_t bind_vars[1];

        /* :client_id */
        bind_vars[0].valuep = (dvoid*)ptr;
        bind_vars[0].value_sz = size;
        bind_vars[0].dty = SQLT_CHR;
        bind_vars[0].indp = NULL;
        bind_vars[0].alenp = NULL;

        oci8_exec_sql(oci8_get_svcctx(self),
                      "BEGIN\n"
                      "  DBMS_SESSION.SET_IDENTIFIER(:client_id);\n"
                      "END;\n", 0, NULL, 1, bind_vars, 1);
    }
    return val;
}

/*
 * call-seq:
 *   module = string or nil
 *
 * <b>(new in 2.0.3)</b>
 *
 * Sets the name of the current module. This information is
 * stored in the V$SESSION view and is also stored in the V$SQL view
 * and the V$SQLAREA view when a SQL statement is executed and the SQL
 * statement is first parsed in the Oracle server.
 *
 * === Oracle 10g client or upper
 *
 * This doesn't perform network round trips. The change is reflected
 * to the server by the next round trip such as OCI8#exec, OCI8#ping,
 * etc.
 *
 * === Oracle 9i client or lower
 *
 * This executes the following PL/SQL block internally.
 * The change is reflected immediately by a network round trip.
 *
 *   DECLARE
 *     action VARCHAR2(32);
 *   BEGIN
 *     -- retrieve action name.
 *     SELECT SYS_CONTEXT('USERENV','ACTION') INTO action FROM DUAL;
 *     -- change module name without modifying the action name.
 *     DBMS_APPLICATION_INFO.SET_MODULE(:module, action);
 *   END;
 *
 * See {Oracle Manual: Oracle Database PL/SQL Packages and Types Reference}[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14258/d_appinf.htm#i999254]
 *
 */
static VALUE oci8_set_module(VALUE self, VALUE val)
{
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }
    if (oracle_client_version >= ORAVER_10_1) {
        /* Oracle 10g or upper */
        chker2(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, (dvoid*)ptr,
                          size, OCI_ATTR_MODULE, oci8_errhp),
               DATA_PTR(self));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :module */
        bind_vars[0].valuep = (dvoid*)ptr;
        bind_vars[0].value_sz = size;
        bind_vars[0].dty = SQLT_CHR;
        bind_vars[0].indp = NULL;
        bind_vars[0].alenp = NULL;

        oci8_exec_sql(oci8_get_svcctx(self),
                      "DECLARE\n"
                      "  action VARCHAR2(32);\n"
                      "BEGIN\n"
                      "  SELECT SYS_CONTEXT('USERENV','ACTION') INTO action FROM DUAL;\n"
                      "  DBMS_APPLICATION_INFO.SET_MODULE(:module, action);\n"
                      "END;\n", 0, NULL, 1, bind_vars, 1);
    }
    return self;
}

/*
 * call-seq:
 *   action = string or nil
 *
 * <b>(new in 2.0.3)</b>
 *
 * Sets the name of the current action within the current module.
 * This information is stored in the V$SESSION view and is also
 * stored in the V$SQL view and the V$SQLAREA view when a SQL
 * statement is executed and the SQL statement is first parsed
 * in the Oracle server.
 *
 * === Oracle 10g client or upper
 *
 * This doesn't perform network round trips. The change is reflected
 * to the server by the next round trip such as OCI8#exec, OCI8#ping,
 * etc.
 *
 * === Oracle 9i client or lower
 *
 * This executes the following PL/SQL block internally.
 * The change is reflected immediately by a network round trip.
 *
 *   BEGIN
 *     DBMS_APPLICATION_INFO.SET_ACTION(:action);
 *   END;
 *
 * See {Oracle Manual: Oracle Database PL/SQL Packages and Types Reference}[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14258/d_appinf.htm#i999254]
 *
 */
static VALUE oci8_set_action(VALUE self, VALUE val)
{
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }
    if (oracle_client_version >= ORAVER_10_1) {
        /* Oracle 10g or upper */
        chker2(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, (dvoid*)ptr,
                          size, OCI_ATTR_ACTION, oci8_errhp),
               DATA_PTR(self));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :action */
        bind_vars[0].valuep = (dvoid*)ptr;
        bind_vars[0].value_sz = size;
        bind_vars[0].dty = SQLT_CHR;
        bind_vars[0].indp = NULL;
        bind_vars[0].alenp = NULL;

        oci8_exec_sql(oci8_get_svcctx(self),
                      "BEGIN\n"
                      "  DBMS_APPLICATION_INFO.SET_ACTION(:action);\n"
                      "END;\n", 0, NULL, 1, bind_vars, 1);
    }
    return val;
}

/*
 * call-seq:
 *   client_info = string or nil
 *
 * <b>(new in 2.0.3)</b>
 *
 * Sets additional information about the client application.
 * This information is stored in the V$SESSION view.
 *
 * === Oracle 10g client or upper
 *
 * This doesn't perform network round trips. The change is reflected
 * to the server by the next round trip such as OCI8#exec, OCI8#ping,
 * etc.
 *
 * === Oracle 9i client or lower
 *
 * This executes the following PL/SQL block internally.
 * The change is reflected immediately by a network round trip.
 *
 *   BEGIN
 *     DBMS_APPLICATION_INFO.SET_CLIENT_INFO(:client_info);
 *   END;
 *
 * See {Oracle Manual: Oracle Database PL/SQL Packages and Types Reference}[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14258/d_appinf.htm#CHEJCFGG]
 */
static VALUE oci8_set_client_info(VALUE self, VALUE val)
{
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }
    if (oracle_client_version >= ORAVER_10_1) {
        /* Oracle 10g or upper */
        chker2(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, (dvoid*)ptr,
                          size, OCI_ATTR_CLIENT_INFO, oci8_errhp),
               DATA_PTR(self));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :client_info */
        bind_vars[0].valuep = (dvoid*)ptr;
        bind_vars[0].value_sz = size;
        bind_vars[0].dty = SQLT_CHR;
        bind_vars[0].indp = NULL;
        bind_vars[0].alenp = NULL;

        oci8_exec_sql(oci8_get_svcctx(self), 
                      "BEGIN\n"
                      "  DBMS_APPLICATION_INFO.SET_CLIENT_INFO(:client_info);\n"
                      "END;\n", 0, NULL, 1, bind_vars, 1);
    }
    return val;
}

VALUE Init_oci8(void)
{
    VALUE obj;
    oci8_base_t *base;
#if 0
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_vtable);
    cSession = oci8_define_class_under(cOCI8, "Session", &oci8_svcctx_associate_vtable);
    cServer = oci8_define_class_under(cOCI8, "Server", &oci8_svcctx_associate_vtable);
    id_at_session_handle = rb_intern("@session_handle");
    id_at_server_handle = rb_intern("@server_handle");

    /* setup a dummy environment handle to lazily initialize the environment handle */
    obj = rb_obj_alloc(rb_cObject);
    rb_define_singleton_method(obj, "method_missing", dummy_env_method_missing, -1);
    rb_cv_set(cOCI8, "@@environment_handle", obj);

    /* setup the process handle */
    obj = rb_obj_alloc(oci8_cOCIHandle);
    base = DATA_PTR(obj);
    base->type = OCI_HTYPE_PROC;
    base->self = Qnil;
    rb_cv_set(cOCI8, "@@process_handle", obj);

    oracle_client_vernum = INT2FIX(oracle_client_version);
    if (have_OCIClientVersion) {
        sword major, minor, update, patch, port_update;
        OCIClientVersion(&major, &minor, &update, &patch, &port_update);
        oracle_client_vernum = INT2FIX(ORAVERNUM(major, minor, update, patch, port_update));
    }

    id_at_prefetch_rows = rb_intern("@prefetch_rows");
    id_set_prefetch_rows = rb_intern("prefetch_rows=");

    rb_define_const(cOCI8, "VERSION", rb_obj_freeze(rb_usascii_str_new_cstr(OCI8LIB_VERSION)));
    rb_define_singleton_method_nodoc(cOCI8, "oracle_client_vernum", oci8_s_oracle_client_vernum, 0);
    rb_define_singleton_method_nodoc(cOCI8, "__set_property", oci8_s_set_property, 2);
    rb_define_singleton_method(cOCI8, "error_message", oci8_s_error_message, 1);
    rb_define_private_method(cOCI8, "parse_connect_string", oci8_parse_connect_string, 1);
    rb_define_private_method(cOCI8, "logon2", oci8_logon2, 4);
    rb_define_private_method(cOCI8, "allocate_handles", oci8_allocate_handles, 0);
    rb_define_private_method(cOCI8, "server_attach", oci8_server_attach, 2);
    rb_define_private_method(cOCI8, "session_begin", oci8_session_begin, 2);
    rb_define_method(cOCI8, "logoff", oci8_svcctx_logoff, 0);
    rb_define_method(cOCI8, "commit", oci8_commit, 0);
    rb_define_method(cOCI8, "rollback", oci8_rollback, 0);
    rb_define_method(cOCI8, "non_blocking?", oci8_non_blocking_p, 0);
    rb_define_method(cOCI8, "non_blocking=", oci8_set_non_blocking, 1);
    rb_define_method(cOCI8, "autocommit?", oci8_autocommit_p, 0);
    rb_define_method(cOCI8, "autocommit=", oci8_set_autocommit, 1);
    rb_define_method(cOCI8, "long_read_len", oci8_long_read_len, 0);
    rb_define_method(cOCI8, "long_read_len=", oci8_set_long_read_len, 1);
    rb_define_method(cOCI8, "break", oci8_break, 0);
    rb_define_method(cOCI8, "prefetch_rows=", oci8_set_prefetch_rows, 1);
    rb_define_private_method(cOCI8, "oracle_server_vernum", oci8_oracle_server_vernum, 0);
    rb_define_method(cOCI8, "ping", oci8_ping, 0);
    rb_define_method(cOCI8, "client_identifier=", oci8_set_client_identifier, 1);
    rb_define_method(cOCI8, "module=", oci8_set_module, 1);
    rb_define_method(cOCI8, "action=", oci8_set_action, 1);
    rb_define_method(cOCI8, "client_info=", oci8_set_client_info, 1);
    rb_define_attr(cOCI8, "last_error", 1, 1);
    return cOCI8;
}

oci8_svcctx_t *oci8_get_svcctx(VALUE obj)
{
    return (oci8_svcctx_t *)oci8_get_handle(obj, cOCI8);
}

OCISvcCtx *oci8_get_oci_svcctx(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);
    return svcctx->base.hp.svc;
}

OCISession *oci8_get_oci_session(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);
    return svcctx->usrhp;
}

void oci8_check_pid_consistency(oci8_svcctx_t *svcctx)
{
    if (svcctx->pid != getpid()) {
        rb_raise(rb_eRuntimeError, "The connection cannot be reused in the forked process.");
    }
}

