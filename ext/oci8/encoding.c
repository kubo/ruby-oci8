/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * encoding.c - part of ruby-oci8
 *
 * Copyright (C) 2008 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

#ifndef OCI_NLS_MAXBUFSZ
#define OCI_NLS_MAXBUFSZ 100
#endif

/* type of callback function's argument */
typedef struct {
    oci8_svcctx_t *svcctx;
    OCIStmt *stmtp;
    union {
        VALUE name;
        int csid;
    } u;
} cb_arg_t;

/* Oracle charset id -> Oracle charset name */
static VALUE csid2name;

/* Oracle charset name -> Oracle charset id */
static ID id_upcase;
static VALUE csname2id;
static VALUE oci8_charset_name2id(VALUE svc, VALUE name);

VALUE oci8_charset_id2name(VALUE svc, VALUE csid)
{
    VALUE name = rb_hash_aref(csid2name, csid);

    if (!NIL_P(name)) {
        return name;
    }
    Check_Type(csid, T_FIXNUM);
    if (have_OCINlsCharSetIdToName) {
        /* Oracle 9iR2 or upper */
        char buf[OCI_NLS_MAXBUFSZ];
        sword rv;

        rv = OCINlsCharSetIdToName(oci8_envhp, TO_ORATEXT(buf), sizeof(buf), FIX2INT(csid));
        if (rv != OCI_SUCCESS) {
            return Qnil;
        }
        name = rb_str_new2(buf);
    } else {
        /* Oracle 9iR1 or lower */
        oci8_exec_sql_var_t bind_vars[2];
        char buf[OCI_NLS_MAXBUFSZ];
        ub2 buflen = 0;
        int ival = FIX2INT(csid);

        /* :name */
        bind_vars[0].valuep = buf;
        bind_vars[0].value_sz = OCI_NLS_MAXBUFSZ;
        bind_vars[0].dty = SQLT_CHR;
        bind_vars[0].indp = NULL;
        bind_vars[0].alenp = &buflen;
        /* :csid */
        bind_vars[1].valuep = &ival;
        bind_vars[1].value_sz = sizeof(int);
        bind_vars[1].dty = SQLT_INT;
        bind_vars[1].indp = NULL;
        bind_vars[1].alenp = NULL;

        /* convert chaset id to charset name by querying Oracle server. */
        oci8_exec_sql(oci8_get_svcctx(svc), "BEGIN :name := nls_charset_name(:csid); END;", 0, NULL, 2, bind_vars, 1);
        if (buflen == 0) {
            return Qnil;
        }
        name = rb_str_new(buf, buflen);
    }
    OBJ_FREEZE(name);
    rb_hash_aset(csid2name, csid, name);
    rb_hash_aset(csname2id, name, csid);
    return name;
}

static VALUE oci8_charset_name2id(VALUE svc, VALUE name)
{
    VALUE csid;

    name = rb_funcall(name, id_upcase, 0);
    csid = rb_hash_aref(csname2id, StringValue(name));
    if (!NIL_P(csid)) {
        return csid;
    }
    if (have_OCINlsCharSetNameToId) {
        /* Oracle 9iR2 or upper */
        ub2 rv;

        rv = OCINlsCharSetNameToId(oci8_envhp, RSTRING_ORATEXT(name));
        if (rv == 0) {
            return Qnil;
        }
        csid = INT2FIX(rv);
    } else {
        /* Oracle 9iR1 or lower */
        oci8_exec_sql_var_t bind_vars[2];
        int ival;
        sb2 ind = 0; /* null indicator */

        /* :csid */
        bind_vars[0].valuep = &ival;
        bind_vars[0].value_sz = sizeof(int);
        bind_vars[0].dty = SQLT_INT;
        bind_vars[0].indp = &ind;
        bind_vars[0].alenp = NULL;
        /* :name */
        bind_vars[1].valuep = RSTRING_PTR(name);
        bind_vars[1].value_sz = RSTRING_LEN(name);
        bind_vars[1].dty = SQLT_CHR;
        bind_vars[1].indp = NULL;
        bind_vars[1].alenp = NULL;

        /* convert chaset name to charset id by querying Oracle server. */
        oci8_exec_sql(oci8_get_svcctx(svc), "BEGIN :csid := nls_charset_id(:name); END;", 0, NULL, 2, bind_vars, 1);
        if (ind) {
            return Qnil;
        }
        csid = INT2FIX(ival);
    }
    rb_hash_aset(csid2name, csid, name);
    rb_hash_aset(csname2id, name, csid);
    return csid;
}


void Init_oci8_encoding(VALUE cOCI8)
{
    csid2name = rb_hash_new();
    rb_global_variable(&csid2name);

    id_upcase = rb_intern("upcase");
    csname2id = rb_hash_new();
    rb_global_variable(&csname2id);

    rb_define_method(cOCI8, "charset_name2id", oci8_charset_name2id, 1);
    rb_define_method(cOCI8, "charset_id2name", oci8_charset_id2name, 1);
}
