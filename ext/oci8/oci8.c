/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2010 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
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

static VALUE cOCI8;
static VALUE cSession;
static VALUE cServer;

typedef struct oci8_svcctx_associate {
    oci8_base_t base;
    oci8_svcctx_t *svcctx;
} oci8_svcctx_associate_t;

static void oci8_svcctx_associate_free(oci8_base_t *base)
{
    oci8_svcctx_associate_t *assoc = (oci8_svcctx_associate_t *)base;
    if (assoc->svcctx != NULL) {
        switch (assoc->base.type) {
        case OCI_HTYPE_SESSION:
            assoc->svcctx->session = NULL;
            break;
        case OCI_HTYPE_SERVER:
            assoc->svcctx->server = NULL;
            break;
        }
    }
}

static oci8_base_class_t oci8_svcctx_associate_class = {
    NULL,
    oci8_svcctx_associate_free,
    sizeof(oci8_svcctx_associate_t),
};

static void oci8_svcctx_mark(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;

    if (svcctx->session) {
        rb_gc_mark(svcctx->session->self);
    }
    if (svcctx->server) {
        rb_gc_mark(svcctx->server->self);
    }
}

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;

    if (svcctx->session) {
        oci8_base_free(svcctx->session);
        svcctx->session = NULL;
    }
    if (svcctx->server) {
        oci8_base_free(svcctx->server);
        svcctx->server = NULL;
    }
}

static void oci8_svcctx_init(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;

    svcctx->executing_thread = Qnil;
    svcctx->session = DATA_PTR(rb_obj_alloc(cSession));
    svcctx->server = DATA_PTR(rb_obj_alloc(cServer));
    ((oci8_svcctx_associate_t *)svcctx->session)->svcctx = svcctx;
    ((oci8_svcctx_associate_t *)svcctx->server)->svcctx = svcctx;
    svcctx->pid = getpid();
    svcctx->is_autocommit = 0;
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
    svcctx->non_blocking = 1;
#endif
    svcctx->long_read_len = INT2FIX(65535);
}

static oci8_base_class_t oci8_svcctx_class = {
    oci8_svcctx_mark,
    oci8_svcctx_free,
    sizeof(oci8_svcctx_t),
    oci8_svcctx_init,
};

static VALUE oracle_client_vernum; /* Oracle client version number */
static VALUE sym_SYSDBA;
static VALUE sym_SYSOPER;
static ID id_at_prefetch_rows;
static ID id_set_prefetch_rows;

static VALUE oci8_s_oracle_client_vernum(VALUE klass)
{
    return oracle_client_vernum;
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
            char *ptr;
            SafeStringValue(*mode);
            ptr = RSTRING_PTR(*mode);
            if (strcasecmp(ptr, "SYSDBA") == 0) {
                *mode = sym_SYSDBA;
            } else if (strcasecmp(ptr, "SYSOPER") == 0) {
                *mode = sym_SYSOPER;
            }
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

static void call_oci_logoff(oci8_svcctx_t *svcctx)
{
    svcctx->logoff_method = NULL;
    svcctx->session->type = 0;
    svcctx->server->type = 0;
    oci_lc(OCILogoff_nb(svcctx, svcctx->base.hp.svc, oci8_errhp));
    svcctx->base.type = 0;
}

static void call_session_end(oci8_svcctx_t *svcctx)
{
    sword rv = OCI_SUCCESS;

    if (svcctx->state & OCI8_STATE_SESSION_BEGIN_WAS_CALLED) {
        rv = OCISessionEnd_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, svcctx->session->hp.authhp, OCI_DEFAULT);
        svcctx->state &= ~OCI8_STATE_SESSION_BEGIN_WAS_CALLED;
    }
    if (svcctx->state & OCI8_STATE_SERVER_ATTACH_WAS_CALLED) {
        rv = OCIServerDetach_nb(svcctx, svcctx->server->hp.srvhp, oci8_errhp, OCI_DEFAULT);
        svcctx->state &= ~OCI8_STATE_SERVER_ATTACH_WAS_CALLED;
    }
    svcctx->logoff_method = NULL;
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
}

/*
 * call-seq:
 *   logon(username, password, dbname) -> connection
 *
 * <b>internal use only</b>
 *
 * Creates a simple logon session by the OCI function OCILogon().
 */
static VALUE oci8_logon(VALUE self, VALUE username, VALUE password, VALUE dbname)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);

    if (svcctx->logoff_method != NULL) {
        rb_raise(rb_eRuntimeError, "Could not reuse the session.");
    }

    /* check arugmnets */
    OCI8SafeStringValue(username);
    OCI8SafeStringValue(password);
    if (!NIL_P(dbname)) {
        OCI8SafeStringValue(dbname);
    }

    /* logon */
    oci_lc(OCILogon_nb(svcctx, oci8_envhp, oci8_errhp, &svcctx->base.hp.svc,
                       RSTRING_ORATEXT(username), RSTRING_LEN(username),
                       RSTRING_ORATEXT(password), RSTRING_LEN(password),
                       NIL_P(dbname) ? NULL : RSTRING_ORATEXT(dbname),
                       NIL_P(dbname) ? 0 : RSTRING_LEN(dbname)));
    svcctx->base.type = OCI_HTYPE_SVCCTX;
    svcctx->logoff_method = call_oci_logoff;
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

    if (svcctx->logoff_method != NULL) {
        rb_raise(rb_eRuntimeError, "Could not reuse the session.");
    }
    svcctx->logoff_method = call_session_end;
    svcctx->state = 0;

    /* allocate a service context handle */
    rv = OCIHandleAlloc(oci8_envhp, &svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    svcctx->base.type = OCI_HTYPE_SVCCTX;

    /* alocalte a session handle */
    rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->session->hp.ptr, OCI_HTYPE_SESSION, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    svcctx->session->type = OCI_HTYPE_SESSION;

    /* alocalte a server handle */
    rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->server->hp.ptr, OCI_HTYPE_SERVER, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    svcctx->server->type = OCI_HTYPE_SERVER;
    return self;
}

/*
 * call-seq:
 *   session_handle -> a session handle
 *
 * <b>internal use only</b>
 *
 * Returns a session handle associated with the service context handle.
 */
static VALUE oci8_get_session_handle(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    return svcctx->session->self;
}

/*
 * call-seq:
 *   server_handle -> a server handle
 *
 * <b>internal use only</b>
 *
 * Returns a server handle associated with the service context handle.
 */
static VALUE oci8_get_server_handle(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    return svcctx->server->self;
}

/*
 * call-seq:
 *   server_attach(dbname, mode)
 *
 * <b>internal use only</b>
 *
 * Attachs to the server by the OCI function OCIServerAttach().
 */
static VALUE oci8_server_attach(VALUE self, VALUE dbname, VALUE mode)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);

    if (svcctx->logoff_method != call_session_end) {
        rb_raise(rb_eRuntimeError, "Use this method only for the service context handle created by OCI8#server_handle().");
    }
    if (svcctx->state & OCI8_STATE_SERVER_ATTACH_WAS_CALLED) {
        rb_raise(rb_eRuntimeError, "Could not use this method twice.");
    }

    /* check arguments */
    if (!NIL_P(dbname)) {
        OCI8SafeStringValue(dbname);
    }
    Check_Type(mode, T_FIXNUM);

    /* attach to the server */
    oci_lc(OCIServerAttach_nb(svcctx, svcctx->server->hp.srvhp, oci8_errhp,
                              NIL_P(dbname) ? NULL : RSTRING_ORATEXT(dbname),
                              NIL_P(dbname) ? 0 : RSTRING_LEN(dbname),
                              FIX2UINT(mode)));
    oci_lc(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX,
                      svcctx->server->hp.srvhp, 0, OCI_ATTR_SERVER,
                      oci8_errhp));
    svcctx->state |= OCI8_STATE_SERVER_ATTACH_WAS_CALLED;
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

    if (svcctx->logoff_method != call_session_end) {
        rb_raise(rb_eRuntimeError, "Use this method only for the service context handle created by OCI8#server_handle().");
    }
    if (svcctx->state & OCI8_STATE_SESSION_BEGIN_WAS_CALLED) {
        rb_raise(rb_eRuntimeError, "Could not use this method twice.");
    }

    /* check arguments */
    Check_Type(cred, T_FIXNUM);
    Check_Type(mode, T_FIXNUM);

    /* begin session */
    oci_lc(OCISessionBegin_nb(svcctx, svcctx->base.hp.ptr, oci8_errhp,
                              svcctx->session->hp.authhp, FIX2UINT(cred),
                              FIX2UINT(mode)));
    oci_lc(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX,
                      svcctx->session->hp.authhp, 0, OCI_ATTR_SESSION,
                      oci8_errhp));
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
    if (svcctx->logoff_method != NULL) {
        oci_lc(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT));
        svcctx->logoff_method(svcctx);
    }
    return Qtrue;
}

/*
 * call-seq:
 *   parse(sql_text) -> an instance of OCI8::Cursor
 *
 * Prepares the SQL statement and returns a new OCI8::Cursor.
 */
static VALUE oci8_svcctx_parse(VALUE self, VALUE sql)
{
    VALUE obj = rb_funcall(cOCIStmt, oci8_id_new, 2, self, sql);
    VALUE prefetch_rows = rb_ivar_get(self, id_at_prefetch_rows);
    if (!NIL_P(prefetch_rows)) {
        rb_funcall(obj, id_set_prefetch_rows, 1, prefetch_rows);
    }
    return obj;
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
    oci_lc(OCITransCommit_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT));
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
    oci_lc(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT));
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
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
    return svcctx->non_blocking ? Qtrue : Qfalse;
#else
    sb1 non_blocking;

    if (svcctx->server->hp.srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->server->hp.srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->server->hp.srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
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
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
    svcctx->non_blocking = RTEST(val);
#else
    sb1 non_blocking;

    if (svcctx->server->hp.srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->server->hp.srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->server->hp.srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
    if ((RTEST(val) && !non_blocking) || (!RTEST(val) && non_blocking)) {
        /* toggle blocking / non-blocking. */
        oci_lc(OCIAttrSet(svcctx->server->hp.srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
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
#ifndef HAVE_RB_THREAD_BLOCKING_REGION
    sword rv;
#endif

    if (NIL_P(svcctx->executing_thread)) {
        return Qfalse;
    }
#ifndef HAVE_RB_THREAD_BLOCKING_REGION
    rv = OCIBreak(svcctx->base.hp.ptr, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
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
    char *p;

    if (have_OCIServerRelease) {
        /* Oracle 9i or later */
        oci_lc(OCIServerRelease(svcctx->base.hp.ptr, oci8_errhp, (text*)buf, sizeof(buf), (ub1)svcctx->base.type, &version));
        return UINT2NUM(version);
    } else {
        /* Oracle 8.x */
        oci_lc(OCIServerVersion(svcctx->base.hp.ptr, oci8_errhp, (text*)buf, sizeof(buf), (ub1)svcctx->base.type));
        if ((p = strchr(buf, '.')) != NULL) {
            unsigned int major, minor, update, patch, port_update;
            while (p >= buf && *p != ' ') {
                p--;
            }
            if (sscanf(p + 1, "%u.%u.%u.%u.%u", &major, &minor, &update, &patch, &port_update) == 5) {
                return INT2FIX(ORAVERNUM(major, minor, update, patch, port_update));
            }
        }
        return Qnil;
    }
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
    char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }

    if (oracle_client_version >= ORAVERNUM(9, 2, 0, 3, 0) || size >= 0) {
        if (size > 0 && ptr[0] == ':') {
            rb_raise(rb_eArgError, "client identifier should not start with ':'.");
        }
        oci_lc(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, ptr,
                          size, OCI_ATTR_CLIENT_IDENTIFIER, oci8_errhp));
    } else {
        /* Workaround for Bug 2449486 */
        oci8_exec_sql_var_t bind_vars[1];

        /* :client_id */
        bind_vars[0].valuep = ptr;
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
    char *ptr;
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
        oci_lc(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, ptr,
                          size, OCI_ATTR_MODULE, oci8_errhp));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :module */
        bind_vars[0].valuep = ptr;
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
    char *ptr;
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
        oci_lc(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, ptr,
                          size, OCI_ATTR_ACTION, oci8_errhp));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :action */
        bind_vars[0].valuep = ptr;
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
    char *ptr;
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
        oci_lc(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, ptr,
                          size, OCI_ATTR_CLIENT_INFO, oci8_errhp));
    } else {
        /* Oracle 9i or lower */
        oci8_exec_sql_var_t bind_vars[1];

        /* :client_info */
        bind_vars[0].valuep = ptr;
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
#if 0
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_class);
    cSession = oci8_define_class_under(cOCI8, "Session", &oci8_svcctx_associate_class);
    cServer = oci8_define_class_under(cOCI8, "Server", &oci8_svcctx_associate_class);

    oracle_client_vernum = INT2FIX(oracle_client_version);
    if (have_OCIClientVersion) {
        sword major, minor, update, patch, port_update;
        OCIClientVersion(&major, &minor, &update, &patch, &port_update);
        oracle_client_vernum = INT2FIX(ORAVERNUM(major, minor, update, patch, port_update));
    }

    sym_SYSDBA = ID2SYM(rb_intern("SYSDBA"));
    sym_SYSOPER = ID2SYM(rb_intern("SYSOPER"));
    id_at_prefetch_rows = rb_intern("@prefetch_rows");
    id_set_prefetch_rows = rb_intern("prefetch_rows=");

    rb_define_const(cOCI8, "VERSION", rb_obj_freeze(rb_usascii_str_new_cstr(OCI8LIB_VERSION)));
    rb_define_singleton_method_nodoc(cOCI8, "oracle_client_vernum", oci8_s_oracle_client_vernum, 0);
    if (have_OCIMessageOpen && have_OCIMessageGet) {
        rb_define_singleton_method(cOCI8, "error_message", oci8_s_error_message, 1);
    }
    rb_define_private_method(cOCI8, "parse_connect_string", oci8_parse_connect_string, 1);
    rb_define_private_method(cOCI8, "logon", oci8_logon, 3);
    rb_define_private_method(cOCI8, "allocate_handles", oci8_allocate_handles, 0);
    rb_define_private_method(cOCI8, "session_handle", oci8_get_session_handle, 0);
    rb_define_private_method(cOCI8, "server_handle", oci8_get_server_handle, 0);
    rb_define_private_method(cOCI8, "server_attach", oci8_server_attach, 2);
    rb_define_private_method(cOCI8, "session_begin", oci8_session_begin, 2);
    rb_define_method(cOCI8, "logoff", oci8_svcctx_logoff, 0);
    rb_define_method(cOCI8, "parse", oci8_svcctx_parse, 1);
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

    if (svcctx->session->hp.authhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->session->hp.authhp, 0, OCI_ATTR_SESSION, oci8_errhp));
    }
    return svcctx->session->hp.authhp;
}

void oci8_check_pid_consistency(oci8_svcctx_t *svcctx)
{
    if (svcctx->pid != getpid()) {
        rb_raise(rb_eRuntimeError, "The connection cannot be reused in the forked process.");
    }
}

