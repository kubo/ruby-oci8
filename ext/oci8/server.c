/*
  server.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIServer
If you use ((<OCIEnv#logon>)), you have no need to use this handle directly.
Because ((<OCIEnv#logon>)) create this handle implicitly and set it to ((<OCISvcCtx>)).

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIServer|))
=end
*/
#include "oci8.h"

/*
=begin
--- OCIServer#attach(dbname [, mode])
     attach to the database.
     :dbname
        the name of database.
     :mode
        ((|OCI_DEFAULT|)) or ((|OCI_CPOOL|))(Oracle 9i). Default value is ((|OCI_DEFAULT|)).

        This ruby module doesn't support the connection pooling provided by OCI,
        so ((|OCI_CPOOL|)) is invalid value for now.

     correspond native OCI function: ((|OCIServerAttach|))
=end
*/
static VALUE oci8_server_attach(int argc, VALUE *argv, VALUE self)
{
  VALUE vdbname, vmode;
  oci8_handle_t *h;
  oci8_string_t d;
  ub4 mode;
  sword rv;

  rb_scan_args(argc, argv, "11", &vdbname, &vmode);
  Get_Handle(self, h); /* 0 */
  Get_String(vdbname, d); /* 1 */
  Get_Int_With_Default(argc, 2, vmode, mode, OCI_DEFAULT); /* 2 */

  rv = OCIServerAttach(h->hp, h->errhp, d.ptr, d.len, mode);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

/*
=begin
--- OCIServer#detach([mode])
     detach from the database.

     :mode
        ((|OCI_DEFAULT|)) only valid. Default value is ((|OCI_DEFAULT|)).

     correspond native OCI function: ((|OCIServerDetach|))
=end
*/
static VALUE oci8_server_detach(int argc, VALUE *argv, VALUE self)
{
  VALUE vmode;
  oci8_handle_t *h;
  ub4 mode;
  sword rv;


  rb_scan_args(argc, argv, "01", &vmode);
  Get_Handle(self, h); /* 0 */
  Get_Int_With_Default(argc, 1, vmode, mode, OCI_DEFAULT); /* 1 */

  rv = OCIServerDetach(h->hp, h->errhp, mode);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}



void Init_oci8_server(void)
{
  rb_define_method(cOCIServer, "attach", oci8_server_attach, -1);
  rb_define_method(cOCIServer, "detach", oci8_server_detach, -1);
#ifdef HAVE_OCISERVERRELEASE
  rb_define_method(cOCIServer, "release", oci8_server_release, 0);
#endif
}

/*
=begin
--- OCIServer#version()
     get server version.

     :return value
        string of server version. For example
          Oracle8 Release 8.0.5.0.0 - Production
          PL/SQL Release 8.0.5.0.0 - Production

     correspond native OCI function: ((|OCIServerVersion|))
=end
*/
VALUE oci8_server_version(VALUE self)
{
  oci8_handle_t *h;
  OraText buf[1024];
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIServerVersion(h->hp, h->errhp, buf, sizeof(buf), h->type);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return rb_str_new2(buf);
}

/*
=begin
--- OCIServer#release()
     get server version number and string

     :return value
        array of number and string. For example

          version_number, version_str = srv.release()
          version_number is 0x8005000.
          version_str is
            Oracle8 Release 8.0.5.0.0 - Production
            PL/SQL Release 8.0.5.0.0 - Production

     correspond native OCI function: ((|OCIServerVersion|))

     Oracle 9i or later?
=end
*/
#ifdef HAVE_OCISERVERRELEASE
VALUE oci8_server_release(VALUE self)
{
  oci8_handle_t *h;
  OraText buf[1024];
  ub4 version = 0;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIServerRelease(h->hp, h->errhp, buf, sizeof(buf), h->type, &version);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return rb_ary_new3(2, INT2FIX(version), rb_str_new2(buf));
}
#endif

VALUE oci8_break(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIBreak(h->hp, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

#ifdef HAVE_OCIRESET
VALUE oci8_reset(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h); /* 0 */
  rv = OCIReset(h->hp, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}
#endif
