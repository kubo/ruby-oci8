/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
  oci8.h - part of ruby-oci8

  Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
*/
#ifndef _RUBY_OCI_H_
#define _RUBY_OCI_H_ 1

#include "ruby.h"
#include "rubyio.h"
#ifndef RUBY_VM
#include "intern.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <oci.h>
#ifdef __cplusplus
}
#endif
#ifdef HAVE_XMLOTN_H
#include "xmlotn.h"
#endif
#include "extconf.h"

#if ACTUAL_ORACLE_CLIENT_VERSION < 1020
typedef struct OCIAdmin OCIAdmin;
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
#ifndef RCLASS_SUPER
#define RCLASS_SUPER(c) RCLASS(c)->super
#endif
#ifndef RFLOAT_VALUE
#define RFLOAT_VALUE(obj) RFLOAT(obj)->value
#endif

#ifndef RUBY_VM
#define rb_errinfo() ruby_errinfo
#endif

#ifdef _WIN32
#define RUNTIME_API_CHECK
#endif

#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))

#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
/* gcc version >= 3.1 */
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif
#ifdef _MSC_VER
/* microsoft c */
#define ALWAYS_INLINE __forceinline
#endif

#ifdef ALWAYS_INLINE
/*
 * I don't like cast because it can suppress warnings but may hide bugs.
 * These macros make warnings when the source type is invalid.
 */
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
/* if not gcc, use normal cast. */
#define TO_ORATEXT(c) ((OraText*)(c))
#define TO_CHARPTR(c) ((char*)(c))
#endif
#define RSTRING_ORATEXT(obj) TO_ORATEXT(RSTRING_PTR(obj))
#define rb_str_new2_ora(str) rb_str_new2(TO_CHARPTR(str))

#define rb_define_method_nodoc rb_define_method

/* data structure for SQLT_LVC and SQLT_LVB. */
typedef struct {
    sb4 size;
    char buf[1];
} oci8_vstr_t;

typedef struct oci8_base_class oci8_base_class_t;
typedef struct oci8_bind_class oci8_bind_class_t;

typedef struct oci8_base oci8_base_t;
typedef struct oci8_bind oci8_bind_t;

struct oci8_base_class {
    void (*mark)(oci8_base_t *base);
    void (*free)(oci8_base_t *base);
    size_t size;
};

struct oci8_bind_class {
    oci8_base_class_t base;
    VALUE (*get)(oci8_bind_t *obind, void *data, void *null_struct);
    void (*set)(oci8_bind_t *obind, void *data, void **null_structp, VALUE val);
    void (*init)(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length);
    void (*init_elem)(oci8_bind_t *obind, VALUE svc);
    ub1 (*in)(oci8_bind_t *obind, ub4 idx, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp);
    void (*out)(oci8_bind_t *obind, ub4 idx, ub1 piece, void **valuepp, ub4 **alenpp, void **indpp);
    void (*pre_fetch_hook)(oci8_bind_t *obind, VALUE svc);
    ub2 dty;
    ub1 csfrm;
};

struct oci8_base {
    ub4 type;
    union {
        dvoid *ptr;
        OCISvcCtx *svc;
        OCIStmt *stmt;
        OCIDefine *dfn;
        OCIBind *bnd;
        OCIParam *prm;
        OCILobLocator *lob;
        OCIType *tdo;
    } hp;
    VALUE self;
    oci8_base_class_t *klass;
    oci8_base_t *parent;
    oci8_base_t *next;
    oci8_base_t *prev;
    oci8_base_t *children;
};

struct oci8_bind {
    oci8_base_t base;
    void *valuep;
    sb4 value_sz; /* size to define or bind. */
    sb4 alloc_sz; /* size of a element. */
    ub4 maxar_sz; /* maximum array size. */
    ub4 curar_sz; /* current array size. */
    VALUE tdo;
    union {
        void **null_structs;
        sb2 *inds;
    } u;
};

enum logon_type_t {T_NOT_LOGIN = 0, T_IMPLICIT, T_EXPLICIT};

typedef struct  {
    oci8_base_t base;
    volatile VALUE executing_thread;
    enum logon_type_t logon_type;
    OCISession *authhp;
    OCIServer *srvhp;
    char is_autocommit;
#ifdef RUBY_VM
    char non_blocking;
#endif
    VALUE long_read_len;
} oci8_svcctx_t;

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

/* dangerous macros */
#define CHECK_STRING(obj) if (!NIL_P(obj)) { StringValue(obj); }
#define TO_STRING_PTR(obj) (NIL_P(obj) ? NULL : RSTRING(obj)->ptr)
#define TO_STRING_LEN(obj) (NIL_P(obj) ? 0 : RSTRING(obj)->len)

/* env.c */
extern OCIEnv *oci8_envhp;
#ifdef RUBY_VM
OCIError *oci8_get_errhp(void);
#define oci8_errhp oci8_get_errhp()
#else
extern OCIError *oci8_errhp;
#endif
void Init_oci8_env(void);

/* oci8lib.c */
extern ID oci8_id_new;
extern ID oci8_id_get;
extern ID oci8_id_set;
extern ID oci8_id_keys;
extern int oci8_in_finalizer;
void oci8_base_free(oci8_base_t *base);
VALUE oci8_define_class(const char *name, oci8_base_class_t *klass);
VALUE oci8_define_class_under(VALUE outer, const char *name, oci8_base_class_t *klass);
VALUE oci8_define_bind_class(const char *name, oci8_bind_class_t *oci8_bind_class);
void oci8_link_to_parent(oci8_base_t *base, oci8_base_t *parent);
void oci8_unlink_from_parent(oci8_base_t *base);
#ifndef RUBY_VM
typedef VALUE rb_blocking_function_t(void *);
#endif
sword oci8_blocking_region(oci8_svcctx_t *svcctx, rb_blocking_function_t func, void *data);
#if defined RUNTIME_API_CHECK
void *oci8_find_symbol(const char *symbol_name);
#endif
extern oci8_base_class_t oci8_base_class;

/* error.c */
extern VALUE eOCIException;
extern VALUE eOCIBreak;
void Init_oci8_error(void);
NORETURN(void oci8_do_raise(OCIError *, sword status, OCIStmt *, const char *file, int line));
NORETURN(void oci8_do_env_raise(OCIEnv *, sword status, const char *file, int line));
NORETURN(void oci8_do_raise_init_error(const char *file, int line));
sb4 oci8_get_error_code(OCIError *errhp);

/* oci8.c */
VALUE Init_oci8(void);
oci8_svcctx_t *oci8_get_svcctx(VALUE obj);
OCISvcCtx *oci8_get_oci_svcctx(VALUE obj);
OCISession *oci8_get_oci_session(VALUE obj);
#define TO_SVCCTX oci8_get_oci_svcctx
#define TO_SESSION oci8_get_oci_session

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
void Init_oci8_bind(VALUE cOCIBind);
oci8_bind_t *oci8_get_bind(VALUE obj);

/* rowid.c */
void Init_oci8_rowid(void);
VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype);

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

/* oradate.c */
void Init_ora_date(void);

/* ocinumber.c */
void Init_oci_number(VALUE mOCI);
OCINumber *oci8_get_ocinumber(VALUE num);
VALUE oci8_make_ocinumber(OCINumber *s);
VALUE oci8_make_integer(OCINumber *s);
VALUE oci8_make_float(OCINumber *s);
OCINumber *oci8_set_ocinumber(OCINumber *result, VALUE self);
OCINumber *oci8_set_integer(OCINumber *result, VALUE self);

/* ocidatetim.c */
void Init_oci_datetime(void);
VALUE oci8_make_datetime_from_ocidate(OCIDate *s);
VALUE oci8_make_datetime_from_ocidatetime(OCIDateTime *s);
VALUE oci8_make_interval_ym(OCIInterval *s);
VALUE oci8_make_interval_ds(OCIInterval *s);

/* object.c */
void Init_oci_object(VALUE mOCI);

/* xmldb.c */
#ifndef XMLCTX_DEFINED
#define XMLCTX_DEFINED
struct xmlctx; typedef struct xmlctx xmlctx;
#endif
#ifndef XML_TYPES
typedef struct xmlnode xmlnode;
#endif
void Init_oci_xmldb(void);
VALUE oci8_make_rexml(struct xmlctx *xctx, xmlnode *node);

/* attr.c */
VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype);

#include "apiwrap.h"

#endif
