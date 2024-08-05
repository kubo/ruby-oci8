/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2019 Kubo Takehiro <kubo@jiubao.org>
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
#ifndef OCI_ATTR_TRANSACTION_IN_PROGRESS
#define OCI_ATTR_TRANSACTION_IN_PROGRESS 484
#endif

#define OCI8_STATE_SESSION_BEGIN_WAS_CALLED 0x01
#define OCI8_STATE_SERVER_ATTACH_WAS_CALLED 0x02
#define OCI8_STATE_CPOOL 0x04

static VALUE cOCI8;
static VALUE cSession;
static VALUE cServer;
static VALUE cEnvironment;
static VALUE cProcess;
static ID id_at_session_handle;
static ID id_at_server_handle;
static ID id_at_trans_handle;

typedef struct {
    const char *name;
    ub4 val;
} ub4_flag_def_t;

static ub4 check_ub4_flags(int argc, VALUE *argv, const ub4_flag_def_t *flag_defs, const char *flag_name)
{
    ub4 flags = 0;
    int i;

    for (i = 0; i < argc; i++) {
        VALUE val = argv[i];
        const char *name;
        size_t len;
        int j;

        if (SYMBOL_P(val)) {
#ifdef HAVE_RB_SYM2STR
            VALUE symstr = rb_sym2str(val);
            name = RSTRING_PTR(symstr);
            len = RSTRING_LEN(symstr);
#else
            name = rb_id2name(SYM2ID(val));
            len = strlen(symname);
#endif
        } else {
            SafeStringValue(val);
            name = RSTRING_PTR(val);
            len = RSTRING_LEN(val);
        }
        for (j = 0; flag_defs[j].name != NULL; j++) {
            if (strncmp(name, flag_defs[j].name, len) != 0) {
                continue;
            }
            if (flag_defs[j].name[len] != '\0') {
                continue;
            }
            flags |= flag_defs[j].val;
            break;
        }
        if (flag_defs[j].name == NULL) {
            rb_raise(rb_eArgError, "Unknown %s flag: %.*s", flag_name, (int)len, name);
        }
    }
    return flags;
}

static VALUE dummy_env_method_missing(int argc, VALUE *argv, VALUE self)
{
    VALUE obj = rb_cv_get(cOCI8, "@@environment_handle");
    VALUE method_id, args;

    if (self == obj) {
        oci8_base_t *base;
        obj = rb_obj_alloc(cEnvironment);
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

static void oci8_handle_dont_free(oci8_base_t *base)
{
    base->type = 0;
    base->closed = 1;
    base->hp.ptr = NULL;
}

static const oci8_handle_data_type_t oci8_session_data_type = {
    {
        "OCI8::Session",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_handle_dont_free,
    sizeof(oci8_base_t),
};

static VALUE oci8_session_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_session_data_type);
}

static const oci8_handle_data_type_t oci8_server_data_type = {
    {
        "OCI8::Server",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_handle_dont_free,
    sizeof(oci8_base_t),
};

static VALUE oci8_server_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_server_data_type);
}

static const oci8_handle_data_type_t oci8_environment_data_type = {
    {
        "OCI8::Environment",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_handle_dont_free,
    sizeof(oci8_base_t),
};

static VALUE oci8_environment_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_environment_data_type);
}

static const oci8_handle_data_type_t oci8_process_data_type = {
    {
        "OCI8::Process",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_handle_dont_free,
    sizeof(oci8_base_t),
};

static VALUE oci8_process_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_process_data_type);
}

static void copy_session_handle(oci8_svcctx_t *svcctx)
{
    VALUE obj = rb_ivar_get(svcctx->base.self, id_at_session_handle);
    oci8_base_t *base = oci8_check_typeddata(obj, &oci8_session_data_type, 1);

    base->type = OCI_HTYPE_SESSION;
    base->hp.usrhp = svcctx->usrhp;
}

static void copy_server_handle(oci8_svcctx_t *svcctx)
{
    VALUE obj = rb_ivar_get(svcctx->base.self, id_at_server_handle);
    oci8_base_t *base = oci8_check_typeddata(obj, &oci8_server_data_type, 1);

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
        svcctx->base.closed = 1;
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
    svcctx->base.closed = 1;
}

static const oci8_handle_data_type_t oci8_svcctx_data_type = {
    {
        "OCI8",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_svcctx_free,
    sizeof(oci8_svcctx_t),
};

static VALUE oci8_svcctx_alloc(VALUE klass)
{
    VALUE self = oci8_allocate_typeddata(klass, &oci8_svcctx_data_type);
    oci8_svcctx_t *svcctx = (oci8_svcctx_t *)RTYPEDDATA_DATA(self);
    VALUE obj;

    svcctx->executing_thread = Qnil;
    /* set session handle */
    obj = rb_obj_alloc(cSession);
    rb_ivar_set(self, id_at_session_handle, obj);
    oci8_link_to_parent(DATA_PTR(obj), &svcctx->base);
    /* set server handle */
    obj = rb_obj_alloc(cServer);
    rb_ivar_set(self, id_at_server_handle, obj);
    oci8_link_to_parent(DATA_PTR(obj), &svcctx->base);

    svcctx->pid = getpid();
    svcctx->is_autocommit = 0;
    svcctx->non_blocking = 1;
    svcctx->long_read_len = INT2FIX(65535);
    return self;
}

static VALUE oracle_client_vernum; /* Oracle client version number */

/*
 * @overload oracle_client_vernum
 *
 *  Returns Oracle client version as <i>Integer</i>.
 *  The Oracle version is encoded in 32-bit integer.
 *  It is devided into 5 parts: 8, 4, 8, 4 and 8 bits.
 *
 *  @example
 *    # Oracle 10.2.0.4
 *    oracle_client_vernum # => 0x0a200400
 *                         # => 0x0a 2 00 4 00
 *                         # => 10.2.0.4.0
 *
 *    # Oracle 11.1.0.7.0
 *    oracle_client_vernum # => 0x0b100700
 *                         # => 0x0b 1 00 7 00
 *                         # => 11.1.0.7.0
 *
 *  @return [Integer]
 *  @private
 */
static VALUE oci8_s_oracle_client_vernum(VALUE klass)
{
    return oracle_client_vernum;
}

/*
 * @overload OCI8.__get_prop(key)
 *
 *  @param [Integer] key   1, 2 or 3
 *  @return [Object]       depends on +key+.
 *  @private
 */
static VALUE oci8_s_get_prop(VALUE klass, VALUE key)
{
    switch (NUM2INT(key)) {
    case 1:
        return oci8_float_conversion_type_is_ruby ? Qtrue : Qfalse;
    case 2:
        return UINT2NUM(oci8_env_mode);
    default:
        rb_raise(rb_eArgError, "Unknown prop %d", NUM2INT(key));
    }
}


/*
 * @overload OCI8.__set_prop(key, value)
 *
 *  @param [Integer] key   1, 2 or 3
 *  @param [Object] value  depends on +key+.
 *
 *  @private
 */
static VALUE oci8_s_set_prop(VALUE klass, VALUE key, VALUE val)
{
    switch (NUM2INT(key)) {
    case 1:
        oci8_float_conversion_type_is_ruby = RTEST(val) ? 1 : 0;
        break;
    case 2:
        /*
         * Changes the OCI environment mode which will be passed to the second
         * argument of the OCI function OCIEnvCreate.
         */
        if (oci8_global_envhp != NULL) {
            rb_raise(rb_eRuntimeError, "The OCI Environment has been alreadly initialized. It cannot be changed after even one OCI function is called.");
        }
        oci8_env_mode = NUM2UINT(val);
        break;
    case 3:
        if (oci8_cancel_read_at_exit == -1) {
            rb_raise(rb_eNotImpError, "OCI8.properties[:cancel_read_at_exit] isn't available.");
        }
#ifdef HAVE_PLTHOOK
        oci8_install_hook_functions();
        oci8_cancel_read_at_exit = RTEST(val) ? 1 : 0;
#endif
        break;
    case 4:
        if (oci8_tcp_keepalive_time == -1) {
            rb_raise(rb_eNotImpError, "OCI8.properties[:tcp_keepalive_time] isn't available.");
        }
#ifdef HAVE_PLTHOOK
        oci8_install_hook_functions();
        oci8_tcp_keepalive_time = NIL_P(val) ? 0 : NUM2INT(val);
#endif
        break;
    default:
        rb_raise(rb_eArgError, "Unknown prop %d", NUM2INT(key));
    }
    return klass;
}

/*
 * @overload error_message(message_no)
 *
 *  Get the Oracle error message specified by message_no.
 *  Its language depends on NLS_LANGUAGE.
 *
 *  @example
 *    # When NLS_LANG is AMERICAN_AMERICA.AL32UTF8
 *    OCI8.error_message(1) # => "ORA-00001: unique constraint (%s.%s) violated"
 *
 *    # When NLS_LANG is FRENCH_FRANCE.AL32UTF8
 *    OCI8.error_message(1) # => "ORA-00001: violation de contrainte unique (%s.%s)"
 *
 *  @param [Integer] message_no  Oracle error message number
 *  @return [String]             Oracle error message
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
        rb_global_variable(&re);
        re = rb_eval_string(CONN_STR_REGEX);
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
 * @overload parse_connect_string(connect_string)
 *
 *  Extracts +username+, +password+, +dbname+ and +privilege+ from +connect_string+.
 *
 *  @example
 *    "scott/tiger" -> ["scott", "tiger", nil, nil],
 *    "scott/tiger@oradb.example.com" -> ["scott", "tiger", "oradb.example.com", nil]
 *    "sys/change_on_install as sysdba" -> ["sys", "change_on_install", nil, :SYSDBA]
 *
 *  @param [String] connect_string
 *  @return [Array] [username, password, dbname]
 *  @private
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
    complex_logoff_arg_t *cla = malloc(sizeof(complex_logoff_arg_t));
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
    boolean txn = TRUE;
    sword rv = OCI_SUCCESS;

    if (oracle_client_version >= ORAVER_12_1) {
        OCIAttrGet(cla->usrhp, OCI_HTYPE_SESSION, &txn, NULL, OCI_ATTR_TRANSACTION_IN_PROGRESS, errhp);
    }
    if (txn) {
        OCITransRollback(cla->svchp, errhp, OCI_DEFAULT);
    }

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
    free(cla);
    return (void*)(VALUE)rv;
}

static const oci8_logoff_strategy_t complex_logoff = {
    complex_logoff_prepare,
    complex_logoff_execute,
};

/*
 * @overload allocate_handles()
 *
 *  Allocates a service context handle, a session handle and a
 *  server handle to use explicit attach and begin-session calls.
 *
 *  @private
 */
static VALUE oci8_allocate_handles(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
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
 * @overload server_attach(dbname, mode)
 *
 *  Attachs to the server by the OCI function OCIServerAttach().
 *
 *  @param [String] dbname
 *  @param [Integer] mode
 *  @private
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
                              NIL_P(dbname) ? 0 : RSTRING_LENINT(dbname),
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
 * @overload session_begin(cred, mode)
 *
 *  Begins the session by the OCI function OCISessionBegin().
 *
 *  @param [Integer] cred
 *  @param [Integer] mode
 *  @private
 */
static VALUE oci8_session_begin(VALUE self, VALUE cred, VALUE mode)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    char buf[100];
    ub4 version;

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
    if (have_OCIServerRelease2) {
        chker2(OCIServerRelease2(svcctx->base.hp.ptr, oci8_errhp, (text*)buf,
                                 sizeof(buf), (ub1)svcctx->base.type, &version, OCI_DEFAULT),
               &svcctx->base);
    } else {
        chker2(OCIServerRelease(svcctx->base.hp.ptr, oci8_errhp, (text*)buf,
                                sizeof(buf), (ub1)svcctx->base.type, &version),
               &svcctx->base);
    }
    svcctx->server_version = version;
    return Qnil;
}

/*
 * @overload logoff
 *
 *  Disconnects from the Oracle server. The uncommitted transaction is
 *  rollbacked.
 */
static VALUE oci8_svcctx_logoff(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);

    while (svcctx->base.children != NULL) {
        oci8_base_free(svcctx->base.children);
    }
    if (svcctx->logoff_strategy != NULL) {
        const oci8_logoff_strategy_t *strategy = svcctx->logoff_strategy;
        void *data = strategy->prepare(svcctx);
        svcctx->base.type = 0;
        svcctx->base.closed = 1;
        svcctx->logoff_strategy = NULL;
        chker2(oci8_call_without_gvl(svcctx, strategy->execute, data), &svcctx->base);
    }
    return Qtrue;
}

/*
 * @overload commit(*flags)
 *
 *  Commits the transaction.
 *
 *  @param []
 */
static VALUE oci8_commit(int argc, VALUE *argv, VALUE self)
{
    static const ub4_flag_def_t flag_defs[] = {
        {"two_phase", OCI_TRANS_TWOPHASE},
        {"write_immediate", OCI_TRANS_WRITEIMMED},
        {"write_batch", OCI_TRANS_WRITEBATCH},
        {"write_wait", OCI_TRANS_WRITEWAIT},
        {"write_no_wait", OCI_TRANS_WRITENOWAIT},
        {NULL, 0},
    };
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    ub4 flags = check_ub4_flags(argc, argv, flag_defs, "commit");

    chker2(OCITransCommit_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, flags),
           &svcctx->base);
    return self;
}

/*
 * @overload rollback
 *
 *  Rollbacks the transaction.
 */
static VALUE oci8_rollback(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    chker2(OCITransRollback_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT), &svcctx->base);
    return self;
}

/*
 * @overload non_blocking?
 *
 *  Returns +true+ if the connection is in non-blocking mode, +false+
 *  otherwise.
 *
 *  @see #non_blocking=
 */
static VALUE oci8_non_blocking_p(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    return svcctx->non_blocking ? Qtrue : Qfalse;
}

/*
 * @overload non_blocking=(non_blocking_mode)
 *
 *  Sets +true+ to enable non-blocking mode, +false+ otherwise.
 *  The default value is +true+ except ruby 1.8.
 *
 *  When the connection is in non-blocking mode (non_blocking = true),
 *  an SQL execution blocks the thread running the SQL.
 *  It does't prevent other threads. The blocking thread can be canceled
 *  by {OCI8#break}.
 *
 *  When in blocking mode (non_blocking = false), an SQL execution blocks
 *  not only the thread, but also the ruby process itself. It makes the
 *  whole application stop until the SQL finishes.
 *
 *  @param [Boolean] non_blocking_mode
 */
static VALUE oci8_set_non_blocking(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    svcctx->non_blocking = RTEST(val);
    return val;
}

/*
 * @overload autocommit?
 *
 *  Returns +true+ if the connection is in autocommit mode, +false+
 *  otherwise. The default value is +false+.
 */
static VALUE oci8_autocommit_p(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    return svcctx->is_autocommit ? Qtrue : Qfalse;
}

/*
 * @overload autocommit=(autocommit_mode)
 *
 *  Sets the autocommit mode. The default value is +false+.
 *
 *  @param [Boolean] autocommit_mode
 */
static VALUE oci8_set_autocommit(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    svcctx->is_autocommit = RTEST(val);
    return val;
}

/*
 * @overload long_read_len
 *
 *  @deprecated This has no effect since ruby-oci8 2.2.7.
 *    LONG, LONG RAW and XMLTYPE columns are fetched up to 4 gigabytes
 *    without this parameter.
 *
 *  @return [Integer]
 *  @see #long_read_len=
 */
static VALUE oci8_long_read_len(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    rb_warning("OCI8.long_read_len has no effect since ruby-oci8 2.2.7");
    return svcctx->long_read_len;
}

/*
 * @overload long_read_len=(length)
 *
 *  @deprecated This has no effect since ruby-oci8 2.2.7.
 *    LONG, LONG RAW and XMLTYPE columns are fetched up to 4 gigabytes
 *    without this parameter.
 *
 *  @param [Integer] length
 *  @see #long_read_len
 */
static VALUE oci8_set_long_read_len(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    Check_Type(val, T_FIXNUM);
    RB_OBJ_WRITE(self, &svcctx->long_read_len, val);
    rb_warning("OCI8.long_read_len= has no effect since ruby-oci8 2.2.7");
    return val;
}

/*
 * @overload break
 *
 *  Cancels the executing SQL.
 *
 *  Note that this doesn't work when the following cases.
 *
 *  * The Oracle server runs on Windows.
 *  * {Out-of-band data}[http://en.wikipedia.org/wiki/Transmission_Control_Protocol#Out-of-band_data] are blocked by a firewall or by a VPN.
 *
 *  In the latter case, create an sqlnet.ora file in the path specified
 *  by the TNS_ADMIN environment variable that sets {DISABLE_OOB=on}[http://www.orafaq.com/wiki/SQL*Net_FAQ#What_are_inband_and_out_of_band_breaks.3F].
 *
 *  @see OCI8#non_blocking=
 */
static VALUE oci8_break(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);

    if (NIL_P(svcctx->executing_thread)) {
        return Qfalse;
    }
    rb_thread_wakeup(svcctx->executing_thread);
    return Qtrue;
}

/*
 * @overload oracle_server_vernum
 *
 *  Returns a numerical format of the Oracle server version.
 *
 *  @return [Integer]
 *  @see OCI8#oracle_server_version
 *  @since 2.0.1
 *  @private
 */
static VALUE oci8_oracle_server_vernum(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);

    return UINT2NUM(svcctx->server_version);
}

/*
 * @overload ping
 *
 *  Makes a round trip call to the server to confirm that the connection and
 *  the server are active.
 *
 *  This also flushes all the pending OCI client-side calls such as {OCI8#action=},
 *  {OCI8#client_identifier=}, {OCI8#client_info=} and {OCI8#module=}.
 *
 *  === Oracle 10.2 client or upper
 *  A dummy round trip call is made by the OCI function
 *  OCIPing[http://download.oracle.com/docs/cd/B19306_01/appdev.102/b14250/oci16msc007.htm#sthref3540] added in Oracle 10.2.
 *
 *  === Oracle 10.1 client or lower
 *  A simple PL/SQL block "BEGIN NULL; END;" is executed to make a round trip call.
 *
 *  @return [Boolean]
 *  @since 2.0.2
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
 * @overload client_identifier=(client_identifier)
 *
 *  Sets the specified value to {V$SESSION.CLIENT_IDENTIFIER}[http://docs.oracle.com/database/121/REFRN/refrn30223.htm#r62c1-t21].
 *
 *  The specified value is sent to the server by piggybacking on the next network
 *  round trip issued by {OCI8#exec}, {OCI8#ping} and so on.
 *
 *  @param [String] client_identifier
 *  @since 2.0.3
 */
static VALUE oci8_set_client_identifier(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LENINT(val);
    } else {
        ptr = "";
        size = 0;
    }

    if (size > 0 && ptr[0] == ':') {
        rb_raise(rb_eArgError, "client identifier should not start with ':'.");
    }
    chker2(OCIAttrSet(svcctx->usrhp, OCI_HTYPE_SESSION, (dvoid*)ptr,
                      size, OCI_ATTR_CLIENT_IDENTIFIER, oci8_errhp),
           &svcctx->base);
    return val;
}

/*
 * @overload module=(module)
 *
 *  Sets the specified value to {V$SESSION.MODULE}[http://docs.oracle.com/database/121/REFRN/refrn30223.htm#r40c1-t21].
 *  This is also stored in {V$SQL.MODULE}[http://docs.oracle.com/database/121/REFRN/refrn30246.htm#r49c1-t58]
 *  and {V$SQLAREA.MODULE}[http://docs.oracle.com/database/121/REFRN/refrn30259.htm#r46c1-t94]
 *  when an SQL statement is first parsed in the Oracle server.
 *
 *  @param [String] module
 *  @since 2.0.3
 */
static VALUE oci8_set_module(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LENINT(val);
    } else {
        ptr = "";
        size = 0;
    }
    chker2(OCIAttrSet(svcctx->usrhp, OCI_HTYPE_SESSION, (dvoid*)ptr,
                      size, OCI_ATTR_MODULE, oci8_errhp),
           &svcctx->base);
    return self;
}

/*
 * @overload action=(action)
 *
 *  Sets the specified value to {V$SESSION.ACTION}[http://docs.oracle.com/database/121/REFRN/refrn30223.htm#r42c1-t21].
 *  This is also stored in {V$SQL.ACTION}[http://docs.oracle.com/database/121/REFRN/refrn30246.htm#r51c1-t58]
 *  and {V$SQLAREA.ACTION}[http://docs.oracle.com/database/121/REFRN/refrn30259.htm#r48c1-t94]
 *  when an SQL statement is first parsed in the Oracle server.
 *
 *  The specified value is sent to the server by piggybacking on the next network
 *  round trip issued by {OCI8#exec}, {OCI8#ping} and so on.
 *
 *  @param [String] action
 *  @since 2.0.3
 */
static VALUE oci8_set_action(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LENINT(val);
    } else {
        ptr = "";
        size = 0;
    }
    chker2(OCIAttrSet(svcctx->usrhp, OCI_HTYPE_SESSION, (dvoid*)ptr,
                      size, OCI_ATTR_ACTION, oci8_errhp),
           &svcctx->base);
    return val;
}

/*
 * @overload client_info=(client_info)
 *
 *  Sets the specified value to {V$SESSION.CLIENT_INFO}[http://docs.oracle.com/database/121/REFRN/refrn30223.htm#r44c1-t21].
 *
 *  The specified value is sent to the server by piggybacking on the next network
 *  round trip issued by {OCI8#exec}, {OCI8#ping} and so on.
 *
 *  @param [String] client_info
 *  @since 2.0.3
 */
static VALUE oci8_set_client_info(VALUE self, VALUE val)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    const char *ptr;
    ub4 size;

    if (!NIL_P(val)) {
        OCI8SafeStringValue(val);
        ptr = RSTRING_PTR(val);
        size = RSTRING_LENINT(val);
    } else {
        ptr = "";
        size = 0;
    }
    chker2(OCIAttrSet(svcctx->usrhp, OCI_HTYPE_SESSION, (dvoid*)ptr,
                      size, OCI_ATTR_CLIENT_INFO, oci8_errhp),
           &svcctx->base);
    return val;
}

static VALUE oci8_get_trans_handle(VALUE self)
{
    return rb_ivar_get(self, id_at_trans_handle);
}

static VALUE oci8_set_trans_handle(VALUE self, VALUE obj)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    oci8_base_t *trans = oci8_check_typeddata(obj, &oci8_trans_data_type, 1);
    chker2(OCIAttrSet(svcctx->base.hp.svc, OCI_HTYPE_SVCCTX, trans->hp.ptr, 0, OCI_ATTR_TRANS, oci8_errhp),
           &svcctx->base);
    rb_ivar_set(self, id_at_trans_handle, obj);
    return obj;
}

static VALUE oci8_trans_start(int argc, VALUE *argv, VALUE self)
{
    static const ub4_flag_def_t flag_defs[] = {
        {"new", OCI_TRANS_NEW},
        {"tight", OCI_TRANS_TIGHT},
        {"loose", OCI_TRANS_LOOSE},
        {"resume", OCI_TRANS_RESUME},
        {"readonly", OCI_TRANS_READONLY},
        {"serializable", OCI_TRANS_SERIALIZABLE},
        {NULL, 0},
    };
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    uword timeout;
    ub4 flags;

    if (argc == 0) {
        rb_raise(rb_eArgError, "wrong number of arguments (given 0, expected 1+)");
    }
    timeout = NUM2UINT(argv[0]);
    flags = check_ub4_flags(argc - 1, argv + 1, flag_defs, "trans_start");
    chker2(OCITransStart_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, timeout, flags),
           &svcctx->base);
    return self;
}

static VALUE oci8_trans_detach(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    chker2(OCITransDetach_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT),
           &svcctx->base);
    return self;
}

static VALUE oci8_trans_prepare(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    sword rv = OCITransPrepare_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT);
    chker2(rv, &svcctx->base);
    return rv == OCI_SUCCESS ? Qtrue : Qfalse;
}

static VALUE oci8_trans_forget(VALUE self)
{
    oci8_svcctx_t *svcctx = oci8_get_svcctx(self);
    chker2(OCITransForget_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, OCI_DEFAULT),
           &svcctx->base);
    return self;
}

void Init_oci8(VALUE *out)
{
    VALUE obj;
    oci8_base_t *base;
#if /* for yard */ 0
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif
    cOCI8 = oci8_define_class("OCI8", &oci8_svcctx_data_type, oci8_svcctx_alloc);
    cSession = oci8_define_class_under(cOCI8, "Session", &oci8_session_data_type, oci8_session_alloc);
    cServer = oci8_define_class_under(cOCI8, "Server", &oci8_server_data_type, oci8_server_alloc);
    cEnvironment = oci8_define_class_under(cOCI8, "Environment", &oci8_environment_data_type, oci8_environment_alloc);
    cProcess = oci8_define_class_under(cOCI8, "Process", &oci8_process_data_type, oci8_process_alloc);
    id_at_session_handle = rb_intern("@session_handle");
    id_at_server_handle = rb_intern("@server_handle");
    id_at_trans_handle = rb_intern("@trans_handle");

    /* setup a dummy environment handle to lazily initialize the environment handle */
    obj = rb_obj_alloc(rb_cObject);
    rb_define_singleton_method(obj, "method_missing", dummy_env_method_missing, -1);
    rb_cv_set(cOCI8, "@@environment_handle", obj);

    /* setup the process handle */
    obj = rb_obj_alloc(cProcess);
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

    rb_define_const(cOCI8, "LIB_VERSION", rb_obj_freeze(rb_usascii_str_new_cstr(OCI8LIB_VERSION)));
    rb_define_singleton_method_nodoc(cOCI8, "oracle_client_vernum", oci8_s_oracle_client_vernum, 0);
    rb_define_singleton_method_nodoc(cOCI8, "__get_prop", oci8_s_get_prop, 1);
    rb_define_singleton_method_nodoc(cOCI8, "__set_prop", oci8_s_set_prop, 2);
    rb_define_singleton_method(cOCI8, "error_message", oci8_s_error_message, 1);
    rb_define_private_method(cOCI8, "parse_connect_string", oci8_parse_connect_string, 1);
    rb_define_private_method(cOCI8, "allocate_handles", oci8_allocate_handles, 0);
    rb_define_private_method(cOCI8, "server_attach", oci8_server_attach, 2);
    rb_define_private_method(cOCI8, "session_begin", oci8_session_begin, 2);
    rb_define_method(cOCI8, "logoff", oci8_svcctx_logoff, 0);
    rb_define_method(cOCI8, "commit", oci8_commit, -1);
    rb_define_method(cOCI8, "rollback", oci8_rollback, 0);
    rb_define_method(cOCI8, "non_blocking?", oci8_non_blocking_p, 0);
    rb_define_method(cOCI8, "non_blocking=", oci8_set_non_blocking, 1);
    rb_define_method(cOCI8, "autocommit?", oci8_autocommit_p, 0);
    rb_define_method(cOCI8, "autocommit=", oci8_set_autocommit, 1);
    rb_define_method(cOCI8, "long_read_len", oci8_long_read_len, 0);
    rb_define_method(cOCI8, "long_read_len=", oci8_set_long_read_len, 1);
    rb_define_method(cOCI8, "break", oci8_break, 0);
    rb_define_private_method(cOCI8, "oracle_server_vernum", oci8_oracle_server_vernum, 0);
    rb_define_method(cOCI8, "ping", oci8_ping, 0);
    rb_define_method(cOCI8, "client_identifier=", oci8_set_client_identifier, 1);
    rb_define_method(cOCI8, "module=", oci8_set_module, 1);
    rb_define_method(cOCI8, "action=", oci8_set_action, 1);
    rb_define_method(cOCI8, "client_info=", oci8_set_client_info, 1);
    rb_define_method(cOCI8, "trans_handle", oci8_get_trans_handle, 0);
    rb_define_method(cOCI8, "trans_handle=", oci8_set_trans_handle, 1);
    rb_define_method(cOCI8, "trans_start", oci8_trans_start, -1);
    rb_define_method(cOCI8, "trans_detach", oci8_trans_detach, 0);
    rb_define_method(cOCI8, "trans_prepare", oci8_trans_prepare, 0);
    rb_define_method(cOCI8, "trans_forget", oci8_trans_forget, 0);
    *out = cOCI8;
}

oci8_svcctx_t *oci8_get_svcctx(VALUE obj)
{
    return (oci8_svcctx_t *)oci8_check_typeddata(obj, &oci8_svcctx_data_type, 1);
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

