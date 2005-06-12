/*
  error.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIError
=end
*/
#include "oci8.h"

/* Exception */
VALUE eOCIException;
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

NORETURN(static void oci8_raise2(dvoid *errhp, sword status, ub4 type, OCIStmt *stmthp));

static void oci8_raise2(dvoid *errhp, sword status, ub4 type, OCIStmt *stmthp)
{
  VALUE vcodes;
  VALUE vmessages;
  VALUE exc;
  OraText errmsg[1024];
  ub4 errcode;
  ub4 recodeno;
  VALUE msg;
  int i;
  int rv;

  switch (status) {
  case OCI_ERROR:
  case OCI_SUCCESS_WITH_INFO:
    vcodes = rb_ary_new();
    vmessages = rb_ary_new();
    for (recodeno = 1;;recodeno++) {
      /* get error string */
      rv = OCIErrorGet(errhp, recodeno, NULL, &errcode, errmsg, sizeof(errmsg), type);
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
    if (RARRAY(vmessages)->len > 0) {
      msg = RARRAY(vmessages)->ptr[0];
    } else {
      msg = rb_str_new2("ERROR");
    }
    if (status == OCI_ERROR) {
      exc = rb_funcall(eOCIError, oci8_id_new, 1, msg);
    } else {
      exc = rb_funcall(eOCISuccessWithInfo, oci8_id_new, 1, msg);
    }
    rb_ivar_set(exc, oci8_id_code, vcodes);
    rb_ivar_set(exc, oci8_id_message, vmessages);
#ifdef OCI_ATTR_PARSE_ERROR_OFFSET
    if (stmthp != NULL) {
      ub2 offset;
      rv = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &offset, 0, OCI_ATTR_PARSE_ERROR_OFFSET, errhp);
      if (rv == OCI_SUCCESS) {
	rb_ivar_set(exc, oci8_id_parse_error_offset, INT2FIX(offset));
      }
    }
#endif
#ifdef OCI_ATTR_STATEMENT
    if (stmthp != NULL) {
      text *sql;
      ub4 size;
      rv = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &sql, &size, OCI_ATTR_STATEMENT, errhp);
      if (rv == OCI_SUCCESS) {
	rb_ivar_set(exc, oci8_id_sql, rb_str_new(sql, size));
      }
    }
#endif
    rb_exc_raise(exc);
    break;
  case OCI_NO_DATA:
    rb_raise(eOCINoData, "No Data");
    break;
  case OCI_INVALID_HANDLE:
    rb_raise(eOCIInvalidHandle, "Invalid Handle");
    break;
  case OCI_NEED_DATA:
    rb_raise(eOCINeedData, "Need Data");
    break;
  case OCI_STILL_EXECUTING:
    rb_raise(eOCIStillExecuting, "Still Executing");
    break;
  case OCI_CONTINUE:
    rb_raise(eOCIContinue, "Continue");
    break;
  default:
    rb_raise(rb_eStandardError, "Unknown error (%d)", status);
  }
}

/*
=begin
--- OCIError#code()
=end
*/
static VALUE oci8_error_code(VALUE self)
{
  VALUE ary = rb_ivar_get(self, oci8_id_code);
  if (RARRAY(ary)->len == 0) {
    return Qnil;
  }
  return RARRAY(ary)->ptr[0];
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

void Init_oci8_error(void)
{
  oci8_id_code = rb_intern("code");
  oci8_id_message = rb_intern("message");
  oci8_id_parse_error_offset = rb_intern("parse_error_offset");
  oci8_id_sql = rb_intern("sql");

  eOCIException = rb_define_class("OCIException", rb_eStandardError);
  eOCINoData = rb_define_class("OCINoData", eOCIException);
  eOCIError = rb_define_class("OCIError", eOCIException);
  eOCIInvalidHandle = rb_define_class("OCIInvalidHandle", eOCIException);
  eOCINeedData = rb_define_class("OCINeedData", eOCIException);
  eOCIStillExecuting = rb_define_class("OCIStillExecuting", eOCIException);
  eOCIContinue = rb_define_class("OCIContinue", eOCIException);
  eOCISuccessWithInfo = rb_define_class("OCISuccessWithInfo", eOCIError);

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

void oci8_raise(OCIError *errhp, sword status, OCIStmt *stmthp)
{
  oci8_raise2(errhp, status, OCI_HTYPE_ERROR, stmthp);
}

void oci8_env_raise(OCIEnv *envhp, sword status)
{
  oci8_raise2(envhp, status, OCI_HTYPE_ENV, NULL);
}
