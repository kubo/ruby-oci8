/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  error.c - part of ruby-oci8

  Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIError
=end
*/
#include "oci8.h"

/* Exception */
VALUE eOCIException;
VALUE eOCIBreak;
static VALUE eOCINoData;
static VALUE eOCIError;
static VALUE eOCIInvalidHandle;
static VALUE eOCINeedData;
static VALUE eOCIStillExecuting;
static VALUE eOCIContinue;
static VALUE eOCISuccessWithInfo;

static ID oci8_id_code;
static ID oci8_id_message;
static ID oci8_id_parse_error_offset;
static ID oci8_id_sql;
static ID oci8_id_caller;
static ID oci8_id_set_backtrace;

NORETURN(static void oci8_raise2(dvoid *errhp, sword status, ub4 type, OCIStmt *stmthp, const char *file, int line));
NORETURN(static void set_backtrace_and_raise(VALUE exc, const char *file, int line));

static void oci8_raise2(dvoid *errhp, sword status, ub4 type, OCIStmt *stmthp, const char *file, int line)
{
    VALUE vcodes = Qnil;
    VALUE vmessages = Qnil;
    VALUE exc;
    char errmsg[1024];
    sb4 errcode;
    ub4 recodeno;
    VALUE msg;
#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
    VALUE vparse_error_offset = Qnil;
#endif
#ifdef OCI_ATTR_STATEMENT
    VALUE vsql = Qnil;
#endif
    int i;
    int rv;

    switch (status) {
    case OCI_ERROR:
    case OCI_SUCCESS_WITH_INFO:
        vcodes = rb_ary_new();
        vmessages = rb_ary_new();
        for (recodeno = 1;;recodeno++) {
            /* get error string */
            rv = OCIErrorGet(errhp, recodeno, NULL, &errcode, TO_ORATEXT(errmsg), sizeof(errmsg), type);
            if (rv != OCI_SUCCESS) {
                break;
            }
            /* chop error string */
            for (i = strlen(errmsg) - 1;i >= 0;i--) {
                if (errmsg[i] == '\n' || errmsg[i] == '\r') {
                    errmsg[i] = '\0';
                } else {
                    break;
                }
            }
            rb_ary_push(vcodes, INT2FIX(errcode));
            rb_ary_push(vmessages, rb_str_new2(errmsg));
        }
        if (RARRAY_LEN(vmessages) > 0) {
            msg = RARRAY_PTR(vmessages)[0];
        } else {
            msg = rb_usascii_str_new_cstr("ERROR");
        }
        if (status == OCI_ERROR) {
            exc = eOCIError;
        } else {
            exc = eOCISuccessWithInfo;
        }
#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
        if (stmthp != NULL) {
            ub2 offset;
            rv = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &offset, 0, OCI_ATTR_PARSE_ERROR_OFFSET, errhp);
            if (rv == OCI_SUCCESS) {
                vparse_error_offset = INT2FIX(offset);
            }
        }
#endif
#ifdef OCI_ATTR_STATEMENT
        if (stmthp != NULL) {
            text *sql;
            ub4 size;
            rv = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &sql, &size, OCI_ATTR_STATEMENT, errhp);
            if (rv == OCI_SUCCESS) {
                vsql = rb_str_new(TO_CHARPTR(sql), size);
            }
        }
#endif
        break;
    case OCI_NO_DATA:
        exc = eOCINoData;
        msg = rb_usascii_str_new_cstr("No Data");
        break;
    case OCI_INVALID_HANDLE:
        exc = eOCIInvalidHandle;
        msg = rb_usascii_str_new_cstr("Invalid Handle");
        break;
    case OCI_NEED_DATA:
        exc = eOCINeedData;
        msg = rb_usascii_str_new_cstr("Need Data");
        break;
    case OCI_STILL_EXECUTING:
        exc = eOCIStillExecuting;
        msg = rb_usascii_str_new_cstr("Still Executing");
        break;
    case OCI_CONTINUE:
        exc = eOCIContinue;
        msg = rb_usascii_str_new_cstr("Continue");
        break;
    default:
        sprintf(errmsg, "Unknown error (%d)", status);
        exc = rb_eStandardError;
        msg = rb_usascii_str_new_cstr(errmsg);
    }
    exc = rb_funcall(exc, oci8_id_new, 1, msg);
    if (!NIL_P(vcodes)) {
        rb_ivar_set(exc, oci8_id_code, vcodes);
    }
    if (!NIL_P(vmessages)) {
        rb_ivar_set(exc, oci8_id_message, vmessages);
    }
#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
    if (!NIL_P(vparse_error_offset)) {
        rb_ivar_set(exc, oci8_id_parse_error_offset, vparse_error_offset);
    }
#endif
#ifdef OCI_ATTR_STATEMENT
    if (!NIL_P(vsql)) {
        rb_ivar_set(exc, oci8_id_sql, vsql);
    }
#endif
    set_backtrace_and_raise(exc, file, line);
}

static void set_backtrace_and_raise(VALUE exc, const char *file, int line)
{
    char errmsg[64];
    VALUE backtrace;
#ifdef _WIN32
    char *p = strrchr(file, '\\');
    if (p != NULL)
        file = p + 1;
#endif
    backtrace = rb_funcall(rb_cObject, oci8_id_caller, 0);
    if (TYPE(backtrace) == T_ARRAY) {
        snprintf(errmsg, sizeof(errmsg), "%s:%d:in oci8lib.so", file, line);
        errmsg[sizeof(errmsg) - 1] = '\0';
        rb_ary_unshift(backtrace, rb_usascii_str_new_cstr(errmsg));
        rb_funcall(exc, oci8_id_set_backtrace, 1, backtrace);
    }
    rb_exc_raise(exc);
}

static VALUE oci8_error_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE msg;
    VALUE code;

    rb_scan_args(argc, argv, "02", &msg, &code);
    rb_call_super(argc > 1 ? 1 : argc, argv);
    if (!NIL_P(code)) {
        rb_ivar_set(self, oci8_id_code, rb_ary_new3(1, code));
    }
    return Qnil;
}

/*
=begin
--- OCIError#code()
=end
*/
static VALUE oci8_error_code(VALUE self)
{
    VALUE ary = rb_ivar_get(self, oci8_id_code);
    if (RARRAY_LEN(ary) == 0) {
        return Qnil;
    }
    return RARRAY_PTR(ary)[0];
}

/*
=begin
--- OCIError#codes()
=end
*/
static VALUE oci8_error_code_array(VALUE self)
{
    return rb_ivar_get(self, oci8_id_code);
}

/*
=begin
--- OCIError#message()
=end
*/

/*
=begin
--- OCIError#messages()
=end
*/
static VALUE oci8_error_message_array(VALUE self)
{
    return rb_ivar_get(self, oci8_id_message);
}

#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
/*
=begin
--- OCIError#parseErrorOffset()
=end
*/
static VALUE oci8_error_parse_error_offset(VALUE self)
{
    return rb_ivar_get(self, oci8_id_parse_error_offset);
}
#endif

#ifdef OCI_ATTR_STATEMENT
/*
=begin
--- OCIError#sql()
     (Oracle 8.1.6 or later)
=end
*/
static VALUE oci8_error_sql(VALUE self)
{
    return rb_ivar_get(self, oci8_id_sql);
}
#endif

sb4 oci8_get_error_code(OCIError *errhp)
{
    sb4 errcode = -1;
    OCIErrorGet(oci8_errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
    return errcode;
}

void Init_oci8_error(void)
{
    oci8_id_code = rb_intern("code");
    oci8_id_message = rb_intern("message");
    oci8_id_parse_error_offset = rb_intern("parse_error_offset");
    oci8_id_sql = rb_intern("sql");
    oci8_id_caller = rb_intern("caller");
    oci8_id_set_backtrace = rb_intern("set_backtrace");

    eOCIException = rb_define_class("OCIException", rb_eStandardError);
    eOCIBreak = rb_define_class("OCIBreak", eOCIException);

    eOCINoData = rb_define_class("OCINoData", eOCIException);
    eOCIError = rb_define_class("OCIError", eOCIException);
    eOCIInvalidHandle = rb_define_class("OCIInvalidHandle", eOCIException);
    eOCINeedData = rb_define_class("OCINeedData", eOCIException);
    eOCIStillExecuting = rb_define_class("OCIStillExecuting", eOCIException);
    eOCIContinue = rb_define_class("OCIContinue", eOCIException);
    eOCISuccessWithInfo = rb_define_class("OCISuccessWithInfo", eOCIError);

    rb_define_method(eOCIError, "initialize", oci8_error_initialize, -1);
    rb_define_method(eOCIError, "code", oci8_error_code, 0);
    rb_define_method(eOCIError, "codes", oci8_error_code_array, 0);
    rb_define_method(eOCIError, "messages", oci8_error_message_array, 0);
#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
    rb_define_method(eOCIError, "parseErrorOffset", oci8_error_parse_error_offset, 0);
#endif
#ifdef OCI_ATTR_STATEMENT
    rb_define_method(eOCIError, "sql", oci8_error_sql, 0);
#endif
}

void oci8_do_raise(OCIError *errhp, sword status, OCIStmt *stmthp, const char *file, int line)
{
    oci8_raise2(errhp, status, OCI_HTYPE_ERROR, stmthp, file, line);
}

void oci8_do_env_raise(OCIEnv *envhp, sword status, const char *file, int line)
{
    oci8_raise2(envhp, status, OCI_HTYPE_ENV, NULL, file, line);
}

void oci8_do_raise_init_error(const char *file, int line)
{
    VALUE msg = rb_usascii_str_new_cstr("OCI Library Initialization Error");
    VALUE exc = rb_funcall(eOCIError, oci8_id_new, 1, msg);

    rb_ivar_set(exc, oci8_id_code, rb_ary_new3(1, INT2FIX(-1)));
    rb_ivar_set(exc, oci8_id_message, rb_ary_new3(1, msg));
    set_backtrace_and_raise(exc, file, line);
}
