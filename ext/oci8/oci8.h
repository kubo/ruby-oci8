/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.h - part of ruby-oci8
 *
 * Copyright (C) 2002-2016 Kubo Takehiro <kubo@jiubao.org>
 */
#ifndef _RUBY_OCI_H_
#define _RUBY_OCI_H_ 1

#include "ruby.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <oci.h>
#ifdef __cplusplus
}
#endif

/*
 * Oracle version number format in 32-bit integer.
 *
 * Oracle 12c or earier                     Oracle 18c or later
 *
 * hexadecimal ->  dotted version number    hexadecimal ->  dotted version number
 *  0c102304   ->  12.1.2.3.4                12012034   ->  18.1.2.3.4
 *                 ^  ^ ^ ^ ^                               ^  ^ ^ ^ ^
 *  0c ------------'  | | | |  8 bits        12 ------------'  | | | |  8 bits
 *    1 --------------' | | |  4 bits          01 -------------' | | |  8 bits
 *     02 --------------' | |  8 bits            2 --------------' | |  4 bits
 *       3 ---------------' |  4 bits             03 --------------' |  8 bits
 *        04 ---------------'  8 bits               4 ---------------'  4 bits
 *                      total 32 bits                            total 32 bits
 */
#define ORAVERNUM(major, minor, update, patch, port_update) \
    (((major) >= 18) ? (((major) << 24) | ((minor) << 16) | ((update) << 12) | ((patch) << 4) | (port_update)) \
                     : (((major) << 24) | ((minor) << 20) | ((update) << 12) | ((patch) << 8) | (port_update)))

#define ORAVER_8_0 ORAVERNUM(8, 0, 0, 0, 0)
#define ORAVER_8_1 ORAVERNUM(8, 1, 0, 0, 0)
#define ORAVER_9_0 ORAVERNUM(9, 0, 0, 0, 0)
#define ORAVER_9_2 ORAVERNUM(9, 2, 0, 0, 0)
#define ORAVER_10_1 ORAVERNUM(10, 1, 0, 0, 0)
#define ORAVER_10_2 ORAVERNUM(10, 2, 0, 0, 0)
#define ORAVER_11_1 ORAVERNUM(11, 1, 0, 0, 0)
#define ORAVER_12_1 ORAVERNUM(12, 1, 0, 0, 0)
#define ORAVER_18 ORAVERNUM(18, 0, 0, 0, 0)

#include "extconf.h"
#include <ruby/encoding.h>

#ifndef OCI_TEMP_CLOB
#define OCI_TEMP_CLOB 1
#endif
#ifndef OCI_TEMP_BLOB
#define OCI_TEMP_BLOB 2
#endif

#ifndef ORAXB8_DEFINED
#if SIZEOF_LONG == 8
typedef unsigned long oraub8;
typedef   signed long orasb8;
#elif SIZEOF_LONG_LONG == 8
typedef unsigned long long oraub8;
typedef   signed long long orasb8;
#elif SIZEOF___INT64 == 8
typedef unsigned __int64 oraub8;
typedef   signed __int64 orasb8;
#endif
typedef oraub8 ub8;
typedef orasb8 sb8;
#endif /* ORAXB8_DEFINED */

#if defined RBX_CAPI_RUBY_H
/* rubinius 2.0 */
#ifndef HAVE_RB_ENC_STR_BUF_CAT
#define rb_enc_str_buf_cat(str, ptr, len, enc) \
    rb_str_concat((str), rb_enc_str_new((ptr), (len), (enc)))
#endif
#ifndef HAVE_RB_STR_BUF_CAT_ASCII
#define rb_str_buf_cat_ascii(str, ptr) \
    rb_str_concat((str), rb_usascii_str_new_cstr(ptr))
#endif
#ifndef STRINGIZE
#define STRINGIZE(name) #name
#endif
#endif

/* macros depends on the compiler.
 *  LIKELY(x)      hint for the compiler that 'x' is 1(TRUE) in many cases.
 *  UNLIKELY(x)    hint for the compiler that 'x' is 0(FALSE) in many cases.
 */
#if defined(__GNUC__) && ((__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
/* gcc version >= 2.96 */
#define LIKELY(x)   (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
/* other compilers */
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

/* macros to access thread-local storage.
 *
 *  void *oci8_tls_get(oci8_tls_key_t key);
 *    get a value associated with the key.
 *
 *  void oci8_tls_set(oci8_tls_key_t key, void *value);
 *    set a value to the key.
 *
 */
#if defined(_WIN32)
#include <windows.h>
#define oci8_tls_key_t           DWORD
#define oci8_tls_get(key)        TlsGetValue(key)
#define oci8_tls_set(key, val)   TlsSetValue((key), (val))
#else
#include <pthread.h>
#define oci8_tls_key_t           pthread_key_t
#define oci8_tls_get(key)        pthread_getspecific(key)
#define oci8_tls_set(key, val)   pthread_setspecific((key), (val))
#endif

/* utility macros
 */
#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))
#define TO_ORATEXT to_oratext
#define TO_CONST_ORATEXT to_const_oratext
#define TO_CHARPTR to_charptr
ALWAYS_INLINE(static OraText *to_oratext(char *c));
static inline OraText *to_oratext(char *c)
{
    return (OraText*)c;
}
ALWAYS_INLINE(static const OraText *to_const_oratext(const char *c));
static inline const OraText *to_const_oratext(const char *c)
{
    return (const OraText*)c;
}
ALWAYS_INLINE(static char *to_charptr(OraText *c));
static inline char *to_charptr(OraText *c)
{
    return (char*)c;
}
#define RSTRING_ORATEXT(obj) TO_ORATEXT(RSTRING_PTR(obj))
#define rb_str_new2_ora(str) rb_str_new2(TO_CHARPTR(str))

#define TO_BIND(obj) ((oci8_bind_t *)oci8_check_typeddata((obj), &oci8_bind_data_type, 1))

/*
 * prevent rdoc from gathering the specified method.
 */
#define rb_define_method_nodoc rb_define_method
#define rb_define_singleton_method_nodoc rb_define_singleton_method

/* data structure for SQLT_LVC and SQLT_LVB. */
typedef struct {
    sb4 size;
    char buf[1];
} oci8_vstr_t;

typedef struct oci8_handle_data_type oci8_handle_data_type_t;
typedef struct oci8_bind_data_type oci8_bind_data_type_t;

typedef struct oci8_base oci8_base_t;
typedef struct oci8_bind oci8_bind_t;

/* The virtual method table of oci8_base_t */
struct oci8_handle_data_type {
    rb_data_type_t rb_data_type;
    void (*free)(oci8_base_t *base);
    size_t size;
};

/* The virtual method table of oci8_bind_t */
struct oci8_bind_data_type {
    oci8_handle_data_type_t base;
    VALUE (*get)(oci8_bind_t *obind, void *data, void *null_struct);
    void (*set)(oci8_bind_t *obind, void *data, void **null_structp, VALUE val);
    void (*init)(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length);
    void (*init_elem)(oci8_bind_t *obind, VALUE svc);
    void (*pre_fetch_hook)(oci8_bind_t *obind, VALUE svc);
    ub2 dty;
    void (*post_bind_hook)(oci8_bind_t *obind);
};

/* Class structure implemented by C language.
 * oci8_base_t represents OCIHandle and its subclasses.
 *
 */
struct oci8_base {
    const oci8_handle_data_type_t *data_type;
    ub4 type;
    ub1 closed;
    union {
        dvoid *ptr;
        OCISvcCtx *svc;
        OCICPool *poolhp;
        OCIServer *srvhp;
        OCISession *usrhp;
        OCIStmt *stmt;
        OCIDefine *dfn;
        OCIBind *bnd;
        OCIParam *prm;
        OCILobLocator *lob;
        OCIType *tdo;
        OCIDescribe *dschp;
    } hp;
    VALUE self;
    oci8_base_t *parent;
    oci8_base_t *next;
    oci8_base_t *prev;
    oci8_base_t *children;
};

/* Class structure implemented by C language.
 * This represents OCI8::BindType::Base's subclasses.
 *
 * This is a subclass of oci8_base_t because the first member
 * base is oci8_base_t itself.
 */
struct oci8_bind {
    oci8_base_t base;
    void *valuep;
    sb4 value_sz; /* size to define or bind. */
    sb4 alloc_sz; /* size of a element. */
    ub4 maxar_sz; /* maximum array size. */
    ub4 curar_sz; /* current array size. */
    ub4 curar_idx;/* current array index. */
    VALUE tdo;
    union {
        void **null_structs;
        sb2 *inds;
    } u;
};

typedef struct oci8_logoff_strategy oci8_logoff_strategy_t;

typedef struct oci8_temp_lob {
    struct oci8_temp_lob *next;
    OCILobLocator *lob;
} oci8_temp_lob_t;

typedef struct oci8_svcctx {
    oci8_base_t base;
    volatile VALUE executing_thread;
    const oci8_logoff_strategy_t *logoff_strategy;
    OCISession *usrhp;
    OCIServer *srvhp;
    ub4 server_version;
    rb_pid_t pid;
    unsigned char state;
    char is_autocommit;
    char suppress_free_temp_lobs;
    char non_blocking;
    VALUE long_read_len;
    oci8_temp_lob_t *temp_lobs;
} oci8_svcctx_t;

struct oci8_logoff_strategy {
    void *(*prepare)(oci8_svcctx_t *svcctx);
    void *(*execute)(void *);
};

typedef struct {
    dvoid *hp; /* OCIBind* or OCIDefine* */
    dvoid *valuep;
    sb4 value_sz;
    ub2 dty;
    sb2 *indp;
    ub2 *alenp;
} oci8_exec_sql_var_t;

#define oci8_raise(err, status, stmt) oci8_do_raise(err, status, stmt, __FILE__, __LINE__)
#define oci8_env_raise(env, status) oci8_do_env_raise(env, status, 0, __FILE__, __LINE__)
#define oci8_env_free_and_raise(env, status) oci8_do_env_raise(env, status, 1, __FILE__, __LINE__)
#define oci8_raise_init_error() oci8_do_raise_init_error(__FILE__, __LINE__)
#define oci8_raise_by_msgno(msgno, default_msg) oci8_do_raise_by_msgno(msgno, default_msg, __FILE__, __LINE__)

#define chkerr(status) oci8_check_error_((status), NULL, NULL, __FILE__, __LINE__)
#define chker2(status, base) oci8_check_error_((status), (base), NULL, __FILE__, __LINE__)
#define chker3(status, base, stmt) oci8_check_error_((status), (base), (stmt), __FILE__, __LINE__)

/* The folloiwng macros oci8_envhp and oci8_errhp are used
 * as if they are defined as follows:
 *
 *   extern OCIEnv *oci8_envhp;
 *   extern OCIError *oci8_errhp;
 */
#define oci8_envhp (LIKELY(oci8_global_envhp != NULL) ? oci8_global_envhp : oci8_make_envhp())
#define oci8_errhp oci8_get_errhp()

/* env.c */
extern ub4 oci8_env_mode;
extern OCIEnv *oci8_global_envhp;
OCIEnv *oci8_make_envhp(void);
extern oci8_tls_key_t oci8_tls_key; /* native thread key */
OCIError *oci8_make_errhp(void);

static inline OCIError *oci8_get_errhp(void)
{
    OCIError *errhp = (OCIError *)oci8_tls_get(oci8_tls_key);
    return LIKELY(errhp != NULL) ? errhp : oci8_make_errhp();
}

void Init_oci8_env(void);

/* oci8lib.c */
extern ID oci8_id_at_last_error;
extern ID oci8_id_get;
extern ID oci8_id_set;
#ifdef CHAR_IS_NOT_A_SHORTCUT_TO_ID
extern ID oci8_id_add_op; /* ID of the addition operator '+' */
extern ID oci8_id_sub_op; /* ID of the subtraction operator '-' */
extern ID oci8_id_mul_op; /* ID of the multiplication operator '*' */
extern ID oci8_id_div_op; /* ID of the division operator '/' */
#else
#define oci8_id_add_op '+'
#define oci8_id_sub_op '-'
#define oci8_id_mul_op '*'
#define oci8_id_div_op '/'
#endif
extern int oci8_in_finalizer;
extern VALUE oci8_cOCIHandle;
void oci8_base_free(oci8_base_t *base);
VALUE oci8_define_class(const char *name, const oci8_handle_data_type_t *data_type, VALUE (*alloc_func)(VALUE));
VALUE oci8_define_class_under(VALUE outer, const char *name, const oci8_handle_data_type_t *data_type, VALUE (*alloc_func)(VALUE));
VALUE oci8_define_bind_class(const char *name, const oci8_bind_data_type_t *data_type, VALUE (*alloc_func)(VALUE));
void oci8_link_to_parent(oci8_base_t *base, oci8_base_t *parent);
void oci8_unlink_from_parent(oci8_base_t *base);
sword oci8_call_without_gvl(oci8_svcctx_t *svcctx, void *(*func)(void *), void *data);
sword oci8_exec_sql(oci8_svcctx_t *svcctx, const char *sql_text, ub4 num_define_vars, oci8_exec_sql_var_t *define_vars, ub4 num_bind_vars, oci8_exec_sql_var_t *bind_vars, int raise_on_error);
#if defined RUNTIME_API_CHECK
void *oci8_find_symbol(const char *symbol_name);
#endif
void *oci8_check_typeddata(VALUE obj, const oci8_handle_data_type_t *data_type, int error_if_closed);

/* error.c */
extern VALUE eOCIException;
extern VALUE eOCIBreak;
void Init_oci8_error(void);
NORETURN(void oci8_do_raise(OCIError *errhp, sword status, OCIStmt *stmthp, const char *file, int line));
NORETURN(void oci8_do_env_raise(OCIEnv *envhp, sword status, int free_envhp, const char *file, int line));
NORETURN(void oci8_do_raise_init_error(const char *file, int line));
sb4 oci8_get_error_code(OCIError *errhp);
VALUE oci8_get_error_message(ub4 msgno, const char *default_msg);
NORETURN(void oci8_do_raise_by_msgno(ub4 msgno, const char *default_msg, const char *file, int line));
void oci8_check_error_(sword status, oci8_base_t *base, OCIStmt *stmthp, const char *file, int line);

/* ocihandle.c */
extern const oci8_handle_data_type_t oci8_handle_data_type;
void Init_oci8_handle(void);
VALUE oci8_allocate_typeddata(VALUE klass, const oci8_handle_data_type_t *data_type);
void oci8_handle_cleanup(void *ptr);
size_t oci8_handle_size(const void *ptr);

/* oci8.c */
void Init_oci8(VALUE *out);
void oci8_do_parse_connect_string(VALUE conn_str, VALUE *user, VALUE *pass, VALUE *dbname, VALUE *mode);
oci8_svcctx_t *oci8_get_svcctx(VALUE obj);
OCISession *oci8_get_oci_session(VALUE obj);
void oci8_check_pid_consistency(oci8_svcctx_t *svcctx);
#define TO_SESSION oci8_get_oci_session

/* connection_pool.c */
void Init_oci8_connection_pool(VALUE cOCI8);

/* stmt.c */
void Init_oci8_stmt(VALUE cOCI8);

/* bind.c */
typedef struct {
    void *hp;
    VALUE obj;
} oci8_hp_obj_t;
extern const oci8_handle_data_type_t oci8_bind_data_type;
void oci8_bind_free(oci8_base_t *base);
void oci8_bind_hp_obj_mark(oci8_base_t *base);
void Init_oci8_bind(VALUE cOCI8BindTypeBase);

/* metadata.c */
extern const oci8_handle_data_type_t oci8_metadata_base_data_type;
VALUE oci8_do_describe(VALUE self, void *objptr, ub4 objlen, ub1 objtype, VALUE klass, VALUE check_public);
void Init_oci8_metadata(VALUE cOCI8);
VALUE oci8_metadata_create(OCIParam *parmhp, VALUE svc, VALUE parent);

/* lob.c */
void Init_oci8_lob(VALUE cOCI8);
VALUE oci8_make_clob(oci8_svcctx_t *svcctx, OCILobLocator *s);
VALUE oci8_make_nclob(oci8_svcctx_t *svcctx, OCILobLocator *s);
VALUE oci8_make_blob(oci8_svcctx_t *svcctx, OCILobLocator *s);
VALUE oci8_make_bfile(oci8_svcctx_t *svcctx, OCILobLocator *s);
void oci8_assign_clob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest);
void oci8_assign_nclob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest);
void oci8_assign_blob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest);
void oci8_assign_bfile(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest);

/* oradate.c */
void Init_ora_date(void);

/* ocinumber.c */
extern int oci8_float_conversion_type_is_ruby;
void Init_oci_number(VALUE mOCI, OCIError *errhp);
/* OraNumber (ruby object) -> OCINumber (C datatype) */
OCINumber *oci8_get_ocinumber(VALUE num);
/* OCINumber (C datatype) -> OraNumber (ruby object) */
VALUE oci8_make_ocinumber(OCINumber *s, OCIError *errhp);
/* OCINumber (C datatype) -> Integer (ruby object) */
VALUE oci8_make_integer(OCINumber *s, OCIError *errhp);
/* OCINumber (C datatype) -> Float (ruby object) */
VALUE oci8_make_float(OCINumber *s, OCIError *errhp);
/* Numeric (ruby object) -> OCINumber (C datatype) */
OCINumber *oci8_set_ocinumber(OCINumber *result, VALUE self, OCIError *errhp);
/* Numeric (ruby object) -> OCINumber (C datatype) as an integer */
OCINumber *oci8_set_integer(OCINumber *result, VALUE self, OCIError *errhp);
/* OCINumber (C datatype) -> double (C datatype) */
double oci8_onum_to_dbl(OCINumber *s, OCIError *errhp);
/* double (C datatype) -> OCINumber (C datatype) */
OCINumber *oci8_dbl_to_onum(OCINumber *result, double dbl, OCIError *errhp);

/* ocidatetim.c */
void Init_oci_datetime(void);
VALUE oci8_make_ocidate(OCIDate *od);
OCIDate *oci8_set_ocidate(OCIDate *od, VALUE val);
VALUE oci8_make_ocitimestamp(OCIDateTime *dttm, boolean have_tz);
OCIDateTime *oci8_set_ocitimestamp_tz(OCIDateTime *dttm, VALUE val, VALUE svc);
VALUE oci8_make_interval_ym(OCIInterval *s);
VALUE oci8_make_interval_ds(OCIInterval *s);

/* object.c */
void Init_oci_object(VALUE mOCI);

/* transaction.c */
void Init_oci8_transaction(VALUE cOCI8);
extern const oci8_handle_data_type_t oci8_trans_data_type;

/* attr.c */
VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype, OCIStmt *stmtp);

/* encoding.c */
void Init_oci8_encoding(VALUE cOCI8);
extern int oci8_nls_ratio;
extern rb_encoding *oci8_encoding;

/* util.c */
void Init_oci8_util(VALUE cOCI8);
const char *oci8_dll_path(void);

/* win32.c */
void Init_oci8_win32(VALUE cOCI8);

/* hook_funcs.c */
void oci8_install_hook_functions(void);
void oci8_shutdown_sockets(void);
#ifdef HAVE_PLTHOOK
extern int oci8_cancel_read_at_exit;
extern int oci8_tcp_keepalive_time;
#else
#define oci8_cancel_read_at_exit (-1)
#define oci8_tcp_keepalive_time (-1)
#endif

#define OCI8StringValue(v) do { \
    StringValue(v); \
    (v) = rb_str_export_to_enc(v, oci8_encoding); \
} while (0)
#define OCI8SafeStringValue(v) do { \
    SafeStringValue(v); \
    (v) = rb_str_export_to_enc(v, oci8_encoding); \
} while (0)

#include "thread_util.h"
#include "apiwrap.h"

#endif
