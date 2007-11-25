/*
  stmt.c - part of ruby-oci8
           implement the methods of OCIStmt.

  Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIStmt
Statemet handle identify a SQL or PL/SQL statement and its associated attributes.

Information about SQL or PL/SQL's input/output variables is managed by the 
((<bind handle|OCIBind>)). Fetched data of select statement is managed by the
((<define handle|OCIDefine>)). 

super class: ((<OCIHandle>))

correspond native OCI datatype: ((|OCIStmt|))
=end
*/
#include "oci8.h"

static ID id_alloc;

static void check_bind_type(ub4 type, oci8_handle_t *stmth, VALUE vtype, VALUE vlength, oci8_bind_handle_t **bhp, ub2 *dty)
{
  enum oci8_bind_type bind_type;
  oci8_bind_handle_t *bh;
  sb4 value_sz;
  VALUE klass = Qnil;

  if (TYPE(vtype) == T_FIXNUM) {
    switch (FIX2INT(vtype)) {
    case SQLT_CHR: /* OCI_TYPECODE_VARCHAR */
      bind_type = BIND_STRING;
      *dty = SQLT_LVC;
      if (NIL_P(vlength))
	rb_raise(rb_eArgError, "the length of String is not specified.");
      value_sz = NUM2INT(vlength) + 4;
      if (value_sz < 5)
        value_sz = 5; /* at least 5 bytes */
      break;
    case SQLT_LVB: /* OCI_TYPECODE_RAW */
    case SQLT_BIN: /* OCI_TYPECODE_UNSIGNED8 */
      bind_type = BIND_STRING;
      *dty = SQLT_LVB;
      if (NIL_P(vlength))
	rb_raise(rb_eArgError, "the length of String is not specified.");
      value_sz = NUM2INT(vlength) + 4;
      if (value_sz < 5)
        value_sz = 5; /* at least 5 bytes */
      break;
    case SQLT_DAT:
      bind_type = BIND_ORA_DATE;
      *dty = SQLT_DAT;
      value_sz = sizeof(ora_date_t);
      break;
    case SQLT_CLOB: /* OCI_TYPECODE_CLOB */
    case SQLT_BLOB: /* OCI_TYPECODE_BLOB */
      bind_type = BIND_HANDLE;
      *dty = FIX2INT(vtype);
      value_sz = sizeof(bh->value.handle);
      klass = cOCILobLocator;
      break;
    case SQLT_BFILE:
    case SQLT_CFILE:
      bind_type = BIND_HANDLE;
      *dty = FIX2INT(vtype);
      value_sz = sizeof(bh->value.handle);
      klass = cOCIFileLocator;
      break;
    case SQLT_RDD:
      bind_type = BIND_HANDLE;
      *dty = SQLT_RDD;
      value_sz = sizeof(bh->value.handle);
      klass = cOCIRowid;
      break;
    case SQLT_RSET:
      bind_type = BIND_HANDLE;
      *dty = SQLT_RSET;
      value_sz = sizeof(bh->value.handle);
      klass = cOCIStmt;
      break;
#ifdef SQLT_IBDOUBLE
    case SQLT_IBDOUBLE:
      bind_type = BIND_FLOAT;
      *dty = SQLT_BDOUBLE;
      value_sz = sizeof(double);
      break;
#endif
    default:
      rb_raise(rb_eArgError, "Not supported type (%d)", FIX2INT(vtype));
    }
  } else if (vtype == rb_cFixnum) {
    bind_type = BIND_FIXNUM;
    *dty = SQLT_INT;
    value_sz = sizeof(long);
  } else if (vtype == rb_cInteger || vtype == rb_cBignum) {
    bind_type = BIND_INTEGER_VIA_ORA_NUMBER;
    *dty = SQLT_NUM;
    value_sz = sizeof(ora_number_t);
  } else if (vtype == rb_cTime) {
    bind_type = BIND_TIME_VIA_ORA_DATE;
    *dty = SQLT_DAT;
    value_sz = sizeof(ora_date_t);
  } else if (vtype == rb_cString) {
    bind_type = BIND_STRING;
    *dty = SQLT_LVC;
    if (NIL_P(vlength))
      rb_raise(rb_eArgError, "the length of String is not specified.");
    value_sz = NUM2INT(vlength) + 4;
    if (value_sz < 5)
      value_sz = 5; /* at least 5 bytes */
  } else if (vtype == rb_cFloat) {
    bind_type = BIND_FLOAT;
    *dty = SQLT_FLT;
    value_sz = sizeof(double);
  } else if (vtype == cOraDate) {
    bind_type = BIND_ORA_DATE;
    *dty = SQLT_DAT;
    value_sz = sizeof(ora_date_t);
  } else if (vtype == cOraNumber) {
    bind_type = BIND_ORA_NUMBER;
    *dty = SQLT_NUM;
    value_sz = sizeof(ora_number_t);
  } else {
    if (SYMBOL_P(vtype)) {
      rb_raise(rb_eArgError, "Not supported type (:%s)", rb_id2name(SYM2ID(vtype)));
    } else {
      rb_raise(rb_eArgError, "Not supported type (%s)", rb_class2name(vtype));
    }
  }
  bh = (oci8_bind_handle_t *)oci8_make_handle(type, NULL, NULL, stmth, value_sz);
  bh->bind_type = bind_type;
  if (bind_type == BIND_HANDLE) {
    if (NIL_P(vlength)) {
      oci8_handle_t *envh;
      for (envh = stmth; envh->type != OCI_HTYPE_ENV; envh = envh->parent);
      vlength = rb_funcall(envh->self, id_alloc, 1, klass);
    }
    bh->value.handle.klass = klass;
    oci8_set_value(bh, vlength);
  }
  *bhp = bh;
}

static VALUE oci8_each_value(VALUE hash)
{
  return rb_funcall(hash, rb_intern("each_value"), 0);
}

/*
=begin
--- OCIStmt#prepare(stmt [, language [, mode]])
     set and prepare SQL statement.

     :stmt
        SQL or PL/SQL statement
     :language
        ((|OCI_NTV_SYNTAX|)), ((|OCI_V7_SYNTAX|)), or ((|OCI_V8_SYNTAX|)).
        Default value is ((|OCI_NTV_SYNTAX|))
     :mode
        ((|OCI_DEFAULT|)) or ((|OCI_NO_SHARING|)). Default value is ((|OCI_DEFAULT|)).

        ((|OCI_NO_SHARING|)) disables ((<Shared Data Mode>)) for this statement.

     correspond native OCI function: ((|OCIStmtPrepare|))
=end
 */
static VALUE oci8_stmt_prepare(int argc, VALUE *argv, VALUE self)
{
  VALUE vsql, vlanguage, vmode;
  oci8_handle_t *h;
  oci8_string_t s;
  ub4 language;
  ub4 mode;
  sword rv;
  VALUE ary;
  VALUE hash;
  int i;

  rb_scan_args(argc, argv, "12", &vsql, &vlanguage, &vmode);
  Get_Handle(self, h); /* 0 */
  Get_String(vsql, s); /* 1 */
  Get_Int_With_Default(argc, 2, vlanguage, language, OCI_NTV_SYNTAX); /* 2 */
  Get_Int_With_Default(argc, 3, vmode, mode, OCI_DEFAULT); /* 3 */

  /* when a new statement is prepared, OCI implicitly free the previous 
   * statement's define and bind handles. 
   * But ruby's object don't know it. So free these handles in advance.
   */
  /* free define handles */
  ary = rb_ivar_get(self, oci8_id_define_array);
  if (ary != Qnil) {
    for (i = 0;i < RARRAY_LEN(ary);i++) {
      if (RARRAY_PTR(ary)[i] != Qnil)
	oci8_handle_free(RARRAY_PTR(ary)[i]);
    }
    rb_ivar_set(self, oci8_id_define_array, Qnil);
  }
  /* free bind handles */
  hash = rb_ivar_get(self, oci8_id_bind_hash);
  if (hash != Qnil) {
    rb_iterate(oci8_each_value, hash, oci8_handle_free, Qnil);
    rb_ivar_set(self, oci8_id_bind_hash, Qnil);
  }

  rv = OCIStmtPrepare(h->hp, h->errhp, s.ptr, s.len, language, mode);
  if (IS_OCI_ERROR(rv)) {
    oci8_raise(h->errhp, rv, h->hp);
  }
  return self;
}

/*
=begin
--- OCIStmt#defineByPos(position, type [, length [, mode]])
     define the datatype of fetched column.
     You must define all column's datatype, before you fetch data.

     :position
        the position of the column. It starts from 1.
     :type
        the type of column. 
        ((|String|)), ((|Fixnum|)), ((|Integer|)), ((|Float|)), ((|Time|)),
        ((<OraDate>)), ((<OraNumber>)), or ((|OCI_TYPECODE_RAW|))
     :length
        When the 2nd argument is 
        * ((|String|)) or ((|OCI_TYPECODE_RAW|)),
          the max length of fetched data.
        * otherwise,
          its value is ignored.
     :mode
        ((|OCI_DEFAULT|)), or ((|OCI_DYNAMIC_FETCH|)). But now available value is 
        ((|OCI_DEFAULT|)) only. Default value is ((|OCI_DEFAULT|))
     :return value
        newly created ((<define handle|OCIDefine>))

     correspond native OCI function: ((|OCIDefineByPos|))
=end
 */
static VALUE oci8_define_by_pos(int argc, VALUE *argv, VALUE self)
{
  VALUE vposition;
  VALUE vtype;
  VALUE vlength;
  VALUE vmode;
  oci8_handle_t *h;
  ub4 position;
  oci8_bind_handle_t *bh;
  ub2 dty;
  ub4 mode;
  dvoid *indp;
  ub2 *rlenp;
  dvoid *valuep;
  OCIDefine *dfnhp = NULL;
  sword status;
  VALUE ary;
  VALUE obj;

  rb_scan_args(argc, argv, "22", &vposition, &vtype, &vlength, &vmode);
  Get_Handle(self, h); /* 0 */
  position = NUM2INT(vposition); /* 1 */
  check_bind_type(OCI_HTYPE_DEFINE, h, vtype, vlength, &bh, &dty); /* 2, 3 */
  Get_Int_With_Default(argc, 4, vmode, mode, OCI_DEFAULT); /* 4 */

  if (mode & OCI_DYNAMIC_FETCH) {
    indp = NULL;
    rlenp = NULL;
  } else {
    indp = &(bh->ind);
    rlenp = (bh->bind_type == BIND_STRING) ? NULL : &bh->rlen;
  }
  valuep = &bh->value;
  status = OCIDefineByPos(h->hp, &dfnhp, h->errhp, position, valuep, bh->value_sz, dty, indp, rlenp, 0, mode);
  if (status != OCI_SUCCESS) {
    oci8_unlink((oci8_handle_t *)bh);
    bh->type = 0;
    oci8_raise(h->errhp, status, h->hp);
  }
  bh->type = OCI_HTYPE_DEFINE;
  bh->hp = dfnhp;
  bh->errhp = h->errhp;
  obj = bh->self;
  ary = rb_ivar_get(self, oci8_id_define_array);
  if (ary == Qnil) {
    ary = rb_ary_new();
    rb_ivar_set(self, oci8_id_define_array, ary);
  }
  rb_ary_store(ary, position - 1, obj);
  return obj;
}

/*
=begin
--- OCIStmt#bindByPos(position, type [, length [, mode]])
     define the datatype of the bind variable by posision.

     :position
        the position of the bind variable.
     :type
        the type of the bind variable.
        ((|String|)), ((|Fixnum|)), ((|Integer|)), ((|Float|)), ((|Time|)),
        ((<OraDate>)), ((<OraNumber>)), or ((|OCI_TYPECODE_RAW|))
     :length
        When the 2nd argument is 
        * ((|String|)) or ((|OCI_TYPECODE_RAW|)),
          the max length of fetched data.
        * otherwise,
          its value is ignored.
     :mode
        ((|OCI_DEFAULT|)), or ((|OCI_DATA_AT_EXEC|)). But now available value is 
        ((|OCI_DEFAULT|)) only. Default value is ((|OCI_DEFAULT|))
     :return value
        newly created ((<bind handle|OCIBind>))

     correspond native OCI function: ((|OCIBindByPos|))
=end
 */
static VALUE oci8_bind_by_pos(int argc, VALUE *argv, VALUE self)
{
  VALUE vposition;
  VALUE vtype;
  VALUE vlength;
  VALUE vmode;
  oci8_handle_t *h;
  ub4 position;
  oci8_bind_handle_t *bh;
  ub2 dty;
  ub4 mode;
  dvoid *indp;
  ub2 *rlenp;
  dvoid *valuep;
  OCIBind *bindhp = NULL;
  sword status;
  VALUE hash;
  VALUE obj;

  rb_scan_args(argc, argv, "22", &vposition, &vtype, &vlength, &vmode);
  Get_Handle(self, h); /* 0 */
  position = NUM2INT(vposition); /* 1 */
  check_bind_type(OCI_HTYPE_BIND, h, vtype, vlength, &bh, &dty); /* 2, 3 */
  Get_Int_With_Default(argc, 4, vmode, mode, OCI_DEFAULT); /* 4 */

  if (mode & OCI_DATA_AT_EXEC) {
    indp = NULL;
    rlenp = NULL;
  } else {
    indp = &(bh->ind);
    rlenp = (bh->bind_type == BIND_STRING) ? NULL : &bh->rlen;
  }
  valuep = &bh->value;
  status = OCIBindByPos(h->hp, &bindhp, h->errhp, position, valuep, bh->value_sz, dty, indp, rlenp, 0, 0, 0, mode);
  if (status != OCI_SUCCESS) {
    oci8_unlink((oci8_handle_t *)bh);
    bh->type = 0;
    oci8_raise(h->errhp, status, h->hp);
  }
  bh->type = OCI_HTYPE_BIND;
  bh->hp = bindhp;
  bh->errhp = h->errhp;
  obj = bh->self;
  hash = rb_ivar_get(self, oci8_id_bind_hash);
  if (hash == Qnil) {
    hash = rb_hash_new();
    rb_ivar_set(self, oci8_id_bind_hash, hash);
  }
  rb_hash_aset(hash, vposition, obj);
  return obj;
}

/*
=begin
--- OCIStmt#bindByName(name, type [, length [, mode]])
     define the datatype of the bind variable by name.

     :name
        the name of the bind variable including colon.
     :type
        the type of the bind variable.
        ((|String|)), ((|Fixnum|)), ((|Integer|)), ((|Float|)), ((|Time|)),
        ((<OraDate>)), ((<OraNumber>)), or ((|OCI_TYPECODE_RAW|))
     :length
        When the 2nd argument is 
        * ((|String|)) or ((|OCI_TYPECODE_RAW|)),
          the max length of fetched data.
        * otherwise,
          its value is ignored.
     :mode
        ((|OCI_DEFAULT|)), or ((|OCI_DATA_AT_EXEC|)). But now available value is 
        ((|OCI_DEFAULT|)) only. Default value is ((|OCI_DEFAULT|))
     :return value
        newly created ((<bind handle|OCIBind>))

     for example
       stmt = env.alloc(OCIStmt)
       stmt.prepare("SELECT * FROM EMP
                      WHERE ename = :ENAME
                        AND sal > :SAL
                        AND hiredate >= :HIREDATE")
       b_ename = stmt.bindByName(":ENAME", String, 10)
       b_sal = stmt.bindByName(":SAL", Fixnum)
       b_hiredate = stmt.bindByName(":HIREDATE", OraDate)

     correspond native OCI function: ((|OCIBindByName|))
=end
 */
static VALUE oci8_bind_by_name(int argc, VALUE *argv, VALUE self)
{
  VALUE vplaceholder;
  VALUE vtype;
  VALUE vlength;
  VALUE vmode;
  oci8_handle_t *h;
  oci8_string_t placeholder;
  oci8_bind_handle_t *bh;
  ub2 dty;
  ub4 mode;
  dvoid *indp;
  ub2 *rlenp;
  dvoid *valuep;
  OCIBind *bindhp = NULL;
  sword status;
  VALUE hash;
  VALUE obj;

  rb_scan_args(argc, argv, "22", &vplaceholder, &vtype, &vlength, &vmode);
  Get_Handle(self, h); /* 0 */
  Get_String(vplaceholder, placeholder); /* 1 */
  check_bind_type(OCI_HTYPE_BIND, h, vtype, vlength, &bh, &dty); /* 2, 3 */
  Get_Int_With_Default(argc, 4, vmode, mode, OCI_DEFAULT); /* 4 */

  if (mode & OCI_DATA_AT_EXEC) {
    indp = NULL;
    rlenp = NULL;
  } else {
    indp = &(bh->ind);
    rlenp = (bh->bind_type == BIND_STRING) ? NULL : &bh->rlen;
  }
  valuep = &bh->value;
  status = OCIBindByName(h->hp, &bindhp, h->errhp, placeholder.ptr, placeholder.len, valuep, bh->value_sz, dty, indp, rlenp, 0, 0, 0, mode);
  if (status != OCI_SUCCESS) {
    oci8_unlink((oci8_handle_t *)bh);
    bh->type = 0;
    oci8_raise(h->errhp, status, h->hp);
  }
  bh->type = OCI_HTYPE_BIND;
  bh->hp = bindhp;
  bh->errhp = h->errhp;
  obj = bh->self;
  hash = rb_ivar_get(self, oci8_id_bind_hash);
  if (hash == Qnil) {
    hash = rb_hash_new();
    rb_ivar_set(self, oci8_id_bind_hash, hash);
  }
  rb_hash_aset(hash, vplaceholder, obj);
  return obj;
}

/*
=begin
--- OCIStmt#execute(svc [, iters [, mode]])
     execute statement at the ((<service context handle|OCISvcCtx>)).

     :svc
        ((<service context handle|OCISvcCtx>))
     :iters
        the number of iterations to execute.

        For select statement, if there are columns which is not defined
        by ((<OCIStmt#defineByPos>)) and this value is positive, it 
        raises exception. If zero, no exception. In any case you must define
        all columns before you call ((<OCIStmt#fetch>)).

        For non-select statement, use positive value.

        Default value is 0 for select statement, 1 for non-select statement.

        note: Current implemantation doesn't support array fetch and batch mode, so
        valid value is 0 or 1.
     :mode
        ((|OCI_DEFAULT|)), ((|OCI_BATCH_ERRORS|)), ((|OCI_COMMIT_ON_SUCCESS|)),
        ((|OCI_DESCRIBE_ONLY|)), ((|OCI_EXACT_FETCH|)), ((|OCI_PARSE_ONLY|)), 
        any combinations of previous values, or ((|OCI_STMT_SCROLLABLE_READONLY|)).
        Default value is ((|OCI_DEFAULT|)).

        ((|OCI_BATCH_ERRORS|)) and ((|OCI_STMT_SCROLLABLE_READONLY|)) are not
        supported by current implementation.

     correspond native OCI function: ((|OCIStmtExecute|))
=end
*/
static VALUE oci8_stmt_execute(int argc, VALUE *argv, VALUE self)
{
  VALUE vsvc;
  VALUE viters;
  VALUE vmode;
  oci8_handle_t *h;
  oci8_handle_t *svch;
  ub4 mode;
  ub4 iters;
  ub2 stmt_type;
  sword rv;

  rb_scan_args(argc, argv, "12", &vsvc, &viters, &vmode);
  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  if (argc >= 2) {
    iters = NUM2INT(viters); /* 2 */
  } else {
    rv = OCIAttrGet(h->hp, OCI_HTYPE_STMT, &stmt_type, 0, OCI_ATTR_STMT_TYPE, h->errhp);
    if (rv != OCI_SUCCESS) {
      oci8_raise(h->errhp, rv, h->hp);
    }
    if (stmt_type == OCI_STMT_SELECT) {
      /* for select statement, default value 0. */
      iters = 0;
    } else {
      /* for non-select statement, default value 0. */
      iters = 1;
    }
  }
  Get_Int_With_Default(argc, 3, vmode, mode, OCI_DEFAULT); /* 3 */

  if (iters < 0) {
    rb_raise(rb_eArgError, "use Positive value for the 2nd argument");
  } else if (iters > 1) {
    rb_raise(rb_eArgError, "current implementation doesn't support array fatch or batch mode");
  }

  rv = OCIStmtExecute(svch->hp, h->hp, h->errhp, iters, 0, NULL, NULL, mode);
  if (rv == OCI_ERROR) {
    sb4 errcode;
    OCIErrorGet(h->errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
    if (errcode == 1000) {
      /* run GC to close unreferred cursors when ORA-01000 (maximum open cursors exceeded). */
      rb_gc();
      rv = OCIStmtExecute(svch->hp, h->hp, h->errhp, iters, 0, NULL, NULL, mode);
    }
  }
  if (IS_OCI_ERROR(rv)) {
    oci8_raise(h->errhp, rv, h->hp);
  }
  return self;
}

/*
=begin
--- OCIStmt#fetch([nrows [, orientation [, mode]]])
     fetch data from select statement.
     fetched data are stored to previously defined ((<define handle|OCIDefine>)).

     :nrows
        number of rows to fetch. If zero, cancel the cursor.
        The default value is 1.

        Because array fetch is not supported, valid value is 0 or 1.

     :orientation
        orientation to fetch. ((|OCI_FETCH_NEXT|)) only valid.
        The default value is ((|OCI_FETCH_NEXT|)).

     :mode
        ((|OCI_DEFULT|)) only valid. 
        The default value is ((|OCI_DEFAULT|)).

     :return value
        array of define handles, which are defined previously,
        or nil when end of data.

     correspond native OCI function: ((|OCIStmtFetch|))
=end
*/
static VALUE oci8_stmt_fetch(int argc, VALUE *argv, VALUE self)
{
  VALUE vnrows;
  VALUE vorientation;
  VALUE vmode;
  oci8_handle_t *h;
  ub4 nrows;
  ub2 orientation;
  ub4 mode;
  sword rv;

  rb_scan_args(argc, argv, "03", &vnrows, &vorientation, &vmode);
  Get_Handle(self, h); /* 0 */
  Get_Int_With_Default(argc, 1, vnrows, nrows, 1); /* 1 */
  Get_Int_With_Default(argc, 2, vorientation, orientation, OCI_FETCH_NEXT); /* 2 */
  Get_Int_With_Default(argc, 3, vmode, mode, OCI_DEFAULT); /* 3 */

  rv = OCIStmtFetch(h->hp, h->errhp, nrows, orientation, mode);
  if (rv == OCI_NO_DATA) {
    return Qnil;
  }
  if (IS_OCI_ERROR(rv)) {
    oci8_raise(h->errhp, rv, h->hp);
  }
  return rb_ivar_get(self, oci8_id_define_array);
}  

/*
implemented in param.c
=begin
--- OCIStmt#paramGet(position)
     get column information of executed select statement.
     See ((<Select a table whose column types are unknown.>))

     :posision
        the position of the column to get parameter. It starts from 1.
     :return value
        newly created ((<read-only parameter descriptor|OCIParam>))
        
     correspond native OCI function: ((|OCIParamGet|))
=end
*/

void Init_oci8_stmt(void)
{
  id_alloc = rb_intern("alloc");
  rb_define_method(cOCIStmt, "prepare", oci8_stmt_prepare, -1);
  rb_define_method(cOCIStmt, "defineByPos", oci8_define_by_pos, -1);
  rb_define_method(cOCIStmt, "bindByPos", oci8_bind_by_pos, -1);
  rb_define_method(cOCIStmt, "bindByName", oci8_bind_by_name, -1);
  rb_define_method(cOCIStmt, "execute", oci8_stmt_execute, -1);
  rb_define_method(cOCIStmt, "fetch", oci8_stmt_fetch, -1);
  rb_define_method(cOCIStmt, "paramGet", oci8_param_get, 1);
}
