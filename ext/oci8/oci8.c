/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>
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

static void oci8_svcctx_free(oci8_base_t *base)
{
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)base;
    sword rv;

    oci_rc(svcctx, OCITransRollback(svcctx->base.hp, oci8_errhp, OCI_DEFAULT));
    switch (svcctx->logon_type) {
    case T_IMPLICIT:
        rv = OCILogoff(svcctx->base.hp, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->base.type = 0;
        svcctx->logon_type = T_NOT_LOGIN;
        break;
    case T_EXPLICIT:
        rv = OCISessionEnd(svcctx->base.hp, oci8_errhp, svcctx->authhp, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIServerDetach(svcctx->srvhp, oci8_errhp, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->logon_type = T_NOT_LOGIN;
        break;
    case T_NOT_LOGIN:
        break;
    }
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
static ID id_set_prefetch_rows;

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

    rb_scan_args(argc, argv, "22", &vusername, &vpassword, &vdbname, &vmode);

    StringValue(vusername); /* 1 */
    StringValue(vpassword); /* 2 */
    if (!NIL_P(vdbname)) {
        StringValue(vdbname); /* 3 */
    }
    if (NIL_P(vmode)) { /* 4 */
#if defined(HAVE_OCISESSIONGET)
#error TODO
#elif defined(HAVE_OCILOGON2)
#error TODO
#else
        rv = OCILogon(oci8_envhp, oci8_errhp, (OCISvcCtx **)&svcctx->base.hp,
                      RSTRING_PTR(vusername), RSTRING_LEN(vusername),
                      RSTRING_PTR(vpassword), RSTRING_LEN(vpassword),
                      NIL_P(vdbname) ? NULL : RSTRING_PTR(vdbname),
                      NIL_P(vdbname) ? 0 : RSTRING_LEN(vdbname));
        if (rv != OCI_SUCCESS) {
            oci8_raise(oci8_errhp, rv, NULL);
        }
#endif
        svcctx->base.type = OCI_HTYPE_SVCCTX;
        svcctx->logon_type = T_IMPLICIT;
    } else {
        ub4 mode;

        Check_Type(vmode, T_SYMBOL);
        if (vmode == sym_SYSDBA) {
            mode = OCI_SYSDBA;
        } else if (vmode == sym_SYSOPER) {
            mode = OCI_SYSOPER;
        } else {
            rb_raise(rb_eArgError, "invalid privilege name %s (expect :SYSDBA or :SYSOPER)", rb_id2name(SYM2ID(vmode)));
        }
        /* allocate OCI handles. */
        rv = OCIHandleAlloc(oci8_envhp, (void*)&svcctx->base.hp, OCI_HTYPE_SVCCTX, 0, 0);
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
        rv = OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                        RSTRING_PTR(vusername), RSTRING_LEN(vusername),
                        OCI_ATTR_USERNAME, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->authhp, OCI_HTYPE_SESSION,
                        RSTRING_PTR(vpassword), RSTRING_LEN(vpassword),
                        OCI_ATTR_PASSWORD, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);

        /* attach to server and set to OCISvcCtx. */
        rv = OCIServerAttach(svcctx->srvhp, oci8_errhp,
                             NIL_P(vdbname) ? NULL : RSTRING_PTR(vdbname),
                             NIL_P(vdbname) ? 0 : RSTRING_LEN(vdbname), OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->base.hp, OCI_HTYPE_SVCCTX, svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);

        /* begin session. */
        rv = OCISessionBegin(svcctx->base.hp, oci8_errhp, svcctx->authhp, OCI_CRED_RDBMS, mode);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        rv = OCIAttrSet(svcctx->base.hp, OCI_HTYPE_SVCCTX, svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        svcctx->logon_type = T_EXPLICIT;
    }
    svcctx->executing_thread = NB_STATE_NOT_EXECUTING;
    svcctx->is_autocommit = 0;
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
    oci8_base_free((oci8_base_t*)DATA_PTR(self));
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
    oci_rc(svcctx, OCITransCommit(svcctx->base.hp, oci8_errhp, OCI_DEFAULT));
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
    oci_rc(svcctx, OCITransRollback(svcctx->base.hp, oci8_errhp, OCI_DEFAULT));
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
    sb1 non_blocking;

    if (svcctx->srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
    return non_blocking ? Qtrue : Qfalse;
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
    sb1 non_blocking;

    if (svcctx->srvhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->srvhp, 0, OCI_ATTR_SERVER, oci8_errhp));
    }
    oci_lc(OCIAttrGet(svcctx->srvhp, OCI_HTYPE_SERVER, &non_blocking, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
    if ((RTEST(val) && !non_blocking) || (!RTEST(val) && non_blocking)) {
        /* toggle blocking / non-blocking. */
        oci_lc(OCIAttrSet(svcctx->srvhp, OCI_HTYPE_SERVER, 0, 0, OCI_ATTR_NONBLOCKING_MODE, oci8_errhp));
    }
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
    sword rv;

    if (svcctx->executing_thread == NB_STATE_NOT_EXECUTING) {
        return Qfalse;
    }
    if (svcctx->executing_thread == NB_STATE_CANCELING) {
        return Qtrue;
    }
    rv = OCIBreak(svcctx->base.hp, oci8_errhp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
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
    id_set_prefetch_rows = rb_intern("prefetch_rows=");

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
        rb_raise(rb_eTypeError, "invalid argument %s was freed already.", rb_class2name(CLASS_OF(obj)));
    }
    return (oci8_svcctx_t *)base;
}

OCISvcCtx *oci8_get_oci_svcctx(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);
    return svcctx->base.hp;
}

OCISession *oci8_get_oci_session(VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(obj);

    if (svcctx->authhp == NULL) {
        oci_lc(OCIAttrGet(svcctx->base.hp, OCI_HTYPE_SVCCTX, &svcctx->authhp, 0, OCI_ATTR_SESSION, oci8_errhp));
    }
    return svcctx->authhp;
}
