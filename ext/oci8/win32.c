/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  win32.c - part of ruby-oci8

  Copyright (C) 2009-2015 Kubo Takehiro <kubo@jiubao.org>
*/
#include "oci8.h"
#ifdef __CYGWIN__
/* boolean is defined as a macro in oratypes.h.
 * It conflicts with the definition in windows.h.
 */
#undef boolean
#endif
#include <windows.h>

/*
 *  Document-module: OCI8::Win32Util
 *
 *  Windows specific utility module.
 *
 *  @private
 */

NORETURN(static void raise_error(void));

static void raise_error(void)
{
    char msg[1024];
    int err = GetLastError();
    char *p;

    sprintf(msg, "%d: ", err);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  msg + strlen(msg), sizeof(msg) - strlen(msg), NULL);
    for (p = msg; *p != '\0'; p++) {
        if (*p == '\n' || *p == '\r') {
            *p = ' ';
        }
    }
    rb_raise(rb_eRuntimeError, "%s", msg);
}

typedef struct {
    HKEY hKey;
    HKEY hSubKey;
} enum_homes_arg_t;

static VALUE enum_homes_real(enum_homes_arg_t *arg)
{
    LONG rv;
    DWORD type;
    DWORD idx;
    char name[1024];
    DWORD name_len;
    FILETIME ft;

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\ORACLE", 0, KEY_ENUMERATE_SUB_KEYS, &arg->hKey);
    if (rv != ERROR_SUCCESS) {
        return Qnil;
    }
    for (idx = 0; ; idx++) {
        volatile VALUE oracle_home;
        volatile VALUE nls_lang;

        /* Get subkey name */
        name_len = sizeof(name);
        rv = RegEnumKeyEx(arg->hKey, idx, name, &name_len, NULL, NULL, NULL, &ft);
        if (rv != ERROR_SUCCESS) {
            break;
        }
        /* Open subkey */
        if (arg->hSubKey != NULL) {
            RegCloseKey(arg->hSubKey);
            arg->hSubKey = NULL;
        }
        rv = RegOpenKeyEx(arg->hKey, name, 0, KEY_QUERY_VALUE, &arg->hSubKey);
        if (rv != ERROR_SUCCESS) {
            continue;
        }
        /* Get ORACLE_HOME */
        name_len = sizeof(name);
        rv = RegQueryValueEx(arg->hSubKey, "ORACLE_HOME", NULL, &type, TO_ORATEXT(name), &name_len);
        if (rv != ERROR_SUCCESS || type != REG_SZ) {
            continue;
        }
        oracle_home = rb_locale_str_new_cstr(name);
        /* Get NLS_LANG */
        name_len = sizeof(name);
        rv = RegQueryValueEx(arg->hSubKey, "NLS_LANG", NULL, &type, TO_ORATEXT(name), &name_len);
        if (rv != ERROR_SUCCESS || type != REG_SZ) {
            continue;
        }
        nls_lang = rb_locale_str_new_cstr(name);
        rb_yield_values(2, oracle_home, nls_lang);
    }
    return Qnil;
}

static VALUE enum_homes_ensure(enum_homes_arg_t *arg)
{
    if (arg->hKey != NULL) {
        RegCloseKey(arg->hKey);
        arg->hKey = NULL;
    }
    if (arg->hSubKey != NULL) {
        RegCloseKey(arg->hSubKey);
        arg->hSubKey = NULL;
    }
    return Qnil;
}

/*
 *  Enumerates full clients' Oracle homes and NLS_LANG parameters
 *  registerd in the Windows registry.
 *
 *  @yield [oracle_home, nls_lang]
 *  @yieldparam [String] oracle_home
 *  @yieldparam [String] nls_lang
 *  @return [nil]
 */
static VALUE enum_homes(VALUE module)
{
    enum_homes_arg_t arg;
    arg.hKey = NULL;
    arg.hSubKey = NULL;
    return rb_ensure(enum_homes_real, (VALUE)&arg, enum_homes_ensure, (VALUE)&arg);
}

void Init_oci8_win32(VALUE cOCI8)
{
    VALUE mWin32Util = rb_define_module_under(cOCI8, "Win32Util");

    rb_define_module_function(mWin32Util, "enum_homes", enum_homes, 0);
}
