/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

/*
 * Document-class: OCI8
 *
 * The class to access Oracle database server.
 *
 * example:
 *   # output the emp table's content as CSV format.
 *   conn = OCI8.new(username, password)
 *   conn.exec('select * from emp') do |row|
 *     puts row.join(',')
 *   end
 *
 *   # execute PL/SQL block with bind variables.
 *   conn = OCI8.new(username, password)
 *   conn.exec('BEGIN procedure_name(:1, :2); END;',
 *              value_for_the_first_parameter,
 *              value_for_the_second_parameter)
 */
static VALUE cOCI8;

static VALUE oci8_svcctx_logoff(VALUE self);
static VALUE dummy(VALUE self)
{
    return Qnil;
}

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;

    /* ignore exceptions when logoff. */
    rb_rescue(oci8_svcctx_logoff, base->self, dummy, Qnil);
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

static VALUE sym_SYSDBA;
static VALUE sym_SYSOPER;
static ID id_at_prefetch_rows;
static ID id_at_username;
static ID id_set_prefetch_rows;

static VALUE oci8_s_oracle_client_version(VALUE klass)
{
    return INT2FIX(oracle_client_version);
}

#define CONN_STR_REGEX "/^([^(\\s|\\@)]*)\\/([^(\\s|\\@)]*)(?:\\@(\\S+))?(?:\\s+as\\s+(\\S*)\\s*)?$/i"
static void oci8_do_parse_connect_string(VALUE conn_str, VALUE *user, VALUE *pass, VALUE *dbname, VALUE *mode)
{
    static VALUE re = Qnil;
    if (NIL_P(re)) {
        re = rb_eval_string(CONN_STR_REGEX);
        rb_global_variable(&re);
    }
    StringValue(conn_str);
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
            StringValue(*mode);
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
 * Add a private method to test oci8_do_parse_connect_string().
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
 * connect to Oracle by _username_ and _password_.
 * set _dbname_ nil to connect the local database or TNS name to connect
 * via network. You can use the following syntax for _dbname_ if the client
 * library is Oracle 10g or upper.
 *
 *   //HOST_NAME_OR_IP:TNS_LISTENER_PORT/ORACLE_SID
 *
 * If you need DBA privilege, use :SYSDBA or :SYSOPER to _privilege_.
 *
 *   # connect to the local database.
 *   #   sqlplus scott/tiger
 *   conn = OCI8.new("scott", "tiger")
 *
 *   # connect via network with TNS name.
 *   # sqlplus scott/tiger@orcl.world
 *   conn = OCI8.new("scott", "tiger", "orcl.world")
 *
 *   # connect via network with database's host, port and SID.
 *   #   sqlplus scott/tiger@//localhost:1521/XE
 *   conn = OCI8.new("scott", "tiger", "//localhost:1521/XE")
 *
 *   # connect as SYSDBA
 *   #   sqlplus 'sys/change_on_install as sysdba'
 *   conn = OCI8.new("sys", "change_on_install", nil, :SYSDBA)
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
        StringValue(vusername); /* 1 */
        StringValue(vpassword); /* 2 */
    }
    if (!NIL_P(vdbname)) {
        StringValue(vdbname); /* 3 */
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
    svcctx->is_autocommit = 0;
#ifdef RUBY_VM
    svcctx->non_blocking = 0;
#endif
    svcctx->long_read_len = INT2FIX(65535);
    return Qnil;
}


/*
 * call-seq:
 *   logoff
 *
 * disconnect from Oracle. Uncommitted transaction will be
 * rollbacked.
 *
 * example:
 *   conn = OCI8.new("scott", "tiger")
 *   ... do something ...
 *   conn.logoff
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
 * prepare the SQL statement and return an instance of OCI8::Cursor.
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
 * commit the transaction.
 *
 * example:
 *   conn = OCI8.new("scott", "tiger")
 *   conn.exec("UPDATE emp SET sal = sal * 1.1") # yahoo
 *   conn.commit
 *   conn.logoff
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
 * rollback the transaction.
 *
 * example:
 *   conn = OCI8.new("scott", "tiger")
 *   conn.exec("UPDATE emp SET sal = sal * 0.9") # boos
 *   conn.rollback
 *   conn.logoff
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
 * return the status of blocking/non-blocking mode.
 * See non_blocking= also.
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
 * change the status of blocking/non-blocking mode. true for non-blocking
 * mode. false for blocking mode. The default is blocking.
 *
 * When blocking mode, long-time SQL execution blocks the ruby process
 * itself even though multithread application because ruby's thread is
 * not native one.
 *
 * when non-blocking mode, long-time SQL execution doesn't block the ruby
 * process. It only blocks the thread which executing the SQL statement.
 * But each SQL will need a bit more time because it checks the status by
 * polling.
 *
 * You can cancel an executing SQL by using OCI8#break from another thread.
 * The canceled thread raises OCIBreak exception.
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
 * return the state of the autocommit mode. The default value is
 * false. If true, the transaction is committed on every SQL executions.
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
 * change the status of the autocommit mode.
 *
 * example:
 *   conn = OCI8.new("scott", "tiger")
 *   conn.autocommit = true
 *   ... do something ...
 *   conn.logoff
 */
static VALUE oci8_set_autocommit(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    svcctx->is_autocommit = RTEST(val);
    return val;
}

/*
 * call-seq:
 *   long_read_len -> aFixnum   (new in 0.1.16)
 *
 * get the maximum fetch size for a LONG and LONG RAW column.
 */
static VALUE oci8_long_read_len(VALUE self)
{
    oci8_svcctx_t *svcctx = DATA_PTR(self);
    return svcctx->long_read_len;
}

/*
 * call-seq:
 *   long_read_len = aFixnum   (new in 0.1.16)
 *
 * change the maximum fetch size for a LONG and LONG RAW column.
 * The default value is 65535.
 *
 * example:
 *   conn = OCI8.new('scott', 'tiger'
 *   conn.long_read_len = 1000000
 *   cursor = con.exec('select content from articles where id = :1', 23478)
 *   row = cursor.fetch
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
 * cancel an executing SQL in an other thread.
 * See non_blocking= also.
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
 *   prefetch_rows = aFixnum   (new in 0.1.14)
 *
 * change the prefetch rows size. This reduces network round trips
 * when fetching multiple rows.
 */
static VALUE oci8_set_prefetch_rows(VALUE self, VALUE val)
{
    rb_ivar_set(self, id_at_prefetch_rows, val);
    return val;
}

VALUE Init_oci8(void)
{
#if 0 /* for rdoc */
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
#endif
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_class);

    sym_SYSDBA = ID2SYM(rb_intern("SYSDBA"));
    sym_SYSOPER = ID2SYM(rb_intern("SYSOPER"));
    id_at_prefetch_rows = rb_intern("@prefetch_rows");
    id_at_username = rb_intern("@username");
    id_set_prefetch_rows = rb_intern("prefetch_rows=");

    rb_define_singleton_method(cOCI8, "oracle_client_version", oci8_s_oracle_client_version, 0);
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
