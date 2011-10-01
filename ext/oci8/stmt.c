/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * stmt.c - part of ruby-oci8
 *         implement the methods of OCIStmt.
 *
 * Copyright (C) 2002-2010 KUBO Takehiro <kubo@jiubao.org>
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
static ID id_at_column_metadata;
static ID id_at_actual_array_size;
static ID id_at_max_array_size;
static ID id_each_value;
static ID id_at_names;
static ID id_empty_p;
static ID id_at_con;
static ID id_clear;

VALUE cOCIStmt;

#define TO_STMT(obj) ((oci8_stmt_t *)oci8_get_handle((obj), cOCIStmt))

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

static oci8_base_vtable_t oci8_stmt_vtable = {
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

    oci8_check_pid_consistency(oci8_get_svcctx(svc));
    if (argc > 1)
        OCI8SafeStringValue(sql);

    rv = OCIHandleAlloc(oci8_envhp, &stmt->base.hp.ptr, OCI_HTYPE_STMT, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    stmt->base.type = OCI_HTYPE_STMT;
    stmt->svc = svc;
    stmt->binds = rb_hash_new();
    stmt->defns = rb_ary_new();
    rb_ivar_set(stmt->base.self, id_at_column_metadata, rb_ary_new());
    rb_ivar_set(stmt->base.self, id_at_names, Qnil);
    rb_ivar_set(stmt->base.self, id_at_con, svc);
    rb_ivar_set(stmt->base.self, id_at_max_array_size, Qnil);

    if (argc > 1) {
        rv = OCIStmtPrepare(stmt->base.hp.stmt, oci8_errhp, RSTRING_ORATEXT(sql), RSTRING_LEN(sql), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (IS_OCI_ERROR(rv)) {
            oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
        }
    }
    oci8_link_to_parent((oci8_base_t*)stmt, (oci8_base_t*)DATA_PTR(svc));
    return Qnil;
}

static VALUE oci8_define_by_pos(VALUE self, VALUE vposition, VALUE vbindobj)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    ub4 position;
    oci8_bind_t *obind;
    const oci8_bind_vtable_t *vptr;
    sword status;

    position = NUM2INT(vposition); /* 1 */
    obind = oci8_get_bind(vbindobj); /* 2 */
    if (obind->base.hp.dfn != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    vptr = (const oci8_bind_vtable_t *)obind->base.vptr;
    status = OCIDefineByPos(stmt->base.hp.stmt, &obind->base.hp.dfn, oci8_errhp, position, obind->valuep, obind->value_sz, vptr->dty, NIL_P(obind->tdo) ? obind->u.inds : NULL, NULL, 0, OCI_DEFAULT);
    if (status != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, status, stmt->base.hp.ptr);
    }
    obind->base.type = OCI_HTYPE_DEFINE;
    /* link to the parent as soon as possible to preserve deallocation order. */
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);

    if (NIL_P(obind->tdo) && obind->maxar_sz > 0) {
        oci_lc(OCIDefineArrayOfStruct(obind->base.hp.dfn, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0));
    }
    if (vptr->post_bind_hook != NULL) {
        vptr->post_bind_hook(obind);
    }
    if (position - 1 < RARRAY_LEN(stmt->defns)) {
        VALUE old_value = RARRAY_PTR(stmt->defns)[position - 1];
        if (!NIL_P(old_value)) {
            oci8_base_free((oci8_base_t*)oci8_get_bind(old_value));
        }
    }
    rb_ary_store(stmt->defns, position - 1, obind->base.self);
    return obind->base.self;
}

static VALUE oci8_bind(VALUE self, VALUE vplaceholder, VALUE vbindobj)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    char *placeholder_ptr = (char*)-1; /* initialize as an invalid value */
    ub4 placeholder_len = 0;
    ub4 position = 0;
    oci8_bind_t *obind;
    const oci8_bind_vtable_t *vptr;
    sword status;
    VALUE old_value;
    void *indp;
    ub4 *curelep;

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
        OCI8StringValue(vplaceholder);
        placeholder_ptr = RSTRING_PTR(vplaceholder);
        placeholder_len = RSTRING_LEN(vplaceholder);
    }
    obind = oci8_get_bind(vbindobj); /* 2 */
    if (obind->base.hp.bnd != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    vptr = (const oci8_bind_vtable_t *)obind->base.vptr;

    indp = NIL_P(obind->tdo) ? obind->u.inds : NULL;
    if (obind->maxar_sz == 0) {
        curelep = NULL;
    } else {
        curelep = &obind->curar_sz;
    }
    if (placeholder_ptr == (char*)-1) {
        status = OCIBindByPos(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, position, obind->valuep, obind->value_sz, vptr->dty, indp, NULL, 0, 0, 0, OCI_DEFAULT);
    } else {
        status = OCIBindByName(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, TO_ORATEXT(placeholder_ptr), placeholder_len, obind->valuep, obind->value_sz, vptr->dty, indp, NULL, 0, 0, 0, OCI_DEFAULT);
    }
    if (status != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, status, stmt->base.hp.stmt);
    }
    obind->base.type = OCI_HTYPE_BIND;
    /* link to the parent as soon as possible to preserve deallocation order. */
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);

    if (NIL_P(obind->tdo) && obind->maxar_sz > 0) {
        oci_lc(OCIBindArrayOfStruct(obind->base.hp.bnd, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0));
    }
    if (vptr->post_bind_hook != NULL) {
        vptr->post_bind_hook(obind);
    }
    old_value = rb_hash_aref(stmt->binds, vplaceholder);
    if (!NIL_P(old_value)) {
        oci8_base_free((oci8_base_t*)oci8_get_bind(old_value));
    }
    rb_hash_aset(stmt->binds, vplaceholder, obind->base.self);
    return obind->base.self;
}

static sword oci8_call_stmt_execute(oci8_svcctx_t *svcctx, oci8_stmt_t *stmt, ub4 iters, ub4 mode)
{
    sword rv;

    rv = OCIStmtExecute_nb(svcctx, svcctx->base.hp.svc, stmt->base.hp.stmt, oci8_errhp, iters, 0, NULL, NULL, mode);
    if (rv == OCI_ERROR) {
        if (oci8_get_error_code(oci8_errhp) == 1000) {
            /* run GC to close unreferred cursors
             * when ORA-01000 (maximum open cursors exceeded).
             */
            rb_gc();
            rv = OCIStmtExecute_nb(svcctx, svcctx->base.hp.svc, stmt->base.hp.stmt, oci8_errhp, iters, 0, NULL, NULL, mode);
        }
    }
    return rv;
}

static VALUE oci8_stmt_execute(VALUE self, VALUE iteration_count)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(stmt->svc);
    ub4 iters;
    ub4 mode;
    sword rv;

    if (oci8_get_ub2_attr(&stmt->base, OCI_ATTR_STMT_TYPE) == INT2FIX(OCI_STMT_SELECT)) {
        iters = 0;
        mode = OCI_DEFAULT;
    } else {
        if(!NIL_P(iteration_count)) 
            iters = NUM2INT(iteration_count);
        else 
            iters = 1;
        mode = svcctx->is_autocommit ? OCI_COMMIT_ON_SUCCESS : OCI_DEFAULT;
    }
    rv = oci8_call_stmt_execute(svcctx, stmt, iters, mode);
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
    }
    return self;
}

static VALUE each_value(VALUE obj)
{
    return rb_funcall(obj, id_each_value, 0);
}

static VALUE clear_binds_iterator_proc(VALUE val, VALUE arg)
{
    if(!NIL_P(val)) {
        oci8_base_free((oci8_base_t*)oci8_get_bind(val));
    }
    return Qnil;
}

static VALUE oci8_stmt_clear_binds(VALUE self)
{
    oci8_stmt_t *stmt = TO_STMT(self);

    if(!RTEST(rb_funcall(stmt->binds, id_empty_p, 0)))
    {
        rb_iterate(each_value, stmt->binds, clear_binds_iterator_proc, Qnil);
        rb_funcall(stmt->binds,id_clear,0);
    }

    return self;
}

static VALUE oci8_stmt_do_fetch(oci8_stmt_t *stmt, oci8_svcctx_t *svcctx)
{
    VALUE ary;
    sword rv;
    long idx;
    oci8_bind_t *obind;
    const oci8_bind_vtable_t *vptr;

    if (stmt->base.children != NULL) {
        obind = (oci8_bind_t *)stmt->base.children;
        do {
            if (obind->base.type == OCI_HTYPE_DEFINE) {
                vptr = (const oci8_bind_vtable_t *)obind->base.vptr;
                if (vptr->pre_fetch_hook != NULL) {
                    vptr->pre_fetch_hook(obind, stmt->svc);
                }
            }
            obind = (oci8_bind_t *)obind->base.next;
        } while (obind != (oci8_bind_t*)stmt->base.children);
    }
    rv = OCIStmtFetch_nb(svcctx, stmt->base.hp.stmt, oci8_errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
    if (rv == OCI_NO_DATA) {
        return Qnil;
    }
    if (IS_OCI_ERROR(rv)) {
        oci8_raise(oci8_errhp, rv, stmt->base.hp.stmt);
    }
    ary = rb_ary_new2(RARRAY_LEN(stmt->defns));
    for (idx = 0; idx < RARRAY_LEN(stmt->defns); idx++) {
        rb_ary_store(ary, idx, oci8_bind_get_data(RARRAY_PTR(stmt->defns)[idx]));
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
    oci8_stmt_t *stmt = TO_STMT(self);
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
    oci8_stmt_t *stmt = TO_STMT(self);
    OCIParam *parmhp = NULL;
    sword rv;

    Check_Type(pos, T_FIXNUM); /* 1 */
    rv = OCIParamGet(stmt->base.hp.stmt, OCI_HTYPE_STMT, oci8_errhp, (dvoid *)&parmhp, FIX2INT(pos));
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return oci8_metadata_create(parmhp, stmt->svc, self);
}

/*
 * gets the type of SQL statement as follows.
 * * OCI8::STMT_SELECT
 * * OCI8::STMT_UPDATE
 * * OCI8::STMT_DELETE
 * * OCI8::STMT_INSERT
 * * OCI8::STMT_CREATE
 * * OCI8::STMT_DROP
 * * OCI8::STMT_ALTER
 * * OCI8::STMT_BEGIN (PL/SQL block which starts with a BEGIN keyword)
 * * OCI8::STMT_DECLARE (PL/SQL block which starts with a DECLARE keyword)
 * * Other Fixnum value undocumented in Oracle manuals.
 *
 * <em>Changes between ruby-oci8 1.0 and 2.0.</em>
 *
 * [ruby-oci8 2.0] OCI8::STMT_* are Symbols. (:select_stmt, :update_stmt, etc.)
 * [ruby-oci8 1.0] OCI8::STMT_* are Fixnums. (1, 2, 3, etc.)
 */
static VALUE oci8_stmt_get_stmt_type(VALUE self)
{
    VALUE stmt_type = oci8_get_ub2_attr(oci8_get_handle(self, cOCIStmt), OCI_ATTR_STMT_TYPE);
    switch (FIX2INT(stmt_type)) {
    case OCI_STMT_SELECT:
        return oci8_sym_select_stmt;
    case OCI_STMT_UPDATE:
        return oci8_sym_update_stmt;
    case OCI_STMT_DELETE:
        return oci8_sym_delete_stmt;
    case OCI_STMT_INSERT:
        return oci8_sym_insert_stmt;
    case OCI_STMT_CREATE:
        return oci8_sym_create_stmt;
    case OCI_STMT_DROP:
        return oci8_sym_drop_stmt;
    case OCI_STMT_ALTER:
        return oci8_sym_alter_stmt;
    case OCI_STMT_BEGIN:
        return oci8_sym_begin_stmt;
    case OCI_STMT_DECLARE:
        return oci8_sym_declare_stmt;
    default:
        return stmt_type;
    }
}

/*
 * Returns the number of processed rows.
 */
static VALUE oci8_stmt_get_row_count(VALUE self)
{
    return oci8_get_ub4_attr(oci8_get_handle(self, cOCIStmt), OCI_ATTR_ROW_COUNT);
}

/*
 * Get the rowid of the last inserted/updated/deleted row.
 * This cannot be used for select statements.
 *
 * example:
 *   cursor = conn.parse('INSERT INTO foo_table values(:1, :2)', 1, 2)
 *   cursor.exec
 *   cursor.rowid # => the inserted row's rowid
 *
 * <em>Changes between ruby-oci8 1.0.3 and 1.0.4.</em>
 *
 * [ruby-oci8 1.0.4 or upper] The return value is a String.
 * [ruby-oci8 1.0.3 or lower] It returns an OCIRowid object.
 */
static VALUE oci8_stmt_get_rowid(VALUE self)
{
    return oci8_get_rowid_attr(oci8_get_handle(self, cOCIStmt), OCI_ATTR_ROWID);
}

static VALUE oci8_stmt_get_param_count(VALUE self)
{
    return oci8_get_ub4_attr(oci8_get_handle(self, cOCIStmt), OCI_ATTR_PARAM_COUNT);
}

/*
 * call-seq:
 *   [key]
 *
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
    oci8_stmt_t *stmt = TO_STMT(self);
    VALUE obj = rb_hash_aref(stmt->binds, key);
    if (NIL_P(obj)) {
        return Qnil;
    }
    return oci8_bind_get_data(obj);
}

/*
 * call-seq:
 *   [key] = val
 *
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
    long max_array_size;
    long actual_array_size;
    long bind_array_size;

    oci8_stmt_t *stmt = TO_STMT(self);
    VALUE obj = rb_hash_aref(stmt->binds, key);
    if (NIL_P(obj)) {
        return Qnil; /* ?? MUST BE ERROR? */
    }

    if(TYPE(val) == T_ARRAY) {
        max_array_size = NUM2INT(rb_ivar_get(self, id_at_max_array_size));
        actual_array_size = NUM2INT(rb_ivar_get(self, id_at_actual_array_size));
        bind_array_size = RARRAY_LEN(val);

        if(actual_array_size > 0 && bind_array_size != actual_array_size) {
            rb_raise(rb_eRuntimeError, "all binding arrays hould be the same size");        
        }
        if(bind_array_size <= max_array_size && actual_array_size == 0) {
            rb_ivar_set(self, id_at_actual_array_size, INT2NUM(bind_array_size));
        }
    }
    oci8_bind_set_data(obj, val);
    return val;
}

/*
 * call-seq:
 *   keys -> an Array
 *
 * Returns the keys of bind variables as array.
 */
static VALUE oci8_stmt_keys(VALUE self)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    return rb_funcall(stmt->binds, oci8_id_keys, 0);
}

static VALUE oci8_stmt_defined_p(VALUE self, VALUE pos)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    long position = NUM2INT(pos);

    if (position - 1 < RARRAY_LEN(stmt->defns)) {
        VALUE value = RARRAY_PTR(stmt->defns)[position - 1];
        if (!NIL_P(value)) {
            return Qtrue;
        }
    }
    return Qfalse;
}

/*
 * call-seq:
 *   prefetch_rows = aFixnum
 *
 * Set number of rows to be prefetched.
 * This can reduce the number of network round trips when fetching
 * many rows. The default value is one.
 *
 * FYI: Rails oracle adaptor uses 100 by default.
 */
static VALUE oci8_stmt_set_prefetch_rows(VALUE self, VALUE rows)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    ub4 num = NUM2UINT(rows);

    oci_lc(OCIAttrSet(stmt->base.hp.ptr, OCI_HTYPE_STMT, &num, 0, OCI_ATTR_PREFETCH_ROWS, oci8_errhp));
    return Qfalse;
}

/*
 * bind_stmt
 */
VALUE oci8_stmt_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    rb_funcall(oho->obj, rb_intern("define_columns"), 0);
    return oho->obj;
}

static void bind_stmt_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    oci8_base_t *h;
    if (!rb_obj_is_instance_of(val, cOCIStmt))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIStmt)", rb_class2name(CLASS_OF(val)));
    h = DATA_PTR(val);
    oho->hp = h->hp.ptr;
    oho->obj = val;
}

static void bind_stmt_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(void *);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);
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

static const oci8_bind_vtable_t bind_stmt_vtable = {
    {
        oci8_bind_hp_obj_mark,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    oci8_stmt_get,
    bind_stmt_set,
    bind_stmt_init,
    bind_stmt_init_elem,
    bind_stmt_init_elem,
    SQLT_RSET
};

void Init_oci8_stmt(VALUE cOCI8)
{
#if 0
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
    cOCIStmt = rb_define_class_under(cOCI8, "Cursor", cOCIHandle);
#endif
    cOCIStmt = oci8_define_class_under(cOCI8, "Cursor", &oci8_stmt_vtable);

    oci8_sym_select_stmt = ID2SYM(rb_intern("select_stmt"));
    oci8_sym_update_stmt = ID2SYM(rb_intern("update_stmt"));
    oci8_sym_delete_stmt = ID2SYM(rb_intern("delete_stmt"));
    oci8_sym_insert_stmt = ID2SYM(rb_intern("insert_stmt"));
    oci8_sym_create_stmt = ID2SYM(rb_intern("create_stmt"));
    oci8_sym_drop_stmt = ID2SYM(rb_intern("drop_stmt"));
    oci8_sym_alter_stmt = ID2SYM(rb_intern("alter_stmt"));
    oci8_sym_begin_stmt = ID2SYM(rb_intern("begin_stmt"));
    oci8_sym_declare_stmt = ID2SYM(rb_intern("declare_stmt"));
    id_at_column_metadata = rb_intern("@column_metadata");
    id_at_actual_array_size = rb_intern("@actual_array_size");
    id_at_max_array_size = rb_intern("@max_array_size");
    id_each_value = rb_intern("each_value");
    id_at_names = rb_intern("@names");
    id_at_con = rb_intern("@con");
    id_empty_p = rb_intern("empty?");
    id_clear = rb_intern("clear");

    rb_define_private_method(cOCIStmt, "initialize", oci8_stmt_initialize, -1);
    rb_define_private_method(cOCIStmt, "__define", oci8_define_by_pos, 2);
    rb_define_private_method(cOCIStmt, "__bind", oci8_bind, 2);
    rb_define_private_method(cOCIStmt, "__execute", oci8_stmt_execute, 1);
    rb_define_private_method(cOCIStmt, "__clearBinds", oci8_stmt_clear_binds, 0);
    rb_define_method(cOCIStmt, "fetch", oci8_stmt_fetch, 0);
    rb_define_private_method(cOCIStmt, "__paramGet", oci8_stmt_get_param, 1);
    rb_define_method(cOCIStmt, "type", oci8_stmt_get_stmt_type, 0);
    rb_define_method(cOCIStmt, "row_count", oci8_stmt_get_row_count, 0);
    rb_define_method(cOCIStmt, "rowid", oci8_stmt_get_rowid, 0);
    rb_define_private_method(cOCIStmt, "__param_count", oci8_stmt_get_param_count, 0);
    rb_define_method(cOCIStmt, "[]", oci8_stmt_aref, 1);
    rb_define_method(cOCIStmt, "[]=", oci8_stmt_aset, 2);
    rb_define_method(cOCIStmt, "keys", oci8_stmt_keys, 0);
    rb_define_private_method(cOCIStmt, "__defined?", oci8_stmt_defined_p, 1);
    rb_define_method(cOCIStmt, "prefetch_rows=", oci8_stmt_set_prefetch_rows, 1);

    oci8_define_bind_class("Cursor", &bind_stmt_vtable);
}
