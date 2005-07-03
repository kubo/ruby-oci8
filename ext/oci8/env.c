/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
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

OCIEnv *oci8_envhp;
OCIError *oci8_errhp;

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

void Init_oci8_env(void)
{
    sword rv;

#ifdef HAVE_OCIENVCREATE
    rv = OCIEnvCreate(&oci8_envhp, OCI_OBJECT, NULL, rb_oci8_maloc, rb_oci8_raloc, rb_oci8_mfree, 0, NULL);
#else
    rv = OCIInitialize(OCI_OBJECT, NULL, rb_oci8_maloc, rb_oci8_raloc, rb_oci8_mfree);
    if (rv != OCI_SUCCESS) {
        /* TODO: more proper error */
        oci8_env_raise(h->hp, rv);
    }
    rv = OCIEnvInit(&oci8_envhp, OCI_DEFAULT, 0, NULL);
#endif
    if (rv != OCI_SUCCESS) {
        /* TODO: more proper error */
        oci8_env_raise(oci8_envhp, rv);
    }
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&oci8_errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
}
