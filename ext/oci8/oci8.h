/*
  oci8.h - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>
*/
#ifndef _RUBY_OCI_H_
#define _RUBY_OCI_H_ 1

#include "ruby.h"
#include "rubyio.h"
#include "intern.h"

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
#include "extconf.h"

#define DEBUG_CORE_FILE 1
#define OCI8_DEBUG 1
#ifdef OCI8_DEBUG
#define ASSERT(v) if (!(v)) { rb_bug("%s:%d: " #v, __FILE__, __LINE__); }
#define ASSERT_(v) if (!(v)) { abort(); }
#else
#define ASSERT(v)
#endif

#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))

#define rb_define_method_nodoc rb_define_method

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
    VALUE (*get)(oci8_bind_t *bh);
    void (*set)(oci8_bind_t *bh, VALUE val);
    void (*init)(oci8_bind_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale);
    ub2 dty;
};

struct oci8_base {
    ub4 type;
    dvoid *hp;
    VALUE self;
    oci8_base_class_t *klass;
};

struct oci8_bind {
    oci8_base_t base;
    oci8_bind_t *next;
    oci8_bind_t *prev;
    void *valuep;
    sb4 value_sz;
    ub2 rlen;
    sb2 ind;
};

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

#define Get_Int_With_Default(argc, pos, vval, cval, def) do { \
  if (argc >= pos) { \
    Check_Type(vval, T_FIXNUM); \
    cval = FIX2INT(vval); \
  } else { \
    cval = def; \
  } \
} while (0)

/* env.c */
extern OCIEnv *oci8_envhp;
extern OCIError *oci8_errhp;
void Init_oci8_env(void);

/* const.c */
extern ID oci8_id_new;
void  Init_oci8_const(void);

/* oci8.c */
void oci8_base_free(oci8_base_t *base);
VALUE oci8_define_class(const char *name, oci8_base_class_t *klass);
VALUE oci8_define_bind_class(const char *name, oci8_bind_class_t *oci8_bind_class);

/* error.c */
extern VALUE eOCIException;
void Init_oci8_error(void);
NORETURN(void oci8_raise(OCIError *, sword status, OCIStmt *));
NORETURN(void oci8_env_raise(OCIEnv *, sword status));

/* svcctx.c */
void Init_oci8_svcctx(void);
oci8_base_t *oci8_get_svcctx(VALUE obj);

/* stmt.c */
void Init_oci8_stmt(void);

/* bind.c */
typedef struct {
    oci8_bind_t bind;
    void *hp;
    VALUE obj;
} oci8_bind_handle_t;
void oci8_bind_free(oci8_base_t *base);
void oci8_bind_handle_mark(oci8_base_t *base);
VALUE oci8_bind_handle_get(oci8_bind_t *bind);
void Init_oci8_bind(VALUE cOCIBind);
oci8_bind_t *oci8_get_bind(VALUE obj);

/* rowid.c */
void Init_oci8_rowid(void);
VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype);

/* param.c */
void Init_oci8_param(void);
VALUE oci8_param_create(OCIParam *parmhp, OCIError *errhp);

/* lob.c */
void Init_oci8_lob(void);

/* oradate.c */
void Init_ora_date(void);

/* oranumber.c */
void Init_ora_number(void);

/* ocinumber.c */
void Init_oci_number(VALUE mOCI);

/* attr.c */
VALUE oci8_get_sb1_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_sb2_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_ub4_attr(oci8_base_t *base, ub4 attrtype);
VALUE oci8_get_string_attr(oci8_base_t *base, ub4 attrtype);

#define _D_ fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, __FUNCTION__)
#endif
