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
#ifdef __cplusplus
extern "C" {
#endif
#include <oci.h>
#ifdef __cplusplus
}
#endif
#include "extconf.h"

#define IS_OCI_ERROR(v) (((v) != OCI_SUCCESS) && ((v) != OCI_SUCCESS_WITH_INFO))

/* OraDate - Internal format of DATE */
struct ora_date {
  unsigned char century;
  unsigned char year;
  unsigned char month;
  unsigned char day;
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
};
typedef struct ora_date ora_date_t;

/* Member of ora_vnumber_t and ora_bind_handle_t - Internal format of NUMBER */
struct ora_number {
  unsigned char exponent;
  unsigned char mantissa[20];
};
typedef struct ora_number ora_number_t;

/* OraNumber - Internal format of VARNUM */
struct ora_vnumber {
  unsigned char size;
  struct ora_number num;
};
typedef struct ora_vnumber ora_vnumber_t;

/* OCIEnv, OCISvcCtx, OCIStmt, OCIDescribe, OCIParam */
struct oci8_handle {
  ub4 type;
  dvoid *hp;
  struct oci8_handle *envh;
  OCIError *errhp;
  VALUE self;
  struct oci8_handle *parent;
  size_t size;
  struct oci8_handle **children;
  /* End of common part */
  union {
    struct {
      int logon_type;
      OCISession *authhp;
      OCIServer *srvhp;
    } svcctx;
    struct {
      char is_implicit:1;
    } param;
#ifndef OCI8_USE_CALLBACK_LOB_READ
    struct {
      int char_width;
    } lob_locator;
#endif
  } u;
};
typedef struct oci8_handle oci8_handle_t;

typedef struct oci8_bind_type oci8_bind_type_t;

/* OCIBind, OCIDefine */
struct oci8_bind_handle {
  ub4 type;
  dvoid *hp;
  struct oci8_handle *envh;
  OCIError *errhp;
  VALUE self;
  struct oci8_handle *parent;
  size_t size;
  struct oci8_handle **children;
  /* End of common part */
  oci8_bind_type_t *bind_type;
  void *valuep;
  sb4 value_sz;
  ub2 rlen;
  sb2 ind;
  VALUE obj;
  union {
    sword sw;
    double dbl;
    ora_date_t od;
    ora_number_t on;
    void *hp;
  } value;
};
typedef struct oci8_bind_handle oci8_bind_handle_t;

struct oci8_bind_type {
  VALUE (*get)(oci8_bind_handle_t *bh);
  void (*set)(oci8_bind_handle_t *bh, VALUE val);
  void (*init)(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale);
  void (*free)(oci8_bind_handle_t *bh);
  ub2 dty;
};

#define Get_Handle(obj, hp) do { \
  Data_Get_Struct(obj, oci8_handle_t, hp); \
} while (0);

#define Check_Handle(obj, name, hp) do {\
  if (!rb_obj_is_instance_of(obj, c##name)) { \
    rb_raise(rb_eTypeError, "invalid argument %s (expect " #name ")", rb_class2name(CLASS_OF(obj))); \
  } \
  Data_Get_Struct(obj, oci8_handle_t, hp); \
} while (0)

#define Check_Object(obj, name) do {\
  if (!rb_obj_is_instance_of(obj, c##name)) { \
    rb_raise(rb_eTypeError, "invalid argument %s (expect " #name ")", rb_class2name(CLASS_OF(obj))); \
  } \
} while (0)

struct oci8_string {
  OraText *ptr;
  ub4 len;
};
typedef struct oci8_string oci8_string_t;

#define Get_String(obj, s) do { \
  if (!NIL_P(obj)) { \
    Check_Type(obj, T_STRING); \
    s.ptr = RSTRING(obj)->ptr; \
    s.len = RSTRING(obj)->len; \
  } else { \
    s.ptr = NULL; \
    s.len = 0; \
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

/* Handle */
extern VALUE cOCIHandle;
extern VALUE cOCIEnv;
extern VALUE cOCISvcCtx;
extern VALUE cOCIStmt;
extern VALUE cOCIBind;

/* Descriptor */
extern VALUE cOCIDescriptor;
extern VALUE cOCILobLocator;
extern VALUE cOCIParam;
extern VALUE cOCIRowid;

/* Exception */
extern VALUE eOCIException;
extern VALUE eOCINoData;
extern VALUE eOCIError;
extern VALUE eOCIInvalidHandle;
extern VALUE eOCINeedData;
extern VALUE eOCIStillExecuting;
extern VALUE eOCIContinue;
extern VALUE eOCISuccessWithInfo;

/* oracle specific type */
extern VALUE cOraDate;
extern VALUE cOraNumber;

/* OCI8 class */
extern VALUE cOCI8;
extern VALUE mOCI8BindType;

/* const.c */
void  Init_oci8_const(void);
extern ID oci8_id_code;
extern ID oci8_id_message;
extern ID oci8_id_new;
extern ID oci8_id_parse_error_offset;
extern ID oci8_id_sql;
extern VALUE oci8_sym_select_stmt;
extern VALUE oci8_sym_update_stmt;
extern VALUE oci8_sym_delete_stmt;
extern VALUE oci8_sym_insert_stmt;
extern VALUE oci8_sym_create_stmt;
extern VALUE oci8_sym_drop_stmt;
extern VALUE oci8_sym_alter_stmt;
extern VALUE oci8_sym_begin_stmt;
extern VALUE oci8_sym_declare_stmt;

/* handle.c */
void  Init_oci8_handle(void);
VALUE oci8_handle_do_initialize(VALUE self, ub4 type);
VALUE oci8_handle_free(VALUE self);
void oci8_handle_mark(oci8_handle_t *);
void oci8_handle_cleanup(oci8_handle_t *);
oci8_handle_t *oci8_make_handle(ub4 type, dvoid *hp, OCIError *errhp, oci8_handle_t *chp, sb4 value_sz);
VALUE oci8_make_rowid(OCIEnv *envhp, OCIError *errhp);
void oci8_link(oci8_handle_t *parent, oci8_handle_t *child);
void oci8_unlink(oci8_handle_t *self);

/* env.c */
void Init_oci8_env(void);
extern VALUE oci8_env; /* use temporarily. delete later. */

/* error.c */
void Init_oci8_error(void);
NORETURN(void oci8_raise(OCIError *, sword status, OCIStmt *));
NORETURN(void oci8_env_raise(OCIEnv *, sword status));

/* svcctx.c */
void Init_oci8_svcctx(void);

/* stmt.c */
void Init_oci8_stmt(void);

/* bind.c */
void Init_oci8_bind(void);
void oci8_register_bind_type(const char *name, oci8_bind_type_t *bind_type);

/* descriptor.c */
void Init_oci8_descriptor(void);
VALUE oci8_descriptor_do_initialize(VALUE self, ub4 type);
VALUE oci8_param_get(VALUE self, VALUE pos);

/* param.c */
void Init_oci8_param(void);

/* lob.c */
void Init_oci8_lob(void);

/* oradate.c */
void Init_ora_date(void);

/* oranumber.c */
#define ORA_NUMBER_BUF_SIZE (128 /* max scale */ + 38 /* max precision */ + 1 /* sign */ + 1 /* comma */ + 1 /* nul */)
void Init_ora_number(void);
void ora_number_to_str(unsigned char *buf, size_t *lenp, ora_number_t *on, unsigned char size);

/* attr.c */
VALUE oci8_get_sb1_attr(VALUE self, ub4 attrtype);
VALUE oci8_get_ub2_attr(VALUE self, ub4 attrtype);
VALUE oci8_get_sb2_attr(VALUE self, ub4 attrtype);
VALUE oci8_get_ub4_attr(VALUE self, ub4 attrtype);
VALUE oci8_get_string_attr(VALUE self, ub4 attrtype);
VALUE oci8_get_rowid_attr(VALUE self, ub4 attrtype);

#define _D_ fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, __FUNCTION__)
#endif
