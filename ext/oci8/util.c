/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  util.c - part of ruby-oci8

  Copyright (C) 2015 Kubo Takehiro <kubo@jiubao.org>
*/
#if defined __linux && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#include "oci8.h"
#ifdef HAVE_DLADDR
#include <dlfcn.h>
#endif
#ifdef __CYGWIN__
#undef boolean
#include <windows.h>
#endif

const char *oci8_dll_path(void)
{
#if defined _WIN32 || defined __CYGWIN__
    HMODULE hMod = GetModuleHandleA("OCI.DLL");
    static char buf[MAX_PATH];
    if (hMod != NULL) {
        if (GetModuleFileName(hMod, buf, sizeof(buf))) {
            return buf;
        }
    }
#elif defined HAVE_DLADDR && defined RTLD_DEFAULT
    void *addr = dlsym(RTLD_DEFAULT, "OCIEnvCreate");
    Dl_info info;
    if (addr != NULL && dladdr(addr, &info)) {
        return info.dli_fname;
    }
#elif defined HAVE_DLMODINFO && defined HAVE_DLGETNAME && defined RTLD_DEFAULT
    void *addr = dlsym(RTLD_DEFAULT, "OCIEnvCreate");
    if (addr != NULL) {
        struct load_module_desc desc;
        if (dlmodinfo((uint64_t)addr, &desc, sizeof(desc), NULL, 0, 0) != 0) {
            return dlgetname(&desc, sizeof(desc), NULL, 0, 0);
        }
    }
#endif
    return NULL;
}

/*
 *  Returns the full path of Oracle client library used by the current process.
 *
 *  @return [String]
 */
static VALUE dll_path(VALUE module)
{
    const char *path = oci8_dll_path();
    if (path == NULL) {
        return Qnil;
    }
    return rb_external_str_new_with_enc(path, strlen(path), rb_filesystem_encoding());
}

void Init_oci8_util(VALUE cOCI8)
{
#if 0
    /* for yard */
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
#endif
    VALUE mUtil = rb_define_module_under(cOCI8, "Util");

    rb_define_module_function(mUtil, "dll_path", dll_path, 0);
}
