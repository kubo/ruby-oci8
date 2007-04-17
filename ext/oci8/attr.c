#include "oci8.h"

#define ENTRY(name, type, setter, getter) \
  {"OCI_ATTR_" #name, OCI_ATTR_##name, type, setter, getter}

static VALUE get_param(oci8_handle_t *hp, ub4 attr);
static VALUE get_ub4(oci8_handle_t *hp, ub4 attr);
static VALUE get_ub2(oci8_handle_t *hp, ub4 attr);
static VALUE get_ub1(oci8_handle_t *hp, ub4 attr);
static VALUE get_sb1(oci8_handle_t *hp, ub4 attr);
static VALUE get_boolean(oci8_handle_t *hp, ub4 attr);
static VALUE get_precision(oci8_handle_t *hp, ub4 attr);
static VALUE get_string(oci8_handle_t *hp, ub4 attr);
static VALUE get_rowid(oci8_handle_t *hp, ub4 attr);
static VALUE get_oranum_as_int(oci8_handle_t *hp, ub4 attr);
static VALUE get_server(oci8_handle_t *hp, ub4 attr);

static void set_server(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_session(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_ub4(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_ub2(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_ub1(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_no_arg(oci8_handle_t *hp, ub4 attr, VALUE value);
static void set_string(oci8_handle_t *hp, ub4 attr, VALUE value);

oci8_attr_t oci8_attr_list[] = {
  /* Attribute Types */
  ENTRY(NONBLOCKING_MODE,   ATTR_FOR_HNDL, get_boolean, set_no_arg), /* 3 */
  ENTRY(SERVER,             ATTR_FOR_HNDL, get_server, set_server), /* 6 */
  ENTRY(SESSION,            ATTR_FOR_HNDL, NULL, set_session), /* 7 */
  ENTRY(ROW_COUNT,          ATTR_FOR_HNDL, get_ub4, NULL), /* 9 */
  ENTRY(PREFETCH_ROWS,      ATTR_FOR_HNDL, NULL, set_ub4), /* 11 */
  ENTRY(PREFETCH_MEMORY,    ATTR_FOR_HNDL, NULL, set_ub4), /* 13 */
#ifdef OCI_ATTR_FSPRECISION
  ENTRY(FSPRECISION,        ATTR_FOR_DESC, get_ub1, NULL), /* 16 */
#endif
#ifdef OCI_ATTR_LFPRECISION
  ENTRY(LFPRECISION,        ATTR_FOR_DESC, get_ub1, NULL), /* 17 */
#endif
  ENTRY(PARAM_COUNT,        ATTR_FOR_HNDL, get_ub4, NULL), /* 18 */
  ENTRY(ROWID,              ATTR_FOR_HNDL, get_rowid, NULL), /* 19 */
  ENTRY(USERNAME,           ATTR_FOR_HNDL, NULL, set_string), /* 22 */
  ENTRY(PASSWORD,           ATTR_FOR_HNDL, NULL, set_string), /* 23 */
  ENTRY(STMT_TYPE,          ATTR_FOR_HNDL, get_ub2, NULL), /* 24 */
  ENTRY(CHARSET_ID,         ATTR_FOR_BOTH, get_ub2, set_ub2), /* 31 */
  ENTRY(CHARSET_FORM,       ATTR_FOR_BOTH, get_ub1, set_ub1), /* 32 */
  ENTRY(MAXDATA_SIZE,       ATTR_FOR_HNDL, get_ub2, NULL), /* 33 */
  ENTRY(ROWS_RETURNED,      ATTR_FOR_HNDL, get_ub4, NULL), /* 42 */
  ENTRY(LOBEMPTY,           ATTR_FOR_DESC, NULL, set_ub4), /* 45 */
  ENTRY(NUM_COLS,           ATTR_FOR_DESC, get_ub2, NULL), /* 102 */
  ENTRY(LIST_COLUMNS,       ATTR_FOR_DESC, get_param, NULL), /* 103 */
  ENTRY(CLUSTERED,          ATTR_FOR_DESC, get_boolean, NULL), /* 105 */
  ENTRY(PARTITIONED,        ATTR_FOR_DESC, get_boolean, NULL), /* 106 */
  ENTRY(INDEX_ONLY,         ATTR_FOR_DESC, get_boolean, NULL), /* 107 */
  ENTRY(LIST_ARGUMENTS,     ATTR_FOR_DESC, get_param, NULL), /* 108 */
  ENTRY(LIST_SUBPROGRAMS,   ATTR_FOR_DESC, get_param, NULL), /* 109 */
  ENTRY(LINK,               ATTR_FOR_DESC, get_string, NULL), /* 111 */
  ENTRY(MIN,                ATTR_FOR_DESC, get_oranum_as_int, NULL), /* 112 */
  ENTRY(MAX,                ATTR_FOR_DESC, get_oranum_as_int, NULL), /* 113 */
  ENTRY(INCR,               ATTR_FOR_DESC, get_oranum_as_int, NULL), /* 114 */
  ENTRY(CACHE,              ATTR_FOR_DESC, get_oranum_as_int, NULL), /* 115 */
  ENTRY(ORDER,              ATTR_FOR_DESC, get_boolean, NULL), /* 116 */
  ENTRY(HW_MARK,            ATTR_FOR_DESC, get_oranum_as_int, NULL), /* 117 */
  ENTRY(NUM_PARAMS,         ATTR_FOR_DESC, get_ub2, NULL), /* 121 */
  ENTRY(OBJID,              ATTR_FOR_DESC, get_ub4, NULL), /* 121 */
  ENTRY(PTYPE,              ATTR_FOR_DESC, get_ub1, NULL), /* 123 */
  ENTRY(PARAM,              ATTR_FOR_HNDL, get_param, NULL), /* 124 */
  /* ENTRY(PARSE_ERROR_OFFSET, ATTR_FOR_HNDL, get_ub2, NULL), */ /* 129 */
#ifdef OCI_ATTR_IS_TEMPORARY
  ENTRY(IS_TEMPORARY,       ATTR_FOR_DESC, get_boolean, NULL), /* 130 */
#endif
#ifdef OCI_ATTR_IS_INVOKER_RIGHTS
  ENTRY(IS_INVOKER_RIGHTS,  ATTR_FOR_DESC, get_boolean, NULL), /* 133 */
#endif
#ifdef OCI_ATTR_OBJ_NAME
  ENTRY(OBJ_NAME,           ATTR_FOR_DESC, get_string, NULL), /* 134 */
#endif
#ifdef OCI_ATTR_OBJ_SCHEMA
  ENTRY(OBJ_SCHEMA,         ATTR_FOR_DESC, get_string, NULL), /* 135 */
#endif
#ifdef OCI_ATTR_OBJ_ID
  ENTRY(OBJ_ID,             ATTR_FOR_DESC, get_ub4, NULL), /* 136 */
#endif
#ifdef OCI_ATTR_STATEMENT
  ENTRY(STATEMENT,          ATTR_FOR_HNDL, get_string, NULL), /* 144 */
#endif
#ifdef OCI_ATTR_MAXCHAR_SIZE
  ENTRY(MAXCHAR_SIZE,       ATTR_FOR_HNDL, get_ub2, set_ub2), /* 163 */
#endif
#ifdef OCI_ATTR_CURRENT_POSITION
  ENTRY(CURRENT_POSITION,   ATTR_FOR_HNDL, get_ub4, NULL), /* 164 */
#endif
#ifdef OCI_ATTR_ROWS_FETCHED
  ENTRY(ROWS_FETCHED,       ATTR_FOR_HNDL, get_ub4, NULL), /* 197 */
#endif
  ENTRY(DESC_PUBLIC,        ATTR_FOR_HNDL, NULL, set_ub4), /* 250 */

  /* Describe Handle Parameter Attributes */
  ENTRY(DATA_SIZE,          ATTR_FOR_DESC, get_ub2, NULL), /* 1 */
  ENTRY(DATA_TYPE,          ATTR_FOR_DESC, get_ub2, NULL), /* 2 */
  ENTRY(NAME,               ATTR_FOR_DESC, get_string, NULL), /* 4 */
  ENTRY(PRECISION,          ATTR_FOR_DESC, get_precision, NULL), /* 5 */
  ENTRY(SCALE,              ATTR_FOR_DESC, get_sb1, NULL), /* 6 */
  ENTRY(IS_NULL,            ATTR_FOR_DESC, get_boolean, NULL), /* 7 */
  ENTRY(TYPE_NAME,          ATTR_FOR_DESC, get_string, NULL), /* 8 */
  ENTRY(SCHEMA_NAME,        ATTR_FOR_DESC, get_string, NULL), /* 9 */
#ifdef OCI_ATTR_CHAR_USED
  ENTRY(CHAR_USED,          ATTR_FOR_DESC, get_boolean, NULL), /* 285 */
#endif
#ifdef OCI_ATTR_CHAR_SIZE
  ENTRY(CHAR_SIZE,          ATTR_FOR_DESC, get_ub2, NULL), /* 286 */
#endif
};
size_t oci8_attr_size = sizeof(oci8_attr_list) / sizeof(oci8_attr_list[0]);

static VALUE get_param(oci8_handle_t *hp, ub4 attr)
{
  OCIParam *parmhp;
  ub4 as = 0;
  oci8_handle_t *parmh;
  oci8_handle_t *parenth;
  sword rv;

  rv = OCIAttrGet(hp->hp, hp->type, &parmhp, &as, attr, hp->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(hp->errhp, rv, NULL);

  for (parenth = hp;parenth->type == OCI_DTYPE_PARAM;parenth = parenth->parent);
  parmh = oci8_make_handle(OCI_DTYPE_PARAM, parmhp, hp->errhp, parenth, 0);
  return parmh->self;
}

#if 0
#define DEFINE_GETTER_FUNC_FOR_NUMBER(attrtype) \
static VALUE get_##attrtype(oci8_handle_t *h, ub4 attr) \
{ \
  attrtype av;
  ub4 as = 0; \
  sword rv; \
\
  rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp); \
  if (rv != OCI_SUCCESS) \
    oci8_raise(h->errhp, rv, NULL); \
  return INT2FIX(av); \
}
#else
#define MARKER_1 0xFE
#define MARKER_2 0xEF
#define DEFINE_GETTER_FUNC_FOR_NUMBER(attrtype) \
static VALUE get_##attrtype(oci8_handle_t *h, ub4 attr) \
{ \
  union { \
    attrtype v; \
    unsigned char s[sizeof(attrtype) + 1]; \
  } av; \
  ub4 as = 0; \
  sword rv; \
\
  memset(&av, MARKER_1, sizeof(av)); \
  rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp); \
  if (rv != OCI_SUCCESS) \
    oci8_raise(h->errhp, rv, NULL); \
  if (av.s[sizeof(attrtype)] != MARKER_1) \
    rb_bug("overwrite in get_" #attrtype " for %d", attr); \
  if (av.s[sizeof(attrtype) -1] == MARKER_1) { \
    /* if not overwrited, set another value and retry */ \
    av.s[sizeof(attrtype) -1] = MARKER_2; \
    rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp); \
    if (av.s[sizeof(attrtype) -1] == MARKER_2) \
      rb_bug("specified size is too small in get_" #attrtype " for %d", attr); \
  } \
  return INT2FIX(av.v); \
}
#endif

DEFINE_GETTER_FUNC_FOR_NUMBER(ub4)
DEFINE_GETTER_FUNC_FOR_NUMBER(ub2)
DEFINE_GETTER_FUNC_FOR_NUMBER(ub1)
DEFINE_GETTER_FUNC_FOR_NUMBER(sb1)

static VALUE get_boolean(oci8_handle_t *h, ub4 attr)
{
  sb1 av;
  ub4 as = 0;
  sword rv;

  rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return av ? Qtrue : Qfalse;
}

static VALUE get_precision(oci8_handle_t *h, ub4 attr)
{
  union {
    sb1 _sb1;
    sb2 _sb2;
    unsigned char s[sizeof(sb2) + 1];
  } av;
  ub4 as = 0;
  int size = sizeof(sb1);
  int is_implicit = 0;
  sword rv;

  if (h->type == OCI_DTYPE_PARAM) {
    if (h->u.param.is_implicit) {
      is_implicit = 1;
      size = sizeof(sb2);
    }
  }

  /* runtime check */
  memset(&av, MARKER_1, sizeof(av));
  rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  if (av.s[size] != MARKER_1)
    rb_bug("overwrite in get_precision for %d(%s)", attr, is_implicit ? "implicit" : "explicit");
  if (av.s[size -1] == MARKER_1) {
    /* if not overwrited, set another value and retry */
    av.s[size -1] = MARKER_2;
    rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp);
    if (av.s[size -1] == MARKER_2)
      rb_bug("specified size is too small in get_precision for %d(%s)", attr, is_implicit ? "implicit" : "explicit");
  }
  if (!is_implicit) {
    return INT2FIX(av._sb1);
  } else {
    return INT2FIX(av._sb2);
  }
}

static VALUE get_string(oci8_handle_t *h, ub4 attr)
{
  text *txt;
  ub4 size;
  sword rv;

  rv = OCIAttrGet(h->hp, h->type, &txt, &size, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return rb_str_new(TO_CHARPTR(txt), size);
}

static VALUE get_rowid(oci8_handle_t *h, ub4 attr)
{
  OCIRowid *rowidhp;
  oci8_handle_t *rowidh;
  oci8_handle_t *envh;
  sword rv;

  /* get environment handle */
  for (envh = h; envh->type != OCI_HTYPE_ENV; envh = envh->parent);
  rv = OCIDescriptorAlloc(envh->hp, (void *)&rowidhp, OCI_DTYPE_ROWID, 0, NULL);
  if (rv != OCI_SUCCESS) {
    oci8_env_raise(envh->hp, rv);
  }
  rv = OCIAttrGet(h->hp, h->type, rowidhp, 0, attr, h->errhp);
  if (rv != OCI_SUCCESS) {
    OCIDescriptorFree(rowidhp, OCI_DTYPE_ROWID);
    oci8_raise(h->errhp, rv, NULL);
  }
  rowidh = oci8_make_handle(OCI_DTYPE_ROWID, rowidhp, h->errhp, envh, 0);
  return rowidh->self;
}

static VALUE get_oranum_as_int(oci8_handle_t *h, ub4 attr) {
  ora_number_t *av;
  ub4 as = 0;
  sword rv;
  char buf[ORA_NUMBER_BUF_SIZE];
  size_t len;

  rv = OCIAttrGet(h->hp, h->type, &av, &as, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  ora_number_to_str(TO_ORATEXT(buf), &len, av, as);
  return rb_cstr2inum(buf, 10);
}

static VALUE get_server(oci8_handle_t *h, ub4 attr)
{
  oci8_handle_t *hv;
  void *hp;
  sword rv;

  rv = OCIAttrGet(h->hp, h->type, &hp, 0, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  hv = oci8_make_handle(OCI_HTYPE_SERVER, hp, h->errhp, h, 0);
  return hv->self;
}

static void set_server(oci8_handle_t *h, ub4 attr, VALUE value)
{
  oci8_handle_t *hv;
  sword rv;

  Check_Handle(value, OCIServer, hv);
  rv = OCIAttrSet(h->hp, h->type, hv->hp, 0, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
}

static void set_session(oci8_handle_t *h, ub4 attr, VALUE value)
{
  oci8_handle_t *hv;
  sword rv;

  Check_Handle(value, OCISession, hv);
  rv = OCIAttrSet(h->hp, h->type, hv->hp, 0, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
}

#define DEFINE_SETTER_FUNC_FOR_NUMBER(attrtype) \
static void set_##attrtype(oci8_handle_t *h, ub4 attr, VALUE value) \
{ \
  attrtype hv; \
  sword rv; \
 \
  hv = NUM2INT(value); \
  rv = OCIAttrSet(h->hp, h->type, &hv, sizeof(attrtype), attr, h->errhp); \
  if (rv != OCI_SUCCESS) \
    oci8_raise(h->errhp, rv, NULL); \
}

DEFINE_SETTER_FUNC_FOR_NUMBER(ub4)
DEFINE_SETTER_FUNC_FOR_NUMBER(ub2)
DEFINE_SETTER_FUNC_FOR_NUMBER(ub1)

static void set_no_arg(oci8_handle_t *h, ub4 attr, VALUE value)
{
  sword rv;

  if (!NIL_P(value)) {
    rb_raise(rb_eArgError, "invalid argument %s (expect nil: this attribute value is ignored, so set nil)", rb_class2name(CLASS_OF(value)));
  }
  rv = OCIAttrSet(h->hp, h->type, 0, 0, attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
}

static void set_string(oci8_handle_t *h, ub4 attr, VALUE value)
{
  sword rv;

  Check_Type(value, T_STRING);
  rv = OCIAttrSet(h->hp, h->type, RSTRING_PTR(value), RSTRING_LEN(value), attr, h->errhp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
}

VALUE oci8_attr_set(VALUE self, VALUE vtype, VALUE value)
{
  oci8_handle_t *h;
  ub4 type;
  char attr_type_flag;

  Get_Handle(self, h); /* 0 */
  type = NUM2INT(vtype); /* 1 */

  /* check range. */
  if (type < 0 || oci8_attr_size <= type)
    rb_raise(rb_eArgError, "invalid OCI_ATTR_ type");

  /* check attribute type */
  if (h->type < OCI_DTYPE_FIRST)
    attr_type_flag = ATTR_FOR_HNDL;
  else
    attr_type_flag = ATTR_FOR_DESC;
  if (!(oci8_attr_list[type].attr_type & attr_type_flag))
    rb_raise(rb_eArgError, "invalid OCI_ATTR_ type");

  if (oci8_attr_list[type].set == NULL)
    rb_raise(rb_eArgError, "attrSet is not permitted for %s", oci8_attr_list[type].name);

  oci8_attr_list[type].set(h, oci8_attr_list[type].attr, value);

  switch (oci8_attr_list[type].attr) {
  case OCI_ATTR_SERVER:
    rb_ivar_set(self, oci8_id_server, value);
    break;
  case OCI_ATTR_SESSION:
    rb_ivar_set(self, oci8_id_session, value);
    break;
  }
  return self;
}

VALUE oci8_attr_get(VALUE self, VALUE vtype)
{
  oci8_handle_t *h;
  ub4 type;
  char attr_type_flag;

  Get_Handle(self, h); /* 0 */
  type = NUM2INT(vtype); /* 1 */

  /* check range. */
  if (type < 0 || oci8_attr_size <= type)
    rb_raise(rb_eArgError, "invalid OCI_ATTR_ type");

  /* check attribute type */
  if (h->type < OCI_DTYPE_FIRST)
    attr_type_flag = ATTR_FOR_HNDL;
  else
    attr_type_flag = ATTR_FOR_DESC;
  if (!(oci8_attr_list[type].attr_type & attr_type_flag))
    rb_raise(rb_eArgError, "invalid OCI_ATTR_ type");
  
  if (oci8_attr_list[type].get == NULL)
    rb_raise(rb_eArgError, "attrGet is not permitted for %s", oci8_attr_list[type].name);
  return oci8_attr_list[type].get(h, oci8_attr_list[type].attr);
}
