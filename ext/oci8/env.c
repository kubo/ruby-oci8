/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  env.c - part of ruby-oci8

  Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>

*/
#include "oci8.h"

/* ruby_setenv */
#ifdef RUBY_VM
#include <ruby/util.h>
#else
#include <util.h>
#endif

OCIEnv *oci8_envhp;
#ifdef RUBY_VM
static ID id_thread_key;
static void oci8_free_errhp(OCIError *errhp)
{
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
}

OCIError *oci8_get_errhp(void)
{
    static OCIError *errhp = NULL;
    static VALUE my_thread = Qnil;
    VALUE obj;

    if (rb_thread_current() == my_thread) {
        /* tricky!
         * This code works only when ruby thread model is 1 or 2.
         *   model 1: ruby 1.8 or earlier.
         *   model 2: ruby 1.9
         */
        return errhp;
    }
    my_thread = rb_thread_current();
    obj = rb_thread_local_aref(my_thread, id_thread_key);
    if (!NIL_P(obj)) {
        Data_Get_Struct(obj, OCIError, errhp);
        if (errhp == NULL) {
            rb_bug("oci8_get_errhp");
        }
    } else {
        sword rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_env_raise(oci8_envhp, rv);
        }
        obj = Data_Wrap_Struct(rb_cObject, NULL, oci8_free_errhp, errhp);
        rb_thread_local_aset(my_thread, id_thread_key, obj);
    }
    return errhp;
}
#else
OCIError *oci8_errhp;
#endif

void Init_oci8_env(void)
{
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
    rv = OCIInitialize(OCI_OBJECT, NULL, NULL, NULL, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_raise_init_error();
    }
    rv = OCIEnvInit(&oci8_envhp, OCI_DEFAULT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_raise_init_error();
    }
#ifdef RUBY_VM
    id_thread_key = rb_intern("__oci8_errhp__");
#else
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&oci8_errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
#endif
}
