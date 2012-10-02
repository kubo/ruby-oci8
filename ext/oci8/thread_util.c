/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * thread_util.c - part of ruby-oci8
 *
 * Copyright (C) 2011 KUBO Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"
#include <errno.h>

#ifdef USE_THREAD_LOCAL_ERRHP

#ifndef WIN32
#include <pthread.h>
static pthread_attr_t detached_thread_attr;
#endif

typedef struct {
    void *(*func)(void *);
    void *arg;
} adapter_arg_t;

void Init_oci8_thread_util(void)
{
#ifndef WIN32
    pthread_attr_init(&detached_thread_attr);
    pthread_attr_setdetachstate(&detached_thread_attr, PTHREAD_CREATE_DETACHED);
#endif
}

#ifdef WIN32

static void __cdecl adapter(void *arg)
{
    adapter_arg_t *aa = (adapter_arg_t *)arg;
    aa->func(aa->arg);
    free(aa);
}

int oci8_run_native_thread(void *(*func)(void*), void *arg)
{
    adapter_arg_t *aa = malloc(sizeof(adapter_arg_t));
    if (aa == NULL) {
        return ENOMEM;
    }

    aa->func = func;
    aa->arg = arg;
    if (_beginthread(adapter, 0, aa) == (uintptr_t)-1L) {
        int err = errno;
        free(aa);
        return err;
    }
    return 0;
}

#else /* WIN32 */

static void *adapter(void *arg)
{
    adapter_arg_t *aa = (adapter_arg_t *)arg;
    aa->func(aa->arg);
    free(aa);
    return NULL;
}

int oci8_run_native_thread(void *(*func)(void *), void *arg)
{
    pthread_t thread;
    adapter_arg_t *aa = malloc(sizeof(adapter_arg_t));
    int rv;
    if (aa == NULL) {
        return ENOMEM;
    }

    aa->func = func;
    aa->arg = arg;
    rv = pthread_create(&thread, &detached_thread_attr, adapter, aa);
    if (rv != 0) {
        free(aa);
    }
    return rv;
}
#endif /* WIN32 */

#endif /* USE_THREAD_LOCAL_ERRHP */
