/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * env.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2011 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

#if defined(HAVE_RUBY_UTIL_H)
#include <ruby/util.h>
#elif defined(HAVE_UTIL_H)
#include <util.h>
#endif

#ifdef HAVE_TYPE_RB_BLOCKING_FUNCTION_T
ub4 oci8_env_mode = OCI_OBJECT | OCI_THREADED;
#else
ub4 oci8_env_mode = OCI_OBJECT;
#endif

OCIEnv *oci8_global_envhp;

OCIEnv *oci8_make_envhp(void)
{
    sword rv;

    rv = OCIEnvCreate(&oci8_global_envhp, oci8_env_mode, NULL, NULL, NULL, NULL, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_raise_init_error();
    }
    return oci8_global_envhp;
}

#ifdef HAVE_TYPE_RB_BLOCKING_FUNCTION_T
/*
 * oci8_errhp is a thread local object in ruby 1.9.
 */

oci8_tls_key_t oci8_tls_key; /* native thread key */

/* This function is called on the native thread termination
 * if the thread local errhp is not null.
 */
static void oci8_free_errhp(void *errhp)
{
    OCIHandleFree(errhp, OCI_HTYPE_ERROR);
}

#ifdef _WIN32
static int dllmain_is_called;

__declspec(dllexport)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    void *errhp;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        dllmain_is_called = 1;
        break;
    case DLL_THREAD_ATTACH:
        /* do nothing */
        break;
    case DLL_THREAD_DETACH:
        errhp = oci8_tls_get(oci8_tls_key);
        if (errhp != NULL) {
            oci8_free_errhp(errhp);
        }
        break;
    case DLL_PROCESS_DETACH:
        /* do nothing */
        break;
    }
    return TRUE;
}
#endif

OCIError *oci8_make_errhp(void)
{
    OCIError *errhp;
    sword rv;

    /* create a new errhp. */
    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    /* Set the errhp to the thread local storage.
     * It is freed by oci8_free_errhp().
     */
    oci8_tls_set(oci8_tls_key, (void*)errhp);
    return errhp;
}
#else
/*
 * oci8_errhp is global in ruby 1.8.
 */
OCIError *oci8_global_errhp;

OCIError *oci8_make_errhp(void)
{
    sword rv;

    rv = OCIHandleAlloc(oci8_envhp, (dvoid *)&oci8_global_errhp, OCI_HTYPE_ERROR, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    return oci8_global_errhp;
}
#endif

void Init_oci8_env(void)
{
#ifdef HAVE_TYPE_RB_BLOCKING_FUNCTION_T
    int error;
#endif

#if !defined(HAVE_TYPE_RB_BLOCKING_FUNCTION_T) && !defined(_WIN32)
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

    /* workaround code.
     *
     * When ORACLE_HOME ends with '/' and the Oracle client is
     * an instant client lower than 10.2.0.3, OCIEvnCreate()
     * doesn't work even though the combination of OCIInitialize()
     * and OCIEnvInit() works fine. Delete the last slash for
     * a workaround.
     */
    if (oracle_client_version < ORAVERNUM(10, 2, 0, 3, 0)) {
#ifdef _WIN32
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif
        char *home = getenv("ORACLE_HOME");
        if (home != NULL) {
            size_t homelen = strlen(home);
            if (homelen > 0 && home[homelen - 1] == DIR_SEP) {
                home = ruby_strdup(home);
                home[homelen - 1] = '\0';
                ruby_setenv("ORACLE_HOME", home);
                xfree(home);
            }
        }
    }

#ifdef HAVE_TYPE_RB_BLOCKING_FUNCTION_T
/* ruby 1.9 */
#if defined(_WIN32)
    if (!dllmain_is_called) {
        /* sanity check */
        rb_raise(rb_eRuntimeError, "DllMain is not unexpectedly called. This causes resource leaks.");
    }
    oci8_tls_key = TlsAlloc();
    if (oci8_tls_key == 0xFFFFFFFF) {
        error = GetLastError();
    } else {
        error = 0;
    }
#else
    error = pthread_key_create(&oci8_tls_key, oci8_free_errhp);
#endif
    if (error != 0) {
        rb_raise(rb_eRuntimeError, "Cannot create thread local key (errno = %d)", error);
    }
#endif
}
