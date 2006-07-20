/*
  env.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIEnv
The environment handle is the source of all OCI objects.
Usually there is one instance per one application.

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIEnv|))
=end
*/
#include "oci8.h"
#include <util.h> /* ruby_setenv */

static VALUE oci8_env_s_initialize(int argc, VALUE *argv, VALUE klass)
{
  VALUE vmode;
  ub4 mode;
  sword rv;

#ifndef WIN32
  /* workaround code.
   *
   * Some instant clients set the environment variables
   * ORA_NLS_PROFILE33, ORA_NLS10 and ORACLE_HOME if they aren't
   * set. It causes problems on some platforms.
   */
  if (RTEST(rb_eval_string("RUBY_VERSION == \"1.8.4\""))) {
    if (getenv("ORA_NLS_PROFILE33") == NULL) {
      ruby_setenv("ORA_NLS_PROFILE33", "");
    }
    if (getenv("ORA_NLS10") == NULL) {
      ruby_setenv("ORA_NLS10", "");
    }
    if (getenv("ORACLE_HOME") == NULL) {
      ruby_setenv("ORACLE_HOME", ".");
    }
  }
#endif /* WIN32 */

  rb_scan_args(argc, argv, "01", &vmode);
  Get_Int_With_Default(argc, 1, vmode, mode, OCI_DEFAULT); /* 1 */
  
  rv = OCIInitialize(mode, NULL, NULL, NULL, NULL);
  return INT2FIX(rv);
}

static VALUE oci8_env_s_init(int argc, VALUE *argv, VALUE klass)
{
  VALUE vmode;
  ub4 mode;
  OCIEnv *envhp;
  OCIError *errhp;
  oci8_handle_t *h;
  sword rv;

  rb_scan_args(argc, argv, "01", &vmode);
  Get_Int_With_Default(argc, 1, vmode, mode, OCI_DEFAULT); /* 1 */
  
  rv = OCIEnvInit(&envhp, OCI_DEFAULT, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envhp, rv);
  rv = OCIHandleAlloc(envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envhp, rv);
  h = oci8_make_handle(OCI_HTYPE_ENV, envhp, errhp, NULL, 0);
  return h->self;
}

/*
=begin
--- OCIEnv.create([mode])
     create the environment handle. Don't call `new'.

     :mode
        ((|OCI_DEFAULT|)), ((|OCI_THREADED|)), ((|OCI_OBJECT|)), ((|OCI_SHARED|))
        or these combination.
        But I test only ((|OCI_DEFAULT|)). Default value is ((|OCI_DEFAULT|)).

        ((|OCI_THREADED|)): if your application use native thread(not ruby's thread), use this.

        ((|OCI_OBJECT|)): for OCI objects. But current implementation doesn't support OCI Object-Relational functions.
        This is for ((*far future*)) extension, because my plan doesn't include `OCI object support'.

        ((|OCI_SHARED|))(Oracle 8i or later): for ((<Shared Data Mode>)).

     :return value
        A instance of ((<OCIEnv>)).

     correspond native OCI function: ((|OCIEnvCreate|)) (if ((|OCIEnvCreate|)) is not found, the combination of ((|OCIInitialize|)) and ((|OCIEnvInit|)).)
=end
*/
#ifdef HAVE_OCIENVCREATE
static VALUE oci8_env_s_create(int argc, VALUE *argv, VALUE klass)
{
  VALUE vmode;
  ub4 mode;
  OCIEnv *envhp;
  OCIError *errhp;
  oci8_handle_t *h;
  sword rv;

  rb_scan_args(argc, argv, "01", &vmode);
  Get_Int_With_Default(argc, 1, vmode, mode, OCI_DEFAULT); /* 1 */
  
  rv = OCIEnvCreate(&envhp, mode, NULL, NULL, NULL, NULL, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envhp, rv);
  rv = OCIHandleAlloc(envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envhp, rv);
  h = oci8_make_handle(OCI_HTYPE_ENV, envhp, errhp, NULL, 0);
  return h->self;
}
#endif

/*
=begin
--- OCIEnv.terminate([mode])
     :mode
        ((|OCI_DEFAULT|)) only valid. Default value is ((|OCI__DEFAULT|)).

     correspond native OCI function: ((|OCITerminate|))
=end
*/
#ifdef HAVE_OCITERMINATE
static VALUE oci8_env_s_terminate(int argc, VALUE *argv, VALUE klass)
{
  VALUE vmode;
  ub4 mode;

  if (rb_scan_args(argc, argv, "01", &vmode) == 1) {
    Check_Type(vmode, T_FIXNUM);
    mode = FIX2INT(vmode);
  } else {
    mode = OCI_DEFAULT;
  }

  OCITerminate(mode);
  return Qnil;
}
#endif

/*
=begin
--- OCIEnv#alloc(handle)
     create a new OCI handle.
     :handle
        The valid values are ((<OCISvcCtx>)), ((<OCIStmt>)),
        ((<OCIServer>)), ((<OCISession>)), and ((<OCIDescribe>)).
     :return value
        A newly created handle.

     correspond native OCI function: ((|OCIHandleAlloc|))
=end
*/
typedef sword (*alloc_func_t)(CONST dvoid *, dvoid **, CONST ub4, CONST size_t, dvoid **);
static VALUE oci8_handle_alloc(VALUE self, VALUE klass)
{
  sword rv;
  dvoid *hp;
  oci8_handle_t *envh;
  oci8_handle_t *h;
  int i;
  struct {
    VALUE *klass;
    ub4 type;
    alloc_func_t alloc_func;
  } alloc_map[] = {
    {&cOCISvcCtx,   OCI_HTYPE_SVCCTX,   (alloc_func_t)OCIHandleAlloc},
    {&cOCIStmt,     OCI_HTYPE_STMT,     (alloc_func_t)OCIHandleAlloc},
    {&cOCIServer,   OCI_HTYPE_SERVER,   (alloc_func_t)OCIHandleAlloc},
    {&cOCISession,  OCI_HTYPE_SESSION,  (alloc_func_t)OCIHandleAlloc},
    {&cOCIDescribe, OCI_HTYPE_DESCRIBE, (alloc_func_t)OCIHandleAlloc},
    {&cOCILobLocator, OCI_DTYPE_LOB,    (alloc_func_t)OCIDescriptorAlloc},
  };
#define ALLOC_MAP_SIZE (sizeof(alloc_map) / sizeof(alloc_map[0]))

  Get_Handle(self, envh);
  for (i = 0;i < ALLOC_MAP_SIZE;i++) {
    if (alloc_map[i].klass[0] == klass)
      break;
  }
  if (i == ALLOC_MAP_SIZE) {
    rb_raise(rb_eTypeError, "not valid handle type %s", rb_class2name(CLASS_OF(klass)));
  }

  rv = alloc_map[i].alloc_func(envh->hp, &hp, alloc_map[i].type, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envh->hp, rv);
  h = oci8_make_handle(alloc_map[i].type, hp, envh->errhp, envh, 0);
  return h->self;
}

/*
=begin
--- OCIEnv#logon(username, password, dbname)
     Logon to the Oracle server.

     See ((<Simplified Logon>)) and ((<Explicit Attach and Begin Session>))

     :username
        the username.
     :password
        the user's password.
     :dbname
        the name of the database, or nil.
     :return value
        A instance of ((<OCISvcCtx>))
     For example:
       env = OCIEnv.create()
       svc = env.logon("SCOTT", "TIGER", nil)
         or
       svc = env.logon("SCOTT", "TIGER", "ORCL.WORLD")

     correspond native OCI function: ((|OCILogon|))
=end
*/
static VALUE oci8_logon(VALUE self, VALUE username, VALUE password, VALUE dbname)
{
  oci8_handle_t *envh;
  oci8_handle_t *svch;
  OCISvcCtx *svchp;
  oci8_string_t u, p, d;
  VALUE obj;
  sword rv;

  Get_Handle(self, envh);
  Get_String(username, u);
  Get_String(password, p);
  Get_String(dbname, d);
  rv = OCILogon(envh->hp, envh->errhp, &svchp,
		    u.ptr, u.len, p.ptr, p.len, d.ptr, d.len);
  if (rv != OCI_SUCCESS) {
    oci8_raise(envh->errhp, rv, NULL);
  }
  svch = oci8_make_handle(OCI_HTYPE_SVCCTX, svchp, envh->errhp, envh, 0);
  obj = svch->self;
  return obj;
}

void Init_oci8_env(void)
{
  rb_define_singleton_method(cOCIEnv, "initialise", oci8_env_s_initialize, -1);
  rb_define_singleton_method(cOCIEnv, "init", oci8_env_s_init, -1);
#ifdef HAVE_OCIENVCREATE
  rb_define_singleton_method(cOCIEnv, "create", oci8_env_s_create, -1);
#endif
#ifdef HAVE_OCITERMINATE
  rb_define_singleton_method(cOCIEnv, "terminate", oci8_env_s_terminate, -1);
#endif
  rb_define_method(cOCIEnv, "alloc", oci8_handle_alloc, 1);
  rb_define_method(cOCIEnv, "logon", oci8_logon, 3);
}
