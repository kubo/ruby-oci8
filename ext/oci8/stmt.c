/*
  stmt.c - part of ruby-oci8
           implement the methods of OCIStmt.

  Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>

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

static oci8_bind_handle_t *make_bind_handle(ub4 type, oci8_handle_t *stmth, VALUE obj)
{
  if (rb_obj_is_kind_of(obj, cOCIBind) && CLASS_OF(obj) != cOCIBind) {
    oci8_bind_handle_t *bh = DATA_PTR(obj);
    bh->type = type;
    bh->errhp = stmth->errhp;
    oci8_link(stmth, (oci8_handle_t*)bh);
    return bh;
  } else {
    /* error */
    rb_raise(rb_eArgError, "Not supported object %s (expect a subclass of OCIBind)", rb_class2name(obj));
  }
}

static VALUE oci8_stmt_initialize(VALUE self, VALUE venv)
{
  return oci8_handle_do_initialize(self, venv, OCI_HTYPE_STMT);
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
  for (i = 0;i < h->size;i++) {
    if (h->children[i] != NULL) {
      if (h->children[i]->type == OCI_HTYPE_BIND || h->children[i]->type == OCI_HTYPE_DEFINE) {
	oci8_handle_free(h->children[i]->self);
      }
    }
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
static VALUE oci8_define_by_pos(VALUE self, VALUE vposition, VALUE vbindobj)
{
  oci8_handle_t *h;
  ub4 position;
  oci8_bind_handle_t *bh;
  OCIDefine *dfnhp = NULL;
  sword status;

  Get_Handle(self, h); /* 0 */
  position = NUM2INT(vposition); /* 1 */
  bh = make_bind_handle(OCI_HTYPE_DEFINE, h, vbindobj); /* 2 */

  status = OCIDefineByPos(h->hp, &dfnhp, h->errhp, position, bh->valuep, bh->value_sz, bh->bind_type->dty, &bh->ind, &bh->rlen, 0, OCI_DEFAULT);
  if (status != OCI_SUCCESS) {
    oci8_unlink((oci8_handle_t *)bh);
    bh->type = 0;
    oci8_raise(h->errhp, status, h->hp);
  }
  bh->hp = dfnhp;
  return bh->self;
}

static VALUE oci8_bind(VALUE self, VALUE vplaceholder, VALUE vbindobj)
{
  oci8_handle_t *h;
  oci8_string_t placeholder;
  ub4 position = 0;
  oci8_bind_handle_t *bh;
  OCIBind *bindhp = NULL;
  sword status;
  int bind_by_pos = 0;

  Get_Handle(self, h); /* 0 */
  if (NIL_P(vplaceholder)) {
    placeholder.ptr = NULL;
    placeholder.len = 0;
  } else if (SYMBOL_P(vplaceholder)) {
    char *symname = rb_id2name(SYM2ID(vplaceholder));
    size_t len = strlen(symname);
    placeholder.ptr = ALLOCA_N(char, len + 1);
    placeholder.len = len + 1;
    placeholder.ptr[0] = ':';
    memcpy(placeholder.ptr + 1, symname, len);
  } else if (FIXNUM_P(vplaceholder)) {
    bind_by_pos = 1;
    position = NUM2INT(vplaceholder);
  } else {
    Check_Type(vplaceholder, T_STRING);
    placeholder.ptr = RSTRING(vplaceholder)->ptr;
    placeholder.len = RSTRING(vplaceholder)->len;
  }
  bh = make_bind_handle(OCI_HTYPE_BIND, h, vbindobj); /* 2 */

  if (bind_by_pos) {
    status = OCIBindByPos(h->hp, &bindhp, h->errhp, position, bh->valuep, bh->value_sz, bh->bind_type->dty, &bh->ind, &bh->rlen, 0, 0, 0, OCI_DEFAULT);
  } else {
    status = OCIBindByName(h->hp, &bindhp, h->errhp, placeholder.ptr, placeholder.len, bh->valuep, bh->value_sz, bh->bind_type->dty, &bh->ind, &bh->rlen, 0, 0, 0, OCI_DEFAULT);
  }
  if (status != OCI_SUCCESS) {
    oci8_unlink((oci8_handle_t *)bh);
    bh->type = 0;
    oci8_raise(h->errhp, status, h->hp);
  }
  bh->hp = bindhp;
  return bh->self;
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
    ub4 errcode;
    OCIErrorGet(h->errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
    if (errcode == 1000) {
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
  return Qtrue;
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
  rb_define_method(cOCIStmt, "initialize", oci8_stmt_initialize, 1);
  rb_define_method(cOCIStmt, "prepare", oci8_stmt_prepare, -1);
  rb_define_method(cOCIStmt, "defineByPos", oci8_define_by_pos, 2);
  rb_define_method(cOCIStmt, "bind", oci8_bind, 2);
  rb_define_method(cOCIStmt, "execute", oci8_stmt_execute, -1);
  rb_define_method(cOCIStmt, "fetch", oci8_stmt_fetch, -1);
  rb_define_method(cOCIStmt, "paramGet", oci8_param_get, 1);
}
