/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oci8.h - part of ruby-oci8
 *
 * Copyright (C) 2002-2011 KUBO Takehiro <kubo@jiubao.org>
 */
#ifndef _RUBY_OCI_H_
#define _RUBY_OCI_H_ 1

#include "ruby.h"

#ifndef rb_pid_t
#ifdef WIN32
#define rb_pid_t int
#else
#define rb_pid_t pid_t
#endif
#endif

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

#define ORAVERNUM(major, minor, update, patch, port_update) \
    (((major) << 24) | ((minor) << 20) | ((update) << 12) | ((patch) << 8) | (port_update))

#define ORAVER_8_0 ORAVERNUM(8, 0, 0, 0, 0)
#define ORAVER_8_1 ORAVERNUM(8, 1, 0, 0, 0)
#define ORAVER_9_0 ORAVERNUM(9, 0, 0, 0, 0)
#define ORAVER_9_2 ORAVERNUM(9, 2, 0, 0, 0)
#define ORAVER_10_1 ORAVERNUM(10, 1, 0, 0, 0)
#define ORAVER_10_2 ORAVERNUM(10, 2, 0, 0, 0)
#define ORAVER_11_1 ORAVERNUM(11, 1, 0, 0, 0)

#include "extconf.h"
#ifdef HAVE_TYPE_RB_ENCODING
#include <ruby/encoding.h>
#endif

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

#ifndef HAVE_TYPE_ORATEXT
typedef unsigned char oratext;
#endif
#if !defined HAVE_TYPE_OCIDATETIME_ && !defined HAVE_TYPE_OCIDATETIMEP
typedef struct OCIDateTime OCIDateTime;
#endif
#if !defined HAVE_TYPE_OCIINTERVAL_ && !defined HAVE_TYPE_OCIINTERVALP
typedef struct OCIInterval OCIInterval;
#endif
#ifndef HAVE_TYPE_OCICALLBACKLOBREAD2
typedef sb4 (*OCICallbackLobRead2)(dvoid *ctxp, CONST dvoid *bufp, oraub8 len,
                                   ub1 piece, dvoid **changed_bufpp,
                                   oraub8 *changed_lenp);
#endif
#ifndef HAVE_TYPE_OCICALLBACKLOBWRITE2
typedef sb4 (*OCICallbackLobWrite2)(dvoid *ctxp, dvoid *bufp, oraub8 *lenp,
                                    ub1 *piece, dvoid **changed_bufpp,
                                    oraub8 *changed_lenp);
#endif
#if !defined HAVE_TYPE_OCIADMIN_ && !defined HAVE_TYPE_OCIADMINP
typedef struct OCIAdmin OCIAdmin;
#endif
#if !defined HAVE_TYPE_OCIMSG_ && !defined HAVE_TYPE_OCIMSGP
typedef struct OCIMsg  OCIMsg;
#endif
#if !defined HAVE_TYPE_OCICPOOL_ && !defined HAVE_TYPE_OCICPOOLP
typedef struct OCICPool OCICPool;
#endif

/* new macros in ruby 1.8.6.
 * define compatible macros for ruby 1.8.5 or lower.
 */
#ifndef RSTRING_PTR
#define RSTRING_PTR(obj) RSTRING(obj)->ptr
#endif
#ifndef RSTRING_LEN
#define RSTRING_LEN(obj) RSTRING(obj)->len
#endif
#ifndef RARRAY_PTR
#define RARRAY_PTR(obj) RARRAY(obj)->ptr
#endif
#ifndef RARRAY_LEN
#define RARRAY_LEN(obj) RARRAY(obj)->len
#endif

/* new macros in ruby 1.9.
 * define compatible macros for ruby 1.8 or lower.
 */
#ifndef RFLOAT_VALUE
#define RFLOAT_VALUE(obj) RFLOAT(obj)->value
#endif
#ifndef STRINGIZE
#define STRINGIZE(name) #name
#endif
#ifndef RB_GC_GUARD
#define RB_GC_GUARD(v) (*(volatile VALUE *)&(v))
#endif

/* new functions in ruby 1.9.
 * define compatible macros for ruby 1.8 or lower.
 */
#if !defined(HAVE_RB_ERRINFO) && defined(HAVE_RUBY_ERRINFO)
#define rb_errinfo() ruby_errinfo
#endif
#if !defined HAVE_TYPE_RB_BLOCKING_FUNCTION_T_ && !defined HAVE_TYPE_RB_BLOCKING_FUNCTION_TP
typedef VALUE rb_blocking_function_t(void *);
#endif

#ifndef HAVE_TYPE_RB_ENCODING
#define rb_enc_associate(str, enc) do {} while(0)
#define rb_external_str_new_with_enc(ptr, len, enc) rb_tainted_str_new((ptr), (len))
#define rb_locale_str_new_cstr(ptr)  rb_str_new2(ptr)
#define rb_str_conv_enc(str, from, to) (str)
#define rb_str_export_to_enc(str, enc) (str)
#define rb_usascii_str_new(ptr, len) rb_str_new((ptr), (len))
#define rb_usascii_str_new_cstr(ptr) rb_str_new2(ptr)
#endif

/* a new function in ruby 1.9.3.
 * define a compatible macro for ruby 1.9.2 or lower.
 */
#ifndef HAVE_RB_CLASS_SUPERCLASS
#ifdef RCLASS_SUPER
#define rb_class_superclass(cls) RCLASS_SUPER(cls)
#else
#define rb_class_superclass(cls) (RCLASS(cls)->super)
#endif
#endif

#if defined(HAVE_NATIVETHREAD) || defined(HAVE_RB_THREAD_BLOCKING_REGION)
/*
 * oci8_errhp is a thread local object in ruby 1.9, rubinius
 * and ruby 1.8 configured with --enable-pthread.
 */
#define USE_THREAD_LOCAL_ERRHP 1
#endif

/* macros depends on the compiler.
 *  LIKELY(x)      hint for the compiler that 'x' is 1(TRUE) in many cases.
 *  UNLIKELY(x)    hint for the compiler that 'x' is 0(FALSE) in many cases.
 *  ALWAYS_INLINE  forcely inline the function.
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
#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
/* gcc version >= 3.1 */
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif
#ifdef _MSC_VER
/* microsoft c */
#define ALWAYS_INLINE __forceinline
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
#ifdef USE_THREAD_LOCAL_ERRHP
/* rubies with native-thread support. */
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
#endif /* HAVE_RB_THREAD_BLOCKING_REGION */

/* utility macros
 */
#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))
#ifdef ALWAYS_INLINE
#define TO_ORATEXT to_oratext
#define TO_CHARPTR to_charptr
static ALWAYS_INLINE OraText *to_oratext(char *c)
{
    return (OraText*)c;
}
static ALWAYS_INLINE char *to_charptr(OraText *c)
{
    return (char*)c;
}
#else
#define TO_ORATEXT(c) ((OraText*)(c))
#define TO_CHARPTR(c) ((char*)(c))
#endif
#define RSTRING_ORATEXT(obj) TO_ORATEXT(RSTRING_PTR(obj))
#define rb_str_new2_ora(str) rb_str_new2(TO_CHARPTR(str))

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

typedef struct oci8_base_vtable oci8_base_vtable_t;
typedef struct oci8_bind_vtable oci8_bind_vtable_t;

typedef struct oci8_base oci8_base_t;
typedef struct oci8_bind oci8_bind_t;

/* The virtual method table of oci8_base_t */
struct oci8_base_vtable {
    void (*mark)(oci8_base_t *base);
    void (*free)(oci8_base_t *base);
    size_t size;
    void (*init)(oci8_base_t *base);
};

/* The virtual method table of oci8_bind_t */
struct oci8_bind_vtable {
    oci8_base_vtable_t base;
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
 * The vptr member points to a virtual method table.
 * See: http://en.wikipedia.org/wiki/Virtual_method_table
 */
struct oci8_base {
    const oci8_base_vtable_t *vptr;
    ub4 type;
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

typedef struct oci8_svcctx {
    oci8_base_t base;
    volatile VALUE executing_thread;
    const oci8_logoff_strategy_t *logoff_strategy;
    OCISession *usrhp;
    OCIServer *srvhp;
    rb_pid_t pid;
    unsigned char state;
    char is_autocommit;
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
    char non_blocking;
#endif
    VALUE long_read_len;
} oci8_svcctx_t;

struct oci8_logoff_strategy {
    void *(*prepare)(oci8_svcctx_t *svcctx);
    rb_blocking_function_t *execute;
};

typedef struct {
    dvoid *hp; /* OCIBind* or OCIDefine* */
    dvoid *valuep;
    sb4 value_sz;
    ub2 dty;
    sb2 *indp;
    ub2 *alenp;
} oci8_exec_sql_var_t;

#define Check_Handle(obj, klass, hp) do { \
    if (!rb_obj_is_kind_of(obj, klass)) { \
        rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(klass)); \
    } \
    Data_Get_Struct(obj, oci8_base_t, hp); \
} while (0)

#define Check_Object(obj, klass) do {\
  if (!rb_obj_is_kind_of(obj, klass)) { \
    rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(klass)); \
  } \
} while (0)

#define oci8_raise(err, status, stmt) oci8_do_raise(err, status, stmt, __FILE__, __LINE__)
#define oci8_env_raise(err, status) oci8_do_env_raise(err, status, __FILE__, __LINE__)
#define oci8_raise_init_error() oci8_do_raise_init_error(__FILE__, __LINE__)
#define oci8_raise_by_msgno(msgno, default_msg) oci8_do_raise_by_msgno(msgno, default_msg, __FILE__, __LINE__)

/* raise on error */
#define oci_lc(rv) do { \
    sword __rv = (rv); \
    if (__rv != OCI_SUCCESS) { \
        oci8_raise(oci8_errhp, __rv, NULL); \
    } \
} while(0)

#if SIZEOF_LONG > 4
#define UB4_TO_NUM INT2FIX
#else
#define UB4_TO_NUM UINT2NUM
#endif

/* The folloiwng macros oci8_envhp and oci8_errhp are used
 * as if they are defined as follows:
 *
 *   extern OCIEnv *oci8_envhp;
 *   extern OCIError *oci8_errhp;
 */
#define oci8_envhp (LIKELY(oci8_global_envhp != NULL) ? oci8_global_envhp : oci8_make_envhp())
#ifdef USE_THREAD_LOCAL_ERRHP
#define oci8_errhp oci8_get_errhp()
#else
#define oci8_errhp (LIKELY(oci8_global_errhp != NULL) ? oci8_global_errhp : oci8_make_errhp())
#endif

/* env.c */
extern ub4 oci8_env_mode;
extern OCIEnv *oci8_global_envhp;
OCIEnv *oci8_make_envhp(void);
#ifdef USE_THREAD_LOCAL_ERRHP
extern oci8_tls_key_t oci8_tls_key; /* native thread key */
OCIError *oci8_make_errhp(void);

static inline OCIError *oci8_get_errhp()
{
    OCIError *errhp = (OCIError *)oci8_tls_get(oci8_tls_key);
    return LIKELY(errhp != NULL) ? errhp : oci8_make_errhp();
}

#else
extern OCIError *oci8_global_errhp;
OCIError *oci8_make_errhp(void);
#endif
void Init_oci8_env(void);

/* oci8lib.c */
extern ID oci8_id_new;
extern ID oci8_id_get;
extern ID oci8_id_set;
extern ID oci8_id_keys;
extern ID oci8_id_oci8_vtable;
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
VALUE oci8_define_class(const char *name, oci8_base_vtable_t *vptr);
VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_vtable_t *vptr);
VALUE oci8_define_bind_class(const char *name, const oci8_bind_vtable_t *vptr);
void oci8_link_to_parent(oci8_base_t *base, oci8_base_t *parent);
void oci8_unlink_from_parent(oci8_base_t *base);
sword oci8_blocking_region(oci8_svcctx_t *svcctx, rb_blocking_function_t func, void *data);
sword oci8_exec_sql(oci8_svcctx_t *svcctx, const char *sql_text, ub4 num_define_vars, oci8_exec_sql_var_t *define_vars, ub4 num_bind_vars, oci8_exec_sql_var_t *bind_vars, int raise_on_error);
#if defined RUNTIME_API_CHECK
void *oci8_find_symbol(const char *symbol_name);
#endif
oci8_base_t *oci8_get_handle(VALUE obj, VALUE klass);

/* error.c */
extern VALUE eOCIException;
extern VALUE eOCIBreak;
VALUE oci8_make_exc(dvoid *errhp, sword status, ub4 type, OCIStmt *stmthp, const char *file, int line);
void Init_oci8_error(void);
NORETURN(void oci8_do_raise(OCIError *, sword status, OCIStmt *, const char *file, int line));
NORETURN(void oci8_do_env_raise(OCIEnv *, sword status, const char *file, int line));
NORETURN(void oci8_do_raise_init_error(const char *file, int line));
sb4 oci8_get_error_code(OCIError *errhp);
VALUE oci8_get_error_message(ub4 msgno, const char *default_msg);
NORETURN(void oci8_do_raise_by_msgno(ub4 msgno, const char *default_msg, const char *file, int line));

/* ocihandle.c */
void Init_oci8_handle(void);

/* oci8.c */
VALUE Init_oci8(void);
void oci8_do_parse_connect_string(VALUE conn_str, VALUE *user, VALUE *pass, VALUE *dbname, VALUE *mode);
oci8_svcctx_t *oci8_get_svcctx(VALUE obj);
OCISvcCtx *oci8_get_oci_svcctx(VALUE obj);
OCISession *oci8_get_oci_session(VALUE obj);
void oci8_check_pid_consistency(oci8_svcctx_t *svcctx);
#define TO_SVCCTX oci8_get_oci_svcctx
#define TO_SESSION oci8_get_oci_session

/* connection_pool.c */
void Init_oci8_connection_pool(VALUE cOCI8);

/* stmt.c */
extern VALUE cOCIStmt;
void Init_oci8_stmt(VALUE cOCI8);

/* bind.c */
typedef struct {
    void *hp;
    VALUE obj;
} oci8_hp_obj_t;
void oci8_bind_free(oci8_base_t *base);
void oci8_bind_hp_obj_mark(oci8_base_t *base);
void Init_oci8_bind(VALUE cOCI8BindTypeBase);
oci8_bind_t *oci8_get_bind(VALUE obj);
void oci8_bind_set_data(VALUE self, VALUE val);
VALUE oci8_bind_get_data(VALUE self);

/* metadata.c */
extern VALUE cOCI8MetadataBase;
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
OCINumber *oci8_get_ocinumber(VALUE num);
VALUE oci8_make_ocinumber(OCINumber *s, OCIError *errhp);
VALUE oci8_make_integer(OCINumber *s, OCIError *errhp);
VALUE oci8_make_float(OCINumber *s, OCIError *errhp);
OCINumber *oci8_set_ocinumber(OCINumber *result, VALUE self, OCIError *errhp);
OCINumber *oci8_set_integer(OCINumber *result, VALUE self, OCIError *errhp);
double oci8_onum_to_dbl(OCINumber *s, OCIError *errhp);
OCINumber *oci8_dbl_to_onum(OCINumber *result, double dbl, OCIError *errhp);

/* ocidatetim.c */
void Init_oci_datetime(void);
VALUE oci8_make_ocidate(OCIDate *od);
OCIDate *oci8_set_ocidate(OCIDate *od, VALUE val);
VALUE oci8_make_ocidatetime(OCIDateTime *dttm);
OCIDateTime *oci8_set_ocidatetime(OCIDateTime *dttm, VALUE val);
VALUE oci8_make_interval_ym(OCIInterval *s);
VALUE oci8_make_interval_ds(OCIInterval *s);

/* object.c */
void Init_oci_object(VALUE mOCI);

/* attr.c */
VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype);

/* encoding.c */
void Init_oci8_encoding(VALUE cOCI8);
VALUE oci8_charset_id2name(VALUE svc, VALUE charset_id);
extern int oci8_nls_ratio;

/* win32.c */
void Init_oci8_win32(VALUE cOCI8);

#ifdef HAVE_TYPE_RB_ENCODING
extern rb_encoding *oci8_encoding;

#define OCI8StringValue(v) do { \
    StringValue(v); \
    (v) = rb_str_export_to_enc(v, oci8_encoding); \
} while (0)
#define OCI8SafeStringValue(v) do { \
    SafeStringValue(v); \
    (v) = rb_str_export_to_enc(v, oci8_encoding); \
} while (0)
#else
#define OCI8StringValue(v)     StringValue(v)
#define OCI8SafeStringValue(v) SafeStringValue(v)
#endif

#include "thread_util.h"
#include "apiwrap.h"

#endif
