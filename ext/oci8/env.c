/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * env.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2011 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"
#include <ruby/util.h>

ub4 oci8_env_mode = OCI_OBJECT | OCI_THREADED;

OCIEnv *oci8_global_envhp;

OCIEnv *oci8_make_envhp(void)
{
    sword rv;
    OCIEnv *envhp = NULL;

    rv = OCIEnvCreate(&envhp, oci8_env_mode, NULL, NULL, NULL, NULL, 0, NULL);
    if (rv != OCI_SUCCESS) {
        if (envhp != NULL) {
            oci8_env_free_and_raise(envhp, rv);
        } else {
            oci8_raise_init_error();
        }
    }
    oci8_global_envhp = envhp;
    return oci8_global_envhp;
}

/*
 * Setup thread-local oci8_errhp.
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

void Init_oci8_env(void)
{
    int error;

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
}
