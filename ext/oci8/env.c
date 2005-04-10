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

static dvoid *rb_oci8_maloc(dvoid *ctxp, size_t size)
{
  return ruby_xmalloc(size);
}

static dvoid *rb_oci8_raloc(dvoid *ctxp, dvoid *memptr, size_t newsize)
{
  return ruby_xrealloc(memptr, newsize);
}

static void rb_oci8_mfree(dvoid *ctxp, dvoid *memptr)
{
  ruby_xfree(memptr);
}

static VALUE oci8_env_initialize(VALUE self)
{
  oci8_handle_t *h;
  sword rv;

  Get_Handle(self, h);
#ifdef HAVE_OCIENVCREATE
  rv = OCIEnvCreate((OCIEnv **)&h->hp, OCI_OBJECT, NULL, rb_oci8_maloc, rb_oci8_raloc, rb_oci8_mfree, 0, NULL);
#else
  rv = OCIEnvInit((OCIEnv **)&h->hp, OCI_DEFAULT, 0, NULL);
#endif
  if (rv != OCI_SUCCESS)
    oci8_env_raise(h->hp, rv);
  h->type = OCI_HTYPE_ENV;
  rv = OCIHandleAlloc(h->hp, (dvoid *)&h->errhp, OCI_HTYPE_ERROR, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(h->hp, rv);
  h->envh = h;
  return Qnil;
}

void Init_oci8_env(void)
{
#ifndef HAVE_OCIENVCREATE
  OCIInitialize(OCI_OBJECT, NULL, rb_oci8_maloc, rb_oci8_raloc, rb_oci8_mfree);
#endif
  rb_define_method(cOCIEnv, "initialize", oci8_env_initialize, 0);
}
