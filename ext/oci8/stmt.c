/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * stmt.c - part of ruby-oci8
 *         implement the methods of OCIStmt.
 *
 * Copyright (C) 2002-2015 Kubo Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

static VALUE cOCIStmt;

#define TO_STMT(obj) ((oci8_stmt_t *)oci8_check_typeddata((obj), &oci8_stmt_data_type, 1))

typedef struct {
    oci8_base_t base;
    VALUE svc;
    int use_stmt_release;
} oci8_stmt_t;

static void oci8_stmt_mark(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    rb_gc_mark(stmt->svc);
}

static void oci8_stmt_free(oci8_base_t *base)
{
    oci8_stmt_t *stmt = (oci8_stmt_t *)base;
    stmt->svc = Qnil;
    if (stmt->use_stmt_release) {
        OCIStmtRelease(base->hp.stmt, oci8_errhp, NULL, 0, OCI_DEFAULT);
        base->type = 0;
        base->closed = 1;
        stmt->use_stmt_release = 0;
    }
}

static const oci8_handle_data_type_t oci8_stmt_data_type = {
    {
        "OCI8::Cursor",
        {
            (RUBY_DATA_FUNC)oci8_stmt_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_stmt_free,
    sizeof(oci8_stmt_t),
};

static VALUE oci8_stmt_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_stmt_data_type);
}

/*
 * @overload __initialize(connection, sql_statement)
 *
 *  @param [OCI8] connection
 *  @param [String] sql_statement
 *
 *  @private
 */
static VALUE oci8_stmt_initialize(VALUE self, VALUE svc, VALUE sql)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    oci8_svcctx_t *svcctx;
    sword rv;

    svcctx = oci8_get_svcctx(svc);
    oci8_check_pid_consistency(svcctx);
    if (!NIL_P(sql) && oracle_client_version >= ORAVER_9_2) {
        OCI8SafeStringValue(sql);

        rv = OCIStmtPrepare2_nb(svcctx, svcctx->base.hp.svc, &stmt->base.hp.stmt, oci8_errhp, RSTRING_ORATEXT(sql), RSTRING_LEN(sql), NULL, 0, OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (IS_OCI_ERROR(rv)) {
            chker2(rv, &svcctx->base);
        }
        stmt->base.type = OCI_HTYPE_STMT;
        stmt->use_stmt_release = 1;
    } else {
        rv = OCIHandleAlloc(oci8_envhp, &stmt->base.hp.ptr, OCI_HTYPE_STMT, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_env_raise(oci8_envhp, rv);
        }
        stmt->base.type = OCI_HTYPE_STMT;
        if (!NIL_P(sql)) {
            OCI8SafeStringValue(sql);
            rv = OCIStmtPrepare(stmt->base.hp.stmt, oci8_errhp, RSTRING_ORATEXT(sql), RSTRING_LEN(sql), OCI_NTV_SYNTAX, OCI_DEFAULT);
            if (IS_OCI_ERROR(rv)) {
                chker3(rv, &svcctx->base, stmt->base.hp.stmt);
            }
        }
    }
    RB_OBJ_WRITE(stmt->base.self, &stmt->svc, svc);

    oci8_link_to_parent(&stmt->base, &svcctx->base);
    return Qnil;
}

/*
 * @overload __define(position, bindobj)
 *
 *  @param [Integer] position
 *  @param [OCI8::BindType::Base] bindobj
 *
 *  @private
 */
static VALUE oci8_define_by_pos(VALUE self, VALUE vposition, VALUE vbindobj)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    ub4 position = NUM2INT(vposition);
    oci8_bind_t *obind = TO_BIND(vbindobj);
    const oci8_bind_data_type_t *data_type;
    sword status;

    if (obind->base.hp.dfn != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    data_type = (const oci8_bind_data_type_t *)obind->base.data_type;
    status = OCIDefineByPos(stmt->base.hp.stmt, &obind->base.hp.dfn, oci8_errhp, position, obind->valuep, obind->value_sz, data_type->dty, NIL_P(obind->tdo) ? obind->u.inds : NULL, NULL, 0, OCI_DEFAULT);
    if (status != OCI_SUCCESS) {
        chker3(status, &stmt->base, stmt->base.hp.ptr);
    }
    obind->base.type = OCI_HTYPE_DEFINE;
    /* link to the parent as soon as possible to preserve deallocation order. */
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);

    if (NIL_P(obind->tdo) && obind->maxar_sz > 0) {
        chker2(OCIDefineArrayOfStruct(obind->base.hp.dfn, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0), &stmt->base);
    }
    if (data_type->post_bind_hook != NULL) {
        data_type->post_bind_hook(obind);
    }
    return obind->base.self;
}

/*
 * @overload __bind(placeholder, bindobj)
 *
 *  @param [Integer, String or Symbol] placeholder
 *  @param [OCI8::BindType::Base] bindobj
 *
 *  @private
 */
static VALUE oci8_bind(VALUE self, VALUE vplaceholder, VALUE vbindobj)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    char *placeholder_ptr = (char*)-1; /* initialize as an invalid value */
    ub4 placeholder_len = 0;
    ub4 position = 0;
    oci8_bind_t *obind;
    const oci8_bind_data_type_t *data_type;
    sword status;
    void *indp;

    if (NIL_P(vplaceholder)) { /* 1 */
        placeholder_ptr = NULL;
        placeholder_len = 0;
    } else if (SYMBOL_P(vplaceholder)) {
#ifdef HAVE_RB_SYM2STR
        /* Don't use SYM2ID on ruby 2.2.0 or later.
         * Symbols passed to SYM2ID are not GC'ed.
         */
        VALUE symval = rb_sym2str(vplaceholder);
        const char *symname = RSTRING_PTR(symval);
        size_t len = RSTRING_LEN(symval);
#else
        const char *symname = rb_id2name(SYM2ID(vplaceholder));
        size_t len = strlen(symname);
#endif
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
    obind = TO_BIND(vbindobj); /* 2 */
    if (obind->base.hp.bnd != NULL) {
        oci8_base_free(&obind->base); /* TODO: OK? */
    }
    data_type = (const oci8_bind_data_type_t *)obind->base.data_type;

    indp = NIL_P(obind->tdo) ? obind->u.inds : NULL;
    if (placeholder_ptr == (char*)-1) {
        status = OCIBindByPos(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, position, obind->valuep, obind->value_sz, data_type->dty, indp, NULL, 0, 0, 0, OCI_DEFAULT);
    } else {
        status = OCIBindByName(stmt->base.hp.stmt, &obind->base.hp.bnd, oci8_errhp, TO_ORATEXT(placeholder_ptr), placeholder_len, obind->valuep, obind->value_sz, data_type->dty, indp, NULL, 0, 0, 0, OCI_DEFAULT);
    }
    if (status != OCI_SUCCESS) {
        chker3(status, &stmt->base, stmt->base.hp.stmt);
    }
    obind->base.type = OCI_HTYPE_BIND;
    /* link to the parent as soon as possible to preserve deallocation order. */
    oci8_unlink_from_parent((oci8_base_t*)obind);
    oci8_link_to_parent((oci8_base_t*)obind, (oci8_base_t*)stmt);

    if (NIL_P(obind->tdo) && obind->maxar_sz > 0) {
        chker2(OCIBindArrayOfStruct(obind->base.hp.bnd, oci8_errhp, obind->alloc_sz, sizeof(sb2), 0, 0),
               &stmt->base);
    }
    if (data_type->post_bind_hook != NULL) {
        data_type->post_bind_hook(obind);
    }
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

/*
 * @overload __execute(iteration_count)
 *
 *  @param [Integer] iteration_count
 *
 *  @private
 */
static VALUE oci8_stmt_execute(VALUE self, VALUE iteration_count)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(stmt->svc);

    chker3(oci8_call_stmt_execute(svcctx, stmt, NUM2UINT(iteration_count),
                                  svcctx->is_autocommit ? OCI_COMMIT_ON_SUCCESS : OCI_DEFAULT),
           &stmt->base, stmt->base.hp.stmt);
    return self;
}

/*
 * @overload __fetch(connection)
 *
 *  Fetches one row and set the result to <code>@define_handles</code>.
 *  This is called by private methods of OCI8::Cursor.
 *
 *  @param [OCI8] connection
 *  @return [true or false] true on success and false when all rows are fetched.
 *
 *  @private
 */
static VALUE oci8_stmt_fetch(VALUE self, VALUE svc)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(svc);
    sword rv;
    oci8_bind_t *obind;
    const oci8_bind_data_type_t *data_type;

    if (stmt->base.children != NULL) {
        obind = (oci8_bind_t *)stmt->base.children;
        do {
            if (obind->base.type == OCI_HTYPE_DEFINE) {
                data_type = (const oci8_bind_data_type_t *)obind->base.data_type;
                if (data_type->pre_fetch_hook != NULL) {
                    data_type->pre_fetch_hook(obind, stmt->svc);
                }
            }
            obind = (oci8_bind_t *)obind->base.next;
        } while (obind != (oci8_bind_t*)stmt->base.children);
    }
    rv = OCIStmtFetch_nb(svcctx, stmt->base.hp.stmt, oci8_errhp, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
    if (rv == OCI_NO_DATA) {
        return Qfalse;
    }
    chker3(rv, &svcctx->base, stmt->base.hp.stmt);
    return Qtrue;
}

/*
 * @overload __paramGet(pos)
 *
 *  Returns the definition of column specified by <i>pos</i>
 *
 *  @param [Fixnum] pos Column position which starts from one
 *  @return [OCI8::Metadata::Base]
 *
 *  @private
 */
static VALUE oci8_stmt_get_param(VALUE self, VALUE pos)
{
    oci8_stmt_t *stmt = TO_STMT(self);
    OCIParam *parmhp = NULL;
    sword rv;

    Check_Type(pos, T_FIXNUM); /* 1 */
    rv = OCIParamGet(stmt->base.hp.stmt, OCI_HTYPE_STMT, oci8_errhp, (dvoid *)&parmhp, FIX2INT(pos));
    if (rv != OCI_SUCCESS) {
        chker3(rv, &stmt->base, stmt->base.hp.stmt);
    }
    return oci8_metadata_create(parmhp, stmt->svc, self);
}

/*
 *  Gets the rowid of the last inserted, updated or deleted row.
 *  This cannot be used for select statements.
 *
 *  @example
 *    cursor = conn.parse('INSERT INTO foo_table values(:1, :2)', 1, 2)
 *    cursor.exec
 *    cursor.rowid # => "AAAFlLAAEAAAAG9AAA", the inserted row's rowid
 *
 *  @return [String]
 */
static VALUE oci8_stmt_get_rowid(VALUE self)
{
    oci8_base_t *base = oci8_check_typeddata(self, &oci8_stmt_data_type, 1);
    return oci8_get_rowid_attr(base, OCI_ATTR_ROWID, base->hp.stmt);
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
    oci8_stmt_t *stmt = TO_STMT(val);
    oho->hp = stmt->base.hp.ptr;
    RB_OBJ_WRITE(obind->base.self, &oho->obj, val);
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
        oho[idx].obj = rb_class_new_instance(1, &svc, cOCIStmt);
        RB_OBJ_WRITTEN(obind->base.self, Qundef, oho[idx].obj);
        h = DATA_PTR(oho[idx].obj);
        oho[idx].hp = h->hp.ptr;
    } while (++idx < obind->maxar_sz);
}

static const oci8_bind_data_type_t bind_stmt_data_type = {
    {
        {
            "OCI8::BindType::Cursor",
            {
                (RUBY_DATA_FUNC)oci8_bind_hp_obj_mark,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
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

static VALUE bind_stmt_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_stmt_data_type.base);
}

void Init_oci8_stmt(VALUE cOCI8)
{
#if 0
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
    cOCIStmt = rb_define_class_under(cOCI8, "Cursor", cOCIHandle);
#endif
    cOCIStmt = oci8_define_class_under(cOCI8, "Cursor", &oci8_stmt_data_type, oci8_stmt_alloc);

    rb_define_private_method(cOCIStmt, "__initialize", oci8_stmt_initialize, 2);
    rb_define_private_method(cOCIStmt, "__define", oci8_define_by_pos, 2);
    rb_define_private_method(cOCIStmt, "__bind", oci8_bind, 2);
    rb_define_private_method(cOCIStmt, "__execute", oci8_stmt_execute, 1);
    rb_define_private_method(cOCIStmt, "__fetch", oci8_stmt_fetch, 1);
    rb_define_private_method(cOCIStmt, "__paramGet", oci8_stmt_get_param, 1);
    rb_define_method(cOCIStmt, "rowid", oci8_stmt_get_rowid, 0);

    oci8_define_bind_class("Cursor", &bind_stmt_data_type, bind_stmt_alloc);
}
