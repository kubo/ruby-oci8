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

static VALUE oci8_sym_select_stmt;
static VALUE oci8_sym_update_stmt;
static VALUE oci8_sym_delete_stmt;
static VALUE oci8_sym_insert_stmt;
static VALUE oci8_sym_create_stmt;
static VALUE oci8_sym_drop_stmt;
static VALUE oci8_sym_alter_stmt;
static VALUE oci8_sym_begin_stmt;
static VALUE oci8_sym_declare_stmt;

static VALUE cOCIStmt;

typedef struct {
    oci8_base_t base;
    oci8_bind_t *next;
    oci8_bind_t *prev;
    VALUE svc;
    VALUE binds;
    VALUE defines;
} oci8_stmt_t;

static void oci8_stmt_mark(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    rb_gc_mark(stmt->svc);
    rb_gc_mark(stmt->binds);
    rb_gc_mark(stmt->defines);
}

static void oci8_stmt_free(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    while (stmt->next != (oci8_bind_t*)stmt)
      oci8_base_free((oci8_base_t*)stmt->next);
    stmt->next = stmt->prev = (oci8_bind_t*)stmt;
    stmt->svc = Qnil;
    stmt->binds = Qnil;
    stmt->defines = Qnil;
}

static oci8_base_class_t oci8_stmt_class = {
    oci8_stmt_mark,
    oci8_stmt_free,
    sizeof(oci8_stmt_t),
};

static VALUE oci8_stmt_initialize(VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    sword rv;

    rv = OCIHandleAlloc(oci8_envhp, &stmt->base.hp, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    stmt->base.type = OCI_HTYPE_STMT;
    stmt->next = (oci8_bind_t*)stmt;
    stmt->prev = (oci8_bind_t*)stmt;
    stmt->svc = Qnil;
    stmt->binds = Qnil;
    stmt->defines = Qnil;
    return Qnil;
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
static VALUE oci8_stmt_prepare(VALUE self, VALUE sql)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    sword rv;

    StringValue(sql);

    /* when a new statement is prepared, OCI implicitly free the previous 
     * statement's define and bind handles. 
     * But ruby's object don't know it. So free these handles in advance.
     */
    oci8_stmt_free(DATA_PTR(self));

    rv = OCIStmtPrepare(stmt->base.hp, oci8_errhp, RSTRING(sql)->ptr, RSTRING(sql)->len, OCI_NTV_SYNTAX, OCI_DEFAULT);
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp);
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
    oci8_stmt_t *stmt = DATA_PTR(self);
    ub4 position;
    oci8_bind_t *bind;
    oci8_bind_class_t *bind_class;
    sword status;

    position = NUM2INT(vposition); /* 1 */
    bind = oci8_get_bind(vbindobj); /* 2 */
    if (bind->base.hp != NULL) {
        oci8_base_free(&bind->base); /* TODO: OK? */
    }
    bind_class = (oci8_bind_class_t *)bind->base.klass;
    status = OCIDefineByPos(stmt->base.hp, (OCIDefine**)&bind->base.hp, oci8_errhp, position, bind->valuep, bind->value_sz, bind_class->dty, &bind->ind, &bind->rlen, 0, OCI_DEFAULT);
    if (status != OCI_SUCCESS) {
	oci8_raise(oci8_errhp, status, stmt->base.hp);
    }
    bind->base.type = OCI_HTYPE_DEFINE;
    if (NIL_P(stmt->defines)) {
      stmt->defines = rb_hash_new();
    }
    rb_hash_aset(stmt->defines, INT2FIX(position), bind->base.self);
    bind->next->prev = bind->prev;
    bind->prev->prev = bind->next;
    bind->next = stmt->next;
    bind->prev = (oci8_bind_t*)stmt;
    stmt->next->prev = bind;
    stmt->next = bind;
    return bind->base.self;
}

static VALUE oci8_bind(VALUE self, VALUE vplaceholder, VALUE vbindobj)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    char *placeholder_ptr = (char*)-1; /* initialize as an invalid value */
    ub4 placeholder_len = 0;
    ub4 position = 0;
    oci8_bind_t *bind;
    oci8_bind_class_t *bind_class;
    sword status;

    if (NIL_P(vplaceholder)) { /* 1 */
        placeholder_ptr = NULL;
	placeholder_len = 0;
    } else if (SYMBOL_P(vplaceholder)) {
        char *symname = rb_id2name(SYM2ID(vplaceholder));
	size_t len = strlen(symname);
	placeholder_ptr = ALLOCA_N(char, len + 1);
	placeholder_len = len + 1;
	placeholder_ptr[0] = ':';
	memcpy(placeholder_ptr + 1, symname, len);
    } else if (FIXNUM_P(vplaceholder)) {
	position = NUM2INT(vplaceholder);
    } else {
        StringValue(vplaceholder);
	placeholder_ptr = RSTRING(vplaceholder)->ptr;
	placeholder_len = RSTRING(vplaceholder)->len;
    }
    bind = oci8_get_bind(vbindobj); /* 2 */
    if (bind->base.hp != NULL) {
        oci8_base_free(&bind->base); /* TODO: OK? */
    }
    bind_class = (oci8_bind_class_t *)bind->base.klass;

    if (placeholder_ptr == (char*)-1) {
        status = OCIBindByPos(stmt->base.hp, (OCIBind**)&bind->base.hp, oci8_errhp, position, bind->valuep, bind->value_sz, bind_class->dty, &bind->ind, &bind->rlen, 0, 0, 0, OCI_DEFAULT);
    } else {
        status = OCIBindByName(stmt->base.hp, (OCIBind**)&bind->base.hp, oci8_errhp, placeholder_ptr, placeholder_len, bind->valuep, bind->value_sz, bind_class->dty, &bind->ind, &bind->rlen, 0, 0, 0, OCI_DEFAULT);
    }
    if (status != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, status, stmt->base.hp);
    }
    bind->base.type = OCI_HTYPE_BIND;
    if (NIL_P(stmt->binds)) {
      stmt->binds = rb_hash_new();
    }
    rb_hash_aset(stmt->binds, vplaceholder, bind->base.self);
    bind->next->prev = bind->prev;
    bind->prev->prev = bind->next;
    bind->next = stmt->next;
    bind->prev = (oci8_bind_t*)stmt;
    stmt->next->prev = bind;
    stmt->next = bind;
    return bind->base.self;
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
static VALUE oci8_stmt_execute(VALUE self, VALUE vsvc, VALUE viters, VALUE vmode)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    OCISvcCtx *svchp = oci8_get_oci_svcctx(vsvc); /* 1 */
    ub4 mode;
    ub4 iters;
    sword rv;

    Check_Type(viters, T_FIXNUM); /* 2 */
    iters = FIX2INT(viters);
    Check_Type(vmode, T_FIXNUM); /* 3 */
    mode = FIX2INT(vmode);

    if (iters < 0) {
        rb_raise(rb_eArgError, "use Positive value for the 2nd argument");
    } else if (iters > 1) {
        rb_raise(rb_eArgError, "current implementation doesn't support array fatch or batch mode");
    }

    rv = OCIStmtExecute(svchp, stmt->base.hp, oci8_errhp, iters, 0, NULL, NULL, mode);
    if (rv == OCI_ERROR) {
        ub4 errcode;
	OCIErrorGet(oci8_errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
	if (errcode == 1000) {
	    rb_gc();
	    rv = OCIStmtExecute(svchp, stmt->base.hp, oci8_errhp, iters, 0, NULL, NULL, mode);
	}
    }
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp);
    }
    stmt->svc = vsvc;
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
    oci8_stmt_t *stmt = DATA_PTR(self);
    VALUE vnrows;
    VALUE vorientation;
    VALUE vmode;
    ub4 nrows;
    ub2 orientation;
    ub4 mode;
    sword rv;

    rb_scan_args(argc, argv, "03", &vnrows, &vorientation, &vmode);
    Get_Int_With_Default(argc, 1, vnrows, nrows, 1); /* 1 */
    Get_Int_With_Default(argc, 2, vorientation, orientation, OCI_FETCH_NEXT); /* 2 */
    Get_Int_With_Default(argc, 3, vmode, mode, OCI_DEFAULT); /* 3 */

    rv = OCIStmtFetch(stmt->base.hp, oci8_errhp, nrows, orientation, mode);
    if (rv == OCI_NO_DATA) {
        return Qnil;
    }
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp);
    }
    if (nrows == 0)
      stmt->svc = Qnil;
    return Qtrue;
}

static VALUE oci8_stmt_get_param(VALUE self, VALUE pos)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    OCIParam *parmhp = NULL;
    sword rv;

    Check_Type(pos, T_FIXNUM); /* 1 */
    rv = OCIParamGet(stmt->base.hp, OCI_HTYPE_STMT, oci8_errhp, (dvoid *)&parmhp, FIX2INT(pos));
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return oci8_param_create(parmhp, oci8_errhp);
}

static VALUE oci8_stmt_get_stmt_type(VALUE self)
{
    VALUE stmt_type = oci8_get_ub2_attr(DATA_PTR(self), OCI_ATTR_STMT_TYPE);
    switch (stmt_type) {
    case INT2FIX(OCI_STMT_SELECT):
        return oci8_sym_select_stmt;
    case INT2FIX(OCI_STMT_UPDATE):
        return oci8_sym_update_stmt;
    case INT2FIX(OCI_STMT_DELETE):
        return oci8_sym_delete_stmt;
    case INT2FIX(OCI_STMT_INSERT):
        return oci8_sym_insert_stmt;
    case INT2FIX(OCI_STMT_CREATE):
        return oci8_sym_create_stmt;
    case INT2FIX(OCI_STMT_DROP):
        return oci8_sym_drop_stmt;
    case INT2FIX(OCI_STMT_ALTER):
        return oci8_sym_alter_stmt;
    case INT2FIX(OCI_STMT_BEGIN):
        return oci8_sym_begin_stmt;
    case INT2FIX(OCI_STMT_DECLARE):
        return oci8_sym_declare_stmt;
    default:
        rb_bug("unexcepted statement type %d in OCIStmt#stmt_type", FIX2INT(stmt_type));
    }
}

static VALUE oci8_stmt_get_row_count(VALUE self)
{
    return oci8_get_ub4_attr(DATA_PTR(self), OCI_ATTR_ROW_COUNT);
}

static VALUE oci8_stmt_get_rowid(VALUE self)
{
    return oci8_get_rowid_attr(DATA_PTR(self), OCI_ATTR_ROWID);
}

static VALUE oci8_stmt_get_param_count(VALUE self)
{
    return oci8_get_ub4_attr(DATA_PTR(self), OCI_ATTR_PARAM_COUNT);
}

/*
 * bind_stmt
 */
static void bind_stmt_set(oci8_bind_t *bind, VALUE val)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)bind;
    oci8_base_t *h;
    if (!rb_obj_is_instance_of(val, cOCIStmt))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIStmt)", rb_class2name(CLASS_OF(val)));
    h = DATA_PTR(val);
    handle->hp = h->hp;
    handle->obj = val;
}

static void bind_stmt_init(oci8_bind_t *bind, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)bind;
    bind->valuep = &handle->hp;
    bind->value_sz = sizeof(handle->hp);
    if (NIL_P(*val)) {
        *val = rb_funcall(cOCIStmt, oci8_id_new, 0);
    }
}

static oci8_bind_class_t bind_stmt_class = {
    {
        oci8_bind_handle_mark,
	oci8_bind_free,
	sizeof(oci8_bind_handle_t)
    },
    oci8_bind_handle_get,
    bind_stmt_set,
    bind_stmt_init,
    SQLT_RSET
};

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
    cOCIStmt = oci8_define_class("OCIStmt", &oci8_stmt_class);

    oci8_sym_select_stmt = ID2SYM(rb_intern("select_stmt"));
    oci8_sym_update_stmt = ID2SYM(rb_intern("update_stmt"));
    oci8_sym_delete_stmt = ID2SYM(rb_intern("delete_stmt"));
    oci8_sym_insert_stmt = ID2SYM(rb_intern("insert_stmt"));
    oci8_sym_create_stmt = ID2SYM(rb_intern("create_stmt"));
    oci8_sym_drop_stmt = ID2SYM(rb_intern("drop_stmt"));
    oci8_sym_alter_stmt = ID2SYM(rb_intern("alter_stmt"));
    oci8_sym_begin_stmt = ID2SYM(rb_intern("begin_stmt"));
    oci8_sym_declare_stmt = ID2SYM(rb_intern("declare_stmt"));

    rb_define_method(cOCIStmt, "initialize", oci8_stmt_initialize, 0);
    rb_define_method(cOCIStmt, "prepare", oci8_stmt_prepare, 1);
    rb_define_method(cOCIStmt, "defineByPos", oci8_define_by_pos, 2);
    rb_define_method(cOCIStmt, "bind", oci8_bind, 2);
    rb_define_method(cOCIStmt, "execute", oci8_stmt_execute, 3);
    rb_define_method(cOCIStmt, "fetch", oci8_stmt_fetch, -1);
    rb_define_method(cOCIStmt, "paramGet", oci8_stmt_get_param, 1);
    rb_define_method(cOCIStmt, "stmt_type", oci8_stmt_get_stmt_type, 0);
    rb_define_method(cOCIStmt, "row_count", oci8_stmt_get_row_count, 0);
    rb_define_method(cOCIStmt, "rowid", oci8_stmt_get_rowid, 0);
    rb_define_method(cOCIStmt, "param_count", oci8_stmt_get_param_count, 0);

    oci8_define_bind_class("Cursor", &bind_stmt_class);
}
