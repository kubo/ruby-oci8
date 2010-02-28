/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2009 KUBO Takehiro <kubo@jiubao.org>
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

static VALUE cOCI8;

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;

    if (svcctx->authhp) {
        OCIHandleFree(svcctx->authhp, OCI_HTYPE_SESSION);
        svcctx->authhp = NULL;
    }
    if (svcctx->srvhp) {
        OCIHandleFree(svcctx->srvhp, OCI_HTYPE_SERVER);
        svcctx->srvhp = NULL;
    }
}

static oci8_base_class_t oci8_svcctx_class = {
    NULL,
    oci8_svcctx_free,
    sizeof(oci8_svcctx_t)
};

static VALUE oracle_client_vernum; /* Oracle client version number */
static VALUE sym_SYSDBA;
static VALUE sym_SYSOPER;
static ID id_at_prefetch_rows;
static ID id_at_username;
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
static void oci8_do_parse_connect_string(VALUE conn_str, VALUE *user, VALUE *pass, VALUE *dbname, VALUE *mode)
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

/*
 * call-seq:
 *   new(username, password, dbname = nil, privilege = nil)
 *
 * Connects to an Oracle database server by +username+ and +password+
 * at +dbname+ as +privilege+.
 *
 * === connecting to the local server
 *
 * Set +username+ and +password+ or pass "username/password" as a
 * single argument.
 *
 *   OCI8.new('scott', 'tiger')
 * or
 *   OCI8.new('scott/tiger')
 *
 * === connecting to a remote server
 *
 * Set +username+, +password+ and +dbname+ or pass
 * "username/password@dbname" as a single argument.
 *
 *   OCI8.new('scott', 'tiger', 'orcl.world')
 * or
 *   OCI8.new('scott/tiger@orcl.world')
 *
 * The +dbname+ is a net service name or an easy connectection
 * identifier. The former is a name listed in the file tnsnames.ora.
 * Ask to your DBA if you don't know what it is. The latter has the
 * syntax as "//host:port/service_name".
 *
 *   OCI8.new('scott', 'tiger', '//remote-host:1521/XE')
 * or
 *   OCI8.new('scott/tiger@//remote-host:1521/XE')
 *
 * === connecting as a privileged user
 *
 * Set :SYSDBA or :SYSOPER to +privilege+, otherwise
 * "username/password as sysdba" or "username/password as sysoper"
 * as a single argument.
 *
 *   OCI8.new('sys', 'change_on_install', nil, :SYSDBA)
 * or
 *   OCI8.new('sys/change_on_install as sysdba')
 *
 * === external OS authentication
 *
 * Set nil to +username+ and +password+, or "/" as a single argument.
 *
 *   OCI8.new(nil, nil)
 * or
 *   OCI8.new('/')
 *
 * To connect to a remote host:
 *
 *   OCI8.new(nil, nil, 'dbname')
 * or
 *   OCI8.new('/@dbname')
 *
 * === proxy authentication
 *
 * Enclose end user's username with square brackets and add it at the
 * end of proxy user's username.
 *
 *   OCI8.new('proxy_user_name[end_user_name]', 'proxy_password')
 * or
 *   OCI8.new('proxy_user_name[end_user_name]/proxy_password')
 *
 */
static VALUE oci8_svcctx_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE vusername;
    VALUE vpassword;
    VALUE vdbname;
    VALUE vmode;
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    sword rv;
    enum logon_type_t logon_type = T_IMPLICIT;
    ub4 cred = OCI_CRED_RDBMS;
    ub4 mode = OCI_DEFAULT;
    OCISvcCtx *svchp = NULL;

    svcctx->executing_thread = Qnil;
    if (argc == 1) {
        oci8_do_parse_connect_string(argv[0], &vusername, &vpassword, &vdbname, &vmode);
    } else {
        rb_scan_args(argc, argv, "22", &vusername, &vpassword, &vdbname, &vmode);
    }

    rb_ivar_set(self, id_at_prefetch_rows, Qnil);
    rb_ivar_set(self, id_at_username, Qnil);
    if (NIL_P(vusername) && NIL_P(vpassword)) {
        /* external credential */
        logon_type = T_EXPLICIT;
        cred = OCI_CRED_EXT;
    } else {
        /* RDBMS credential */
        OCI8SafeStringValue(vusername); /* 1 */
        OCI8SafeStringValue(vpassword); /* 2 */
    }
    if (!NIL_P(vdbname)) {
        OCI8SafeStringValue(vdbname); /* 3 */
    }
    if (!NIL_P(vmode)) { /* 4 */
        logon_type = T_EXPLICIT;
        Check_Type(vmode, T_SYMBOL);
        if (vmode == sym_SYSDBA) {
            mode = OCI_SYSDBA;
        } else if (vmode == sym_SYSOPER) {
            mode = OCI_SYSOPER;
        } else {
            rb_raise(rb_eArgError, "invalid privilege name %s (expect :SYSDBA or :SYSOPER)", rb_id2name(SYM2ID(vmode)));
        }
    }
    switch (logon_type) {
    case T_IMPLICIT:
        rv = OCILogon_nb(svcctx, oci8_envhp, oci8_errhp, &svchp,
                         RSTRING_ORATEXT(vusername), RSTRING_LEN(vusername),
                         RSTRING_ORATEXT(vpassword), RSTRING_LEN(vpassword),
                         NIL_P(vdbname) ? NULL : RSTRING_ORATEXT(vdbname),
                         NIL_P(vdbname) ? 0 : RSTRING_LEN(vdbname));
        svcctx->base.hp.svc = svchp;
        svcctx->base.type = OCI_HTYPE_SVCCTX;
        svcctx->logon_type = T_IMPLICIT;
        if (rv != OCI_SUCCESS) {
            oci8_raise(oci8_errhp, rv, NULL);
        }
        break;
    case T_EXPLICIT:
        /* allocate OCI handles. */
        rv = OCIHandleAlloc(oci8_envhp, &svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);
        svcctx->base.type = OCI_HTYPE_SVCCTX;
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->authhp, OCI_HTYPE_SESSION, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0);
        if (rv != OCI_SUCCESS)
            oci8_env_raise(oci8_envhp, rv);

        /* set username and password to OCISession. */
        if (cred == OCI_CRED_RDBMS) {
            oci_lc(OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                              RSTRING_PTR(vusername), RSTRING_LEN(vusername),
                              OCI_ATTR_USERNAME, oci8_errhp));
            oci_lc(OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                              RSTRING_PTR(vpassword), RSTRING_LEN(vpassword),
                              OCI_ATTR_PASSWORD, oci8_errhp));
        }

        /* attach to server and set to OCISvcCtx. */
        rv = OCIServerAttach_nb(svcctx, svcctx->srvhp, oci8_errhp,
                                NIL_P(vdbname) ? NULL : RSTRING_ORATEXT(vdbname),
                                NIL_P(vdbname) ? 0 : RSTRING_LEN(vdbname), OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        oci_lc(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));

        /* begin session. */
        rv = OCISessionBegin_nb(svcctx, svcctx->base.hp.ptr, oci8_errhp, svcctx->authhp, cred, mode);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        oci_lc(OCIAttrSet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp));
        svcctx->logon_type = T_EXPLICIT;
        break;
    default:
        break;
    }
    svcctx->pid = getpid();
    svcctx->is_autocommit = 0;
#ifdef RUBY_VM
    svcctx->non_blocking = 1;
#endif
    svcctx->long_read_len = INT2FIX(65535);
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
    sword rv;

    while (svcctx->base.children != NULL) {
        oci8_base_free(svcctx->base.children);
    }
    switch (svcctx->logon_type) {
    case T_IMPLICIT:
        oci_lc(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT));
        rv = OCILogoff_nb(svcctx, svcctx->base.hp.svc, oci8_errhp);
        svcctx->base.type = 0;
        svcctx->logon_type = T_NOT_LOGIN;
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->authhp = NULL;
        svcctx->srvhp = NULL;
        break;
    case T_EXPLICIT:
        oci_lc(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT));
        rv = OCISessionEnd_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, svcctx->authhp, OCI_DEFAULT);
        if (rv == OCI_SUCCESS) {
            rv = OCIServerDetach_nb(svcctx, svcctx->srvhp, oci8_errhp, OCI_DEFAULT);
        }
        svcctx->logon_type = T_NOT_LOGIN;
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        break;
    case T_NOT_LOGIN:
        break;
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
#ifdef RUBY_VM
    return svcctx->non_blocking ? Qtrue : Qfalse;
#else
    sb1 non_blocking;

    if (svcctx->srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
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
#ifdef RUBY_VM
    svcctx->non_blocking = RTEST(val);
#else
    sb1 non_blocking;

    if (svcctx->srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
    if ((RTEST(val) && !non_blocking) || (!RTEST(val) && non_blocking)) {
        /* toggle blocking / non-blocking. */
        oci_lc(OCIAttrSet(svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
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
#ifndef RUBY_VM
    sword rv;
#endif

    if (NIL_P(svcctx->executing_thread)) {
        return Qfalse;
    }
#ifndef RUBY_VM
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
    int use_attr_set = 1;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LEN(val);
    } else {
        ptr = "";
        size = 0;
    }

    if (oracle_client_version < ORAVER_9_0) {
        use_attr_set = 0;
    } else if (oracle_client_version < ORAVERNUM(9, 2, 0, 3, 0) && size == 0) {
        /* Workaround for Bug 2449486 */
        use_attr_set = 0;
    }

    if (use_attr_set) {
        if (size > 0 && ptr[0] == ':') {
            rb_raise(rb_eArgError, "client identifier should not start with ':'.");
        }
        oci_lc(OCIAttrSet(oci8_get_oci_session(self), OCI_HTYPE_SESSION, ptr,
                          size, OCI_ATTR_CLIENT_IDENTIFIER, oci8_errhp));
    } else {
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
    /*
     * OCIHandle is the abstract base class for all OCI handles and
     * descriptors which are opaque data types of Oracle Call Interface.
     */
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_class);

    oracle_client_vernum = INT2FIX(oracle_client_version);
    if (have_OCIClientVersion) {
        sword major, minor, update, patch, port_update;
        OCIClientVersion(&major, &minor, &update, &patch, &port_update);
        oracle_client_vernum = INT2FIX(ORAVERNUM(major, minor, update, patch, port_update));
    }

    sym_SYSDBA = ID2SYM(rb_intern("SYSDBA"));
    sym_SYSOPER = ID2SYM(rb_intern("SYSOPER"));
    id_at_prefetch_rows = rb_intern("@prefetch_rows");
    id_at_username = rb_intern("@username");
    id_set_prefetch_rows = rb_intern("prefetch_rows=");

    rb_define_singleton_method_nodoc(cOCI8, "oracle_client_vernum", oci8_s_oracle_client_vernum, 0);
    if (have_OCIMessageOpen && have_OCIMessageGet) {
        rb_define_singleton_method(cOCI8, "error_message", oci8_s_error_message, 1);
    }
    rb_define_private_method(cOCI8, "parse_connect_string", oci8_parse_connect_string, 1);
    rb_define_method(cOCI8, "initialize", oci8_svcctx_initialize, -1);
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
    oci8_base_t *base;
    Check_Handle(obj, cOCI8, base);
    if (base->type == 0) {
        rb_raise(eOCIException, "invalid argument %s was freed already.", rb_class2name(CLASS_OF(obj)));
    }
    return (oci8_svcctx_t *)base;
}

OCISvcCtx *oci8_get_oci_svcctx(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);
    return svcctx->base.hp.svc;
}

OCISession *oci8_get_oci_session(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);

    if (svcctx->authhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp.ptr, OCI_HTYPE_SVCCTX, &svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp));
    }
    return svcctx->authhp;
}

void oci8_check_pid_consistency(oci8_svcctx_t *svcctx)
{
    if (svcctx->pid != getpid()) {
        rb_raise(rb_eRuntimeError, "The connection cannot be reused in the forked process.");
    }
}

