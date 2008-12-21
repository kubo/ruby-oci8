/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  env.c - part of ruby-oci8

  Copyright (C) 2002-2008 KUBO Takehiro <kubo@jiubao.org>

*/
#include "oci8.h"

#if !defined(RUBY_VM)
/* ruby_setenv for workaround ruby 1.8.4 */
#include <util.h>
#endif

OCIEnv *oci8_envhp;
#ifdef RUBY_VM
/*
 * oci8_errhp is a thread local object in ruby 1.9.
 */

oci8_tls_key_t oci8_tls_key; /* native thread key */
static ID id_thread_key;     /* ruby's thread key */

static void oci8_free_errhp(OCIError *errhp)
{
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
}

OCIError *oci8_make_errhp(void)
{
    OCIError *errhp;
    VALUE obj;
    sword rv;

    /* create a new errhp. */
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    /* create a new ruby object which contains errhp to make
     * sure that the errhp is freed when it become unnecessary.
     */
    obj = Data_Wrap_Struct(rb_cObject, NULL, oci8_free_errhp, errhp);
    /* set the ruby object to ruby's thread local storage to prevent
     * it from being freed while the thread is available.
     */
    rb_thread_local_aset(rb_thread_current(), id_thread_key, obj);

    oci8_tls_set(oci8_tls_key, (void*)errhp);
    return errhp;
}
#else
/*
 * oci8_errhp is global in ruby 1.8.
 */
OCIError *oci8_errhp;
#endif

void Init_oci8_env(void)
{
    sword rv;
#ifdef RUBY_VM
    ub4 mode = OCI_OBJECT | OCI_THREADED;
    int error;
#else
    ub4 mode = OCI_OBJECT;
#endif

#if !defined(RUBY_VM) && !defined(_WIN32)
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
    rv = OCIInitialize(mode, NULL, NULL, NULL, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_raise_init_error();
    }
    rv = OCIEnvInit(&oci8_envhp, OCI_DEFAULT, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_raise_init_error();
    }
#ifdef RUBY_VM
    id_thread_key = rb_intern("__oci8_errhp__");
    error = oci8_tls_key_init(&oci8_tls_key);
    if (error != 0) {
        rb_raise(rb_eRuntimeError, "Cannot create thread local key (errno = %d)", error);
    }
#else /* RUBY_VM */
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&oci8_errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
#endif
}
