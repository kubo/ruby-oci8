/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2015 Kubo Takehiro <kubo@jiubao.org>
 */
#include <string.h>
#include <dlfcn.h>
#include "plthook.h"

static void *dlopen_hook(const char *path, int mode)
{
    if (strcmp(path, "libclntsh.dylib.11.1") == 0) {
        void *addr = dlsym(RTLD_DEFAULT, "OCIEnvCreate");
        if (addr != NULL) {
            Dl_info info;
            if (dladdr(addr, &info) != 0) {
                path = info.dli_fname;
            }
        }
    }
    return dlopen(path, mode);
}

void Init_oci8_osx(void)
{
    plthook_t *plthook;

    if (plthook_open(&plthook, "libclntsh.dylib.11.1") == 0) {
        plthook_replace(plthook, "dlopen", (void*)dlopen_hook, NULL);
        plthook_close(plthook);
    }
}
