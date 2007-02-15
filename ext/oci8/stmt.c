/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * stmt.c - part of ruby-oci8
 *         implement the methods of OCIStmt.
 *
 * Copyright (C) 2002-2007 KUBO Takehiro <kubo@jiubao.org>
 *
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
static VALUE oci8_sym_other;

VALUE cOCIStmt;

typedef struct {
    oci8_base_t base;
    VALUE svc;
    VALUE binds;
    VALUE defns;
} oci8_stmt_t;

static void oci8_stmt_mark(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    rb_gc_mark(stmt->svc);
    rb_gc_mark(stmt->binds);
    rb_gc_mark(stmt->defns);
}

static void oci8_stmt_free(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    stmt->svc = Qnil;
    stmt->binds = Qnil;
    stmt->defns = Qnil;
}

static oci8_base_class_t oci8_stmt_class = {
    oci8_stmt_mark,
    oci8_stmt_free,
    sizeof(oci8_stmt_t),
};

static VALUE oci8_stmt_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    VALUE svc;
    VALUE sql;
    sword rv;

    rb_scan_args(argc, argv, "11", &svc, &sql);

    if (argc > 1)
        StringValue(sql);

    rv = OCIHandleAlloc(oci8_envhp, &stmt->base.hp.ptr, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    stmt->base.type = OCI_HTYPE_STMT;
    stmt->svc = svc;
    stmt->binds = rb_hash_new();
    stmt->defns = rb_ary_new();
    rb_iv_set(stmt->base.self, "@names", rb_ary_new());
    rb_iv_set(stmt->base.self, "@con", svc);

    if (argc > 1) {
        rv = OCIStmtPrepare(stmt->base.hp.stmt, oci8_errhp, RSTRING_ORATEXT(sql), RSTRING_LEN(sql), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (IS_OCI_ERROR(rv)) {
            oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
        }
    }
    oci8_link_to_parent((oci8_base_t*)stmt, (oci8_base_t*)DATA_PTR(svc));
    return Qnil;
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
    oci8_bind_t *obind;
    oci8_bind_class_t *bind_class;
    sword status;
    ub4 mode;

    position = NUM2INT(vposition); /* 1 */
    obind = oci8_get_bind(vbindobj); /* 2 */
    if (obind->base.hp.dfn != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    bind_class = (oci8_bind_class_t *)obind->base.klass;
    if (bind_class->out == NULL) {
        mode = OCI_DEFAULT;
    } else {
        mode = OCI_DYNAMIC_FETCH;
    }
    status = OCIDefineByPos(stmt->base.hp.stmt, &obind->base.hp.dfn, oci8_errhp, position, obind->valuep, obind->value_sz, bind_class->dty, NIL_P(obind->tdo) ? obind->u.inds : NULL, NULL, 0, mode);
    if (status != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, status, stmt->base.hp.ptr);
    }
    obind->base.type = OCI_HTYPE_DEFINE;
    if (obind->maxar_sz > 0) {
        oci_lc(OCIDefineArrayOfStruct(obind->base.hp.dfn, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0));
    }
    if (RTEST(obind->tdo)) {
        oci8_base_t *tdo = DATA_PTR(obind->tdo);
        oci_lc(OCIDefineObject(obind->base.hp.dfn, oci8_errhp, tdo->hp.tdo,
                               obind->valuep, 0, obind->u.null_structs, 0));
    }
    if (position - 1 < RARRAY_LEN(stmt->defns)) {
        VALUE old_value = RARRAY_PTR(stmt->defns)[position - 1];
        if (!NIL_P(old_value)) {
            oci8_base_free((oci8_base_t*)oci8_get_bind(old_value));
        }
    }
    rb_ary_store(stmt->defns, position - 1, obind->base.self);
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);
    return obind->base.self;
}

static VALUE oci8_bind(VALUE self, VALUE vplaceholder, VALUE vbindobj)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    char *placeholder_ptr = (char*)-1; /* initialize as an invalid value */
    ub4 placeholder_len = 0;
    ub4 position = 0;
    oci8_bind_t *obind;
    oci8_bind_class_t *bind_class;
    sword status;
    VALUE old_value;
    void *indp;
    ub4 *curelep;
    ub4 mode;

    if (NIL_P(vplaceholder)) { /* 1 */
        placeholder_ptr = NULL;
        placeholder_len = 0;
    } else if (SYMBOL_P(vplaceholder)) {
        const char *symname = rb_id2name(SYM2ID(vplaceholder));
        size_t len = strlen(symname);
        placeholder_ptr = ALLOCA_N(char, len + 1);
        placeholder_len = len + 1;
        placeholder_ptr[0] = ':';
        memcpy(placeholder_ptr + 1, symname, len);
    } else if (FIXNUM_P(vplaceholder)) {
        position = NUM2INT(vplaceholder);
    } else {
        StringValue(vplaceholder);
        placeholder_ptr = RSTRING_PTR(vplaceholder);
        placeholder_len = RSTRING_LEN(vplaceholder);
    }
    obind = oci8_get_bind(vbindobj); /* 2 */
    if (obind->base.hp.bnd != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    bind_class = (oci8_bind_class_t *)obind->base.klass;
    if (bind_class->in != NULL || bind_class->out != NULL) {
        mode = OCI_DATA_AT_EXEC;
    } else {
        mode = OCI_DEFAULT;
    }

    indp = NIL_P(obind->tdo) ? obind->u.inds : NULL;
    if (obind->maxar_sz == 0) {
        curelep = NULL;
    } else {
        curelep = &obind->curar_sz;
    }
    if (placeholder_ptr == (char*)-1) {
        status = OCIBindByPos(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, position, obind->valuep, obind->value_sz, bind_class->dty, indp, NULL, 0, obind->maxar_sz, curelep, mode);
    } else {
        status = OCIBindByName(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, TO_ORATEXT(placeholder_ptr), placeholder_len, obind->valuep, obind->value_sz, bind_class->dty, indp, NULL, 0, obind->maxar_sz, curelep, mode);
    }
    if (status != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, status, stmt->base.hp.stmt);
    }
    obind->base.type = OCI_HTYPE_BIND;
    if (obind->maxar_sz > 0) {
        oci_lc(OCIBindArrayOfStruct(obind->base.hp.bnd, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0));
    }

    old_value = rb_hash_aref(stmt->binds, vplaceholder);
    if (!NIL_P(old_value)) {
        oci8_base_free((oci8_base_t*)oci8_get_bind(old_value));
    }
    rb_hash_aset(stmt->binds, vplaceholder, obind->base.self);
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);
    return obind->base.self;
}

static sword oci8_call_stmt_execute(oci8_svcctx_t *svcctx, oci8_stmt_t *stmt, ub4 iters, ub4 mode)
{
    sword rv;

    oci_rc2(rv, svcctx, OCIStmtExecute(svcctx->base.hp.svc, stmt->base.hp.stmt, oci8_errhp, iters, 0, NULL, NULL, mode));
    if (rv == OCI_ERROR) {
        sb4 errcode;
        OCIErrorGet(oci8_errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
        if (errcode == 1000) {
            /* run GC to close unreferred cursors
             * when ORA-01000 (maximum open cursors exceeded).
             */
            rb_gc();
            oci_rc2(rv, svcctx, OCIStmtExecute(svcctx->base.hp.svc, stmt->base.hp.stmt, oci8_errhp, iters, 0, NULL, NULL, mode));
        }
    }
    return rv;
}

static VALUE oci8_stmt_execute(VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(stmt->svc);
    ub4 iters;
    ub4 mode;
    sword rv;

    if (oci8_get_ub2_attr(&stmt->base, OCI_ATTR_STMT_TYPE) == INT2FIX(OCI_STMT_SELECT)) {
        iters = 0;
        mode = OCI_DEFAULT;
    } else {
        iters = 1;
        mode = svcctx->is_autocommit ? OCI_COMMIT_ON_SUCCESS : OCI_DEFAULT;
    }
    rv = oci8_call_stmt_execute(svcctx, stmt, iters, mode);
    while (rv == OCI_NEED_DATA) {
        oci8_bind_t *obind;
        oci8_bind_class_t *bind_class;
        /* get piece info. */
        dvoid *hp;
        ub4 type;
        ub1 in_out;
        ub4 iter;
        ub4 idx;
        ub1 piece;
        /* set piece info. */
        void *valuep;
        ub4 *alenp;
        void *indp;

        oci_lc(OCIStmtGetPieceInfo(stmt->base.hp.ptr, oci8_errhp, &hp, &type, &in_out, &iter, &idx, &piece));
        obind = (oci8_bind_t*)stmt->base.children;
        do {
            if (obind->base.hp.ptr == hp) {
                if (type != OCI_HTYPE_BIND)
                    rb_bug("ruby-oci8: expect OCI_HTYPE_BIND but %d", type);
                bind_class = (oci8_bind_class_t *)obind->base.klass;
                switch (in_out) {
                case OCI_PARAM_IN:
                    if (bind_class->in == NULL)
                        rb_bug("....");
                    piece = bind_class->in(obind, idx, piece, &valuep, &alenp, &indp);
                    break;
                case OCI_PARAM_OUT:
                    if (bind_class->out == NULL)
                        rb_bug("....");
                    bind_class->out(obind, idx, piece, &valuep, &alenp, &indp);
                    break;
                default:
                    rb_bug("ruby-oci8: expect OCI_PARAM_IN or OCI_PARAM_OUT but %d", in_out);
                }
                oci_lc(OCIStmtSetPieceInfo(obind->base.hp.ptr, OCI_HTYPE_BIND, oci8_errhp, valuep, alenp, 0, indp, NULL));
                break;
            }
            obind = (oci8_bind_t*)obind->base.next;
        } while (obind != (oci8_bind_t*)stmt->base.children);
        if (obind == (oci8_bind_t*)stmt) {
            rb_bug("ruby-oci8: No bind handle is found.");
        }
        rv = oci8_call_stmt_execute(svcctx, stmt, iters, mode);
    }
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
    }
    return self;
}

static VALUE oci8_stmt_do_fetch(oci8_stmt_t *stmt, oci8_svcctx_t *svcctx)
{
    VALUE ary;
    sword rv;
    long idx;
    oci8_bind_t *obind;
    oci8_bind_class_t *bind_class;

    oci_rc2(rv, svcctx, OCIStmtFetch(stmt->base.hp.stmt, oci8_errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT));
    while (rv == OCI_NEED_DATA) {
        /* get piece info. */
        dvoid *hp;
        ub4 type;
        ub1 in_out;
        ub4 iter;
        ub4 idx;
        ub1 piece;
        /* set piece info. */
        void *valuep;
        ub4 *alenp;
        void *indp;

        oci_lc(OCIStmtGetPieceInfo(stmt->base.hp.ptr, oci8_errhp, &hp, &type, &in_out, &iter, &idx, &piece));
        obind = (oci8_bind_t *)stmt->base.children;
        do {
            if (obind->base.hp.ptr == hp) {
                if (type != OCI_HTYPE_DEFINE)
                    rb_bug("ruby-oci8: expect OCI_HTYPE_DEFINE but %d", type);
                bind_class = (oci8_bind_class_t *)obind->base.klass;
                switch (in_out) {
                case OCI_PARAM_OUT:
                    if (bind_class->out == NULL)
                        rb_bug("....");
                    bind_class->out(obind, idx, piece == OCI_FIRST_PIECE, &valuep, &alenp, &indp);
                    break;
                default:
                    rb_bug("ruby-oci8: expect OCI_PARAM_OUT but %d", in_out);
                }
                oci_lc(OCIStmtSetPieceInfo(obind->base.hp.ptr, OCI_HTYPE_DEFINE, oci8_errhp, valuep, alenp, 0, indp, NULL));
                break;
            }
            obind = (oci8_bind_t *)obind->base.next;
        } while (obind != (oci8_bind_t*)stmt->base.children);
        if (obind == (oci8_bind_t*)stmt) {
            rb_bug("ruby-oci8: No define handle is found.");
        }
        oci_rc2(rv, svcctx, OCIStmtFetch(stmt->base.hp.stmt, oci8_errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT));
    }
    if (rv == OCI_NO_DATA) {
        return Qnil;
    }
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
    }
    obind = (oci8_bind_t *)stmt->base.children;
    do {
        /* set piece info. */
        void *valuep;
        ub4 *alenp;
        void *indp;

        if (obind->base.type != OCI_HTYPE_DEFINE)
            continue;
        bind_class = (oci8_bind_class_t *)obind->base.klass;
        if (bind_class->out != NULL) {
            if (obind->maxar_sz == 0) {
                bind_class->out(obind, 0, 0, &valuep, &alenp, &indp);
            } else {
                ub4 idx;
                for (idx = 0; idx < obind->curar_sz; idx++) {
                    bind_class->out(obind, idx, 0, &valuep, &alenp, &indp);
                }
            }
        }
        obind = (oci8_bind_t *)obind->base.next;
    } while (obind != (oci8_bind_t*)stmt->base.children);
    ary = rb_ary_new2(RARRAY_LEN(stmt->defns));
    for (idx = 0; idx < RARRAY_LEN(stmt->defns); idx++) {
        rb_ary_store(ary, idx, rb_funcall(RARRAY_PTR(stmt->defns)[idx], oci8_id_get, 0));
    }
    return ary;
}

/*
 * Gets fetched data as array. This is available for select
 * statement only.
 *
 * example:
 *   conn = OCI8.new('scott', 'tiger')
 *   cursor = conn.exec('SELECT * FROM emp')
 *   while r = cursor.fetch()
 *     puts r.join(',')
 *   end
 *   cursor.close
 *   conn.logoff
 */
static VALUE oci8_stmt_fetch(VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(stmt->svc);

    if (rb_block_given_p()) {
        for (;;) {
            VALUE rs = oci8_stmt_do_fetch(stmt, svcctx);
            if (NIL_P(rs))
                return self; /* NEED TO CHECK 0.1 behavior. */
            rb_yield(rs);
        }
    } else {
        return oci8_stmt_do_fetch(stmt, svcctx);
    }
}

static VALUE oci8_stmt_get_param(VALUE self, VALUE pos)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    OCIParam *parmhp = NULL;
    sword rv;

    Check_Type(pos, T_FIXNUM); /* 1 */
    rv = OCIParamGet(stmt->base.hp.stmt, OCI_HTYPE_STMT, oci8_errhp, (dvoid *)&parmhp, FIX2INT(pos));
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return oci8_metadata_create(parmhp, stmt->svc, Qnil);
}

/*
 * gets the type of SQL statement. Its value is one of the follows.
 * * OCI8::STMT_SELECT
 * * OCI8::STMT_UPDATE
 * * OCI8::STMT_DELETE
 * * OCI8::STMT_INSERT
 * * OCI8::STMT_CREATE
 * * OCI8::STMT_DROP
 * * OCI8::STMT_ALTER
 * * OCI8::STMT_BEGIN
 * * OCI8::STMT_DECLARE
 * For PL/SQL statement, it returns OCI8::STMT_BEGIN or
 * OCI8::STMT_DECLARE.
 */
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
    case INT2FIX(0):
        return oci8_sym_other;
    default:
        rb_bug("unexcepted statement type %d in OCIStmt#stmt_type", FIX2INT(stmt_type));
    }
}

/*
 * Returns the number of processed rows.
 */
static VALUE oci8_stmt_get_row_count(VALUE self)
{
    return oci8_get_ub4_attr(DATA_PTR(self), OCI_ATTR_ROW_COUNT);
}

/*
 * get the rowid of the last processed row.
 * This value is available as bind data.
 * On the other hand it isn't available for other purpose.
 */
static VALUE oci8_stmt_get_rowid(VALUE self)
{
    return oci8_get_rowid_attr(DATA_PTR(self), OCI_ATTR_ROWID);
}

static VALUE oci8_stmt_get_param_count(VALUE self)
{
    return oci8_get_ub4_attr(DATA_PTR(self), OCI_ATTR_PARAM_COUNT);
}

static VALUE oci8_stmt_connection(VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    return stmt->svc;
}

/*
 * Gets the value of the bind variable.
 *
 * In case of binding explicitly, use same key with that of
 * OCI8::Cursor#bind_param. A placeholder can be bound by 
 * name or position. If you bind by name, use that name. If you bind
 * by position, use the position.
 *
 * example:
 *   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
 *   cursor.bind_param(':out', 'FOO') # bind by name
 *   p cursor[':out'] # => 'FOO'
 *   p cursor[1] # => nil
 *   cursor.exec()
 *   p cursor[':out'] # => 'BAR'
 *   p cursor[1] # => nil
 *
 * example:
 *   cursor = conn.parse("BEGIN :out := 'BAR'; END;")
 *   cursor.bind_param(1, 'FOO') # bind by position
 *   p cursor[':out'] # => nil
 *   p cursor[1] # => 'FOO'
 *   cursor.exec()
 *   p cursor[':out'] # => nil
 *   p cursor[1] # => 'BAR'
 *
 * In case of binding by OCI8#exec or OCI8::Cursor#exec,
 * get the value by position, which starts from 1.
 *
 * example:
 *   cursor = conn.exec("BEGIN :out := 'BAR'; END;", 'FOO')
 *   # 1st bind variable is bound as String with width 3. Its initial value is 'FOO'
 *   # After execute, the value become 'BAR'.
 *   p cursor[1] # => 'BAR'
 */
static VALUE oci8_stmt_aref(VALUE self, VALUE key)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    VALUE obj = rb_hash_aref(stmt->binds, key);
    if (NIL_P(obj)) {
        return Qnil;
    }
    return rb_funcall(obj, oci8_id_get, 0);
}

/*
 * Sets the value to the bind variable. The way to specify the
 * +key+ is same with OCI8::Cursor#[]. This is available
 * to replace the value and execute many times.
 *
 * example1:
 *   cursor = conn.parse("INSERT INTO test(col1) VALUES(:1)")
 *   cursor.bind_params(1, nil, String, 3)
 *   ['FOO', 'BAR', 'BAZ'].each do |key|
 *     cursor[1] = key
 *     cursor.exec
 *   end
 *   cursor.close()
 *
 * example2:
 *   ['FOO', 'BAR', 'BAZ'].each do |key|
 *     conn.exec("INSERT INTO test(col1) VALUES(:1)", key)
 *   end
 *
 * Both example's results are same. But the former will use less resources.
 */
static VALUE oci8_stmt_aset(VALUE self, VALUE key, VALUE val)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    VALUE obj = rb_hash_aref(stmt->binds, key);
    if (NIL_P(obj)) {
        return Qnil; /* ?? MUST BE ERROR? */
    }
    return rb_funcall(obj, oci8_id_set, 1, val);
}

/*
 * Returns the keys of bind variables as array.
 */
static VALUE oci8_stmt_keys(VALUE self)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    return rb_funcall(stmt->binds, oci8_id_keys, 0);
}

static VALUE oci8_stmt_defined_p(VALUE self, VALUE pos)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    long position = NUM2INT(pos);

    if (position - 1 < RARRAY_LEN(stmt->defns)) {
        VALUE value = RARRAY_PTR(stmt->defns)[position - 1];
        if (!NIL_P(value)) {
            return Qtrue;
        }
    }
    return Qfalse;
}

static VALUE oci8_stmt_set_prefetch_rows(VALUE self, VALUE rows)
{
    oci8_stmt_t *stmt = DATA_PTR(self);
    ub4 num = NUM2UINT(rows);

    oci_lc(OCIAttrSet(stmt->base.hp.ptr, OCI_HTYPE_STMT, &num, 0, OCI_ATTR_PREFETCH_ROWS, oci8_errhp));
    return Qfalse;
}

/*
 * bind_stmt
 */
VALUE oci8_stmt_get(oci8_bind_t *obind, void *data, void *null_structs)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    rb_funcall(oho->obj, rb_intern("define_columns"), 0);
    return oho->obj;
}

static void bind_stmt_set(oci8_bind_t *obind, void *data, void *null_structs, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    oci8_base_t *h;
    if (!rb_obj_is_instance_of(val, cOCIStmt))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIStmt)", rb_class2name(CLASS_OF(val)));
    h = DATA_PTR(val);
    oho->hp = h->hp.ptr;
    oho->obj = val;
}

static void bind_stmt_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    obind->value_sz = sizeof(void *);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);
    if (NIL_P(*val)) {
        *val = rb_funcall(cOCIStmt, oci8_id_new, 1, svc);
    }
}

static void bind_stmt_init_elem(oci8_bind_t *obind, VALUE svc)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_base_t *h;
    ub4 idx = 0;

    do {
        oho[idx].obj = rb_funcall(cOCIStmt, oci8_id_new, 1, svc);
        h = DATA_PTR(oho[idx].obj);
        oho[idx].hp = h->hp.ptr;
    } while (++idx < obind->maxar_sz);
}

static oci8_bind_class_t bind_stmt_class = {
    {
        oci8_bind_hp_obj_mark,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    oci8_stmt_get,
    bind_stmt_set,
    bind_stmt_init,
    bind_stmt_init_elem,
    NULL,
    NULL,
    SQLT_RSET
};

void Init_oci8_stmt(VALUE cOCI8)
{
    cOCIStmt = oci8_define_class_under(cOCI8, "Cursor", &oci8_stmt_class);

    oci8_sym_select_stmt = ID2SYM(rb_intern("select_stmt"));
    oci8_sym_update_stmt = ID2SYM(rb_intern("update_stmt"));
    oci8_sym_delete_stmt = ID2SYM(rb_intern("delete_stmt"));
    oci8_sym_insert_stmt = ID2SYM(rb_intern("insert_stmt"));
    oci8_sym_create_stmt = ID2SYM(rb_intern("create_stmt"));
    oci8_sym_drop_stmt = ID2SYM(rb_intern("drop_stmt"));
    oci8_sym_alter_stmt = ID2SYM(rb_intern("alter_stmt"));
    oci8_sym_begin_stmt = ID2SYM(rb_intern("begin_stmt"));
    oci8_sym_declare_stmt = ID2SYM(rb_intern("declare_stmt"));
    oci8_sym_other = ID2SYM(rb_intern("other"));

    rb_define_private_method(cOCIStmt, "initialize", oci8_stmt_initialize, -1);
    rb_define_private_method(cOCIStmt, "__defineByPos", oci8_define_by_pos, 2);
    rb_define_private_method(cOCIStmt, "__bind", oci8_bind, 2);
    rb_define_private_method(cOCIStmt, "__execute", oci8_stmt_execute, 0);
    rb_define_method(cOCIStmt, "fetch", oci8_stmt_fetch, 0);
    rb_define_private_method(cOCIStmt, "__paramGet", oci8_stmt_get_param, 1);
    rb_define_method(cOCIStmt, "type", oci8_stmt_get_stmt_type, 0);
    rb_define_method(cOCIStmt, "row_count", oci8_stmt_get_row_count, 0);
    rb_define_method(cOCIStmt, "rowid", oci8_stmt_get_rowid, 0);
    rb_define_private_method(cOCIStmt, "__param_count", oci8_stmt_get_param_count, 0);
    rb_define_private_method(cOCIStmt, "__connection", oci8_stmt_connection, 0);
    rb_define_method(cOCIStmt, "[]", oci8_stmt_aref, 1);
    rb_define_method(cOCIStmt, "[]=", oci8_stmt_aset, 2);
    rb_define_method(cOCIStmt, "keys", oci8_stmt_keys, 0);
    rb_define_private_method(cOCIStmt, "__defiend?", oci8_stmt_defined_p, 1);
    rb_define_method(cOCIStmt, "prefetch_rows=", oci8_stmt_set_prefetch_rows, 1);

    oci8_define_bind_class("Cursor", &bind_stmt_class);
}
