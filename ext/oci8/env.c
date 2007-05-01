/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  env.c - part of ruby-oci8

  Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>

*/
#include "oci8.h"
#include <util.h> /* ruby_setenv */

OCIEnv *oci8_envhp;
OCIError *oci8_errhp;

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
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&oci8_errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
}
