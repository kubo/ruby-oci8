/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * connection_pool.c - part of ruby-oci8
 *
 * Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

static VALUE cOCIConnectionPool;

#define TO_CPOOL(obj) ((oci8_cpool_t *)oci8_get_handle((obj), cOCIConnectionPool))

typedef struct {
    oci8_base_t base;
    VALUE pool_name;
} oci8_cpool_t;

static void oci8_cpool_mark(oci8_base_t *base)
{
    oci8_cpool_t *cpool = (oci8_cpool_t *)base;

    rb_gc_mark(cpool->pool_name);
}

static void *cpool_free_thread(void *arg)
{
    OCIConnectionPoolDestroy((OCICPool *)arg, oci8_errhp, OCI_DEFAULT);
    OCIHandleFree(arg, OCI_HTYPE_CPOOL);
    return NULL;
}

static void oci8_cpool_free(oci8_base_t *base)
{
    oci8_run_native_thread(cpool_free_thread, base->hp.poolhp);
    base->type = 0;
    base->hp.ptr = NULL;
}

static void oci8_cpool_init(oci8_base_t *base)
{
    oci8_cpool_t *cpool = (oci8_cpool_t *)base;

    cpool->pool_name = Qnil;
}

static oci8_base_vtable_t oci8_cpool_vtable = {
    oci8_cpool_mark,
    oci8_cpool_free,
    sizeof(oci8_cpool_t),
    oci8_cpool_init,
};

/*
 * call-seq:
 *   OCI8::ConnectionPool.new(conn_min, conn_max, conn_incr, username = nil, password = nil, dbname = nil) -> connection pool
 *   OCI8::ConnectionPool.new(conn_min, conn_max, conn_incr, connect_string) -> connection pool
 *
 * Creates a connection pool.
 *
 * <i>conn_min</i> specifies the minimum number of connections in the
 * connection pool. Valid values are 0 and higher.
 *
 * <i>conn_max</i> specifies the maximum number of connections that
 * can be opened to the database. Once this value is reached, no more
 * connections are opened. Valid values are 1 and higher.
 * Note that this limits the number of concurent SQL executions, not
 * the number of concurrent sessions.
 *
 * <i>conn_incr</i> allows the application to set the next increment
 * for connections to be opened to the database if the current number
 * of connections are less than <i>conn_max</i>. Valid values are 0
 * and higher.
 *
 * <i>username</i> and <i>password</i> are required to establish an
 * implicit primary session. When both are nil, external
 * authentication is used.
 *
 * <i>dbname</i> specifies the database server to connect to.
 *
 * If the number of arguments is four, <i>username</i>,
 * <i>password</i> and <i>dbname</i> are extracted from the fourth
 * argument <i>connect_string</i>. The syntax is "username/password" or
 * "username/password@dbname".
 */
static VALUE oci8_cpool_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE conn_min;
    VALUE conn_max;
    VALUE conn_incr;
    VALUE username;
    VALUE password;
    VALUE dbname;
    oci8_cpool_t *cpool = TO_CPOOL(self);
    OraText *pool_name = NULL;
    sb4 pool_name_len = 0;
    sword rv;

    /* check arguments */
    rb_scan_args(argc, argv, "42", &conn_min, &conn_max, &conn_incr,
                 &username, &password, &dbname);
    Check_Type(conn_min, T_FIXNUM);
    Check_Type(conn_max, T_FIXNUM);
    Check_Type(conn_incr, T_FIXNUM);
    if (argc == 4) {
        VALUE mode;
        VALUE conn_str = username;

        OCI8SafeStringValue(conn_str);
        oci8_do_parse_connect_string(conn_str, &username, &password, &dbname, &mode);
        if (!NIL_P(mode)) {
            rb_raise(rb_eArgError, "invalid connect string \"%s\": Connection pooling doesn't support sysdba and sysoper privileges.", RSTRING_PTR(conn_str));
        }
    } else {
        if (!NIL_P(username)) {
            OCI8SafeStringValue(username);
        }
        if (!NIL_P(password)) {
            OCI8SafeStringValue(password);
        }
        if (!NIL_P(dbname)) {
            OCI8SafeStringValue(dbname);
        }
    }

    rv = OCIHandleAlloc(oci8_envhp, &cpool->base.hp.ptr, OCI_HTYPE_CPOOL, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    cpool->base.type = OCI_HTYPE_CPOOL;

    chker2(OCIConnectionPoolCreate(oci8_envhp, oci8_errhp, cpool->base.hp.poolhp,
                                   &pool_name, &pool_name_len,
                                   NIL_P(dbname) ? NULL : RSTRING_ORATEXT(dbname),
                                   NIL_P(dbname) ? 0 : RSTRING_LEN(dbname),
                                   FIX2UINT(conn_min), FIX2UINT(conn_max),
                                   FIX2UINT(conn_incr),
                                   NIL_P(username) ? NULL : RSTRING_ORATEXT(username),
                                   NIL_P(username) ? 0 : RSTRING_LEN(username),
                                   NIL_P(password) ? NULL : RSTRING_ORATEXT(password),
                                   NIL_P(password) ? 0 : RSTRING_LEN(password),
                                   OCI_DEFAULT),
           &cpool->base);
    cpool->pool_name = rb_str_new(TO_CHARPTR(pool_name), pool_name_len);
    rb_str_freeze(cpool->pool_name);
    return Qnil;
}

/*
 * call-seq:
 *   reinitialize(min, max, incr)
 *
 * Changes the the number of minimum connections, the number of
 * maximum connections and the connection increment parameter.
 */
static VALUE oci8_cpool_reinitialize(VALUE self, VALUE conn_min, VALUE conn_max, VALUE conn_incr)
{
    oci8_cpool_t *cpool = TO_CPOOL(self);
    OraText *pool_name;
    sb4 pool_name_len;

    /* check arguments */
    Check_Type(conn_min, T_FIXNUM);
    Check_Type(conn_max, T_FIXNUM);
    Check_Type(conn_incr, T_FIXNUM);

    chker2(OCIConnectionPoolCreate(oci8_envhp, oci8_errhp, cpool->base.hp.poolhp,
                                   &pool_name, &pool_name_len, NULL, 0, 
                                   FIX2UINT(conn_min), FIX2UINT(conn_max),
                                   FIX2UINT(conn_incr),
                                   NULL, 0, NULL, 0, OCI_CPOOL_REINITIALIZE),
           &cpool->base);
    return self;
}

/*
 * call-seq:
 *   pool_name -> string
 *
 * Retruns the pool name.
 *
 * @private
 */
static VALUE oci8_cpool_pool_name(VALUE self)
{
    oci8_cpool_t *cpool = TO_CPOOL(self);

    return cpool->pool_name;
}

void Init_oci8_connection_pool(VALUE cOCI8)
{
#if 0
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
    cOCIConnectionPool = rb_define_class_under(cOCI8, "ConnectionPool", cOCIHandle);
#endif

    cOCIConnectionPool = oci8_define_class_under(cOCI8, "ConnectionPool", &oci8_cpool_vtable);

    rb_define_private_method(cOCIConnectionPool, "initialize", oci8_cpool_initialize, -1);
    rb_define_method(cOCIConnectionPool, "reinitialize", oci8_cpool_reinitialize, 3);
    rb_define_private_method(cOCIConnectionPool, "pool_name", oci8_cpool_pool_name, 0);
}
