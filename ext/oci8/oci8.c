/*
  oci8.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== class hierarchy
* ((<OCIHandle>))
  * ((<OCIEnv>)) (OCI environment handle)
  * ((<OCISvcCtx>)) (OCI service context handle)
  * ((<OCIServer>)) (OCI server handle)
  * ((<OCISession>)) (OCI user session handle)
  * ((<OCIStmt>)) (OCI statement handle)
  * ((<OCIDefine>)) (OCI define handle)
  * ((<OCIBind>)) (OCI bind handle)
  * ((<OCIDescribe>)) (OCI descibe handle)

* ((<OCIDescriptor>))
  * ((<OCILobLocator>)) (OCI Lob Locator descriptor)
  * ((<OCIParam>)) (read-only parameter descriptor)
  * ((<OCIRowid>)) (OCI ROWID descriptor)

* OCIException
  * ((<OCIError>))
    * ((<OCISuccessWithInfo>))
  * OCINoData
  * OCIInvalidHandle
  * OCINeedData
  * OCIStillExecuting

* ((<OraDate>))
* ((<OraNumber>))

== object hierarchy

* ((<OCIEnv>)) (OCI environment handle)
  * ((<OCISvcCtx>)) (OCI service context handle)
    * ((<OCIServer>)) (OCI server handle)
    * ((<OCISession>)) (OCI user session handle)
  * ((<OCIStmt>)) (OCI statement handle)
    * ((<OCIDefine>)) (OCI define handle)
    * ((<OCIBind>)) (OCI bind handle)
=end
*/
#include "oci8.h"

/* #define DEBUG_CORE_FILE 1 */
#ifdef DEBUG_CORE_FILE
#include <signal.h>
#endif

VALUE cOCIHandle;
VALUE cOCIEnv;
VALUE cOCIServer;
VALUE cOCISession;
VALUE cOCISvcCtx;
VALUE cOCIStmt;
VALUE cOCIDefine;
VALUE cOCIBind;

VALUE cOCIDescriptor;
VALUE cOCIParam;
VALUE cOCILobLocator;
VALUE cOCIRowid;

VALUE eOCIException;
VALUE eOCINoData;
VALUE eOCIError;
VALUE eOCIInvalidHandle;
VALUE eOCINeedData;
VALUE eOCIStillExecuting;
VALUE eOCIContinue;
VALUE eOCISuccessWithInfo;

VALUE cOraDate;
VALUE cOraNumber;

void
Init_oci8lib()
{
  /* Handle */
  cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
  cOCIEnv = rb_define_class("OCIEnv", cOCIHandle);
  cOCISvcCtx = rb_define_class("OCISvcCtx", cOCIHandle);
  cOCIServer = rb_define_class("OCIServer", cOCIHandle);
  cOCISession = rb_define_class("OCISession", cOCIHandle);
  cOCIStmt = rb_define_class("OCIStmt", cOCIHandle);
  cOCIDefine = rb_define_class("OCIDefine", cOCIHandle);
  cOCIBind = rb_define_class("OCIBind", cOCIHandle);

  /* Descriptor */
  cOCIDescriptor = rb_define_class("OCIDescriptor", rb_cObject);
  cOCILobLocator = rb_define_class("OCILobLocator", cOCIDescriptor);
  cOCIParam = rb_define_class("OCIParam", cOCIDescriptor);
  cOCIRowid = rb_define_class("OCIRowid", cOCIDescriptor);

  /* Exception */
  eOCIException = rb_define_class("OCIException", rb_eStandardError);
  eOCINoData = rb_define_class("OCINoData", eOCIException);
  eOCIError = rb_define_class("OCIError", eOCIException);
  eOCIInvalidHandle = rb_define_class("OCIInvalidHandle", eOCIException);
  eOCINeedData = rb_define_class("OCINeedData", eOCIException);
  eOCIStillExecuting = rb_define_class("OCIStillExecuting", eOCIException);
  eOCIContinue = rb_define_class("OCIContinue", eOCIException);
  eOCISuccessWithInfo = rb_define_class("OCISuccessWithInfo", eOCIError);

  /* oracle specific type */
  cOraDate = rb_define_class("OraDate", rb_cObject);
  cOraNumber = rb_define_class("OraNumber", rb_cObject);

  Init_oci8_const();
  Init_oci8_handle();
  Init_oci8_env();
  Init_oci8_error();
  Init_oci8_svcctx();
  Init_oci8_server();
  Init_oci8_session();
  Init_oci8_stmt();
  Init_oci8_define();
  Init_oci8_bind();

  Init_oci8_descriptor();
  Init_oci8_param();
  Init_oci8_lob();

  Init_ora_date();
  Init_ora_number();

#ifdef DEBUG_CORE_FILE
  signal(SIGSEGV, SIG_DFL);
#endif
}
