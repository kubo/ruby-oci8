/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * encoding.c - part of ruby-oci8
 *
 * Copyright (C) 2008-2010 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

#ifndef OCI_NLS_MAXBUFSZ
#define OCI_NLS_MAXBUFSZ 100
#endif

/* NLS ratio, maximum number of bytes per one chracter */
int oci8_nls_ratio = 1;

/* Oracle charset id -> Oracle charset name */
static VALUE csid2name;

/* Oracle charset name -> Oracle charset id */
static ID id_upcase;
static VALUE csname2id;
static VALUE oci8_charset_name2id(VALUE svc, VALUE name);
rb_encoding *oci8_encoding;

/*
 * call-seq:
 *   charset_id2name(charset_id) -> charset_name
 *
 * Returns the Oracle character set name from the specified
 * character set ID if it is valid. Otherwise, +nil+ is returned.
 *
 * === Oracle 9iR2 client or upper
 *
 * It is done by using the mapping table stored in the client side.
 *
 * === Oracle 9iR1 client or lower
 *
 * It executes the following PL/SQL block internally to use
 * the mapping table stored in the server side.
 *
 *   BEGIN
 *     :name := nls_charset_name(:csid);
 *   END;
 *
 * @param [Fixnum] charset_id   Oracle character set id
 * @return [String]             Oracle character set name or nil
 * @since 2.0.0
 */
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

        rv = OCINlsCharSetIdToName(oci8_envhp, TO_ORATEXT(buf), sizeof(buf), (ub2)FIX2INT(csid));
        if (rv != OCI_SUCCESS) {
            return Qnil;
        }
        name = rb_usascii_str_new_cstr(buf);
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
        name = rb_usascii_str_new(buf, buflen);
    }
    OBJ_FREEZE(name);
    rb_hash_aset(csid2name, csid, name);
    rb_hash_aset(csname2id, name, csid);
    return name;
}

/*
 * call-seq:
 *   charset_name2id(charset_name) -> charset_id
 *
 * Returns the Oracle character set ID for the specified Oracle
 * character set name if it is valid. Othewise, +nil+ is returned.
 *
 * === Oracle 9iR2 client or upper
 *
 * It is done by using the mapping table stored in the client side.
 *
 * === Oracle 9iR1 client or lower
 *
 * It executes the following PL/SQL block internally to use
 * the mapping table stored in the server side.
 *
 *   BEGIN
 *     :csid := nls_charset_id(:name);
 *   END;
 *
 * @param [String] charset_name   Oracle character set name
 * @return [Fixnum]               Oracle character set id or nil
 * @since 2.0.0
 */
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

/*
 * call-seq:
 *   OCI8.nls_ratio -> integer
 *
 * Gets NLS ratio, maximum number of bytes per one character of the
 * current NLS chracter set. It is a factor to calculate the
 * internal buffer size of a string bind variable whose nls length
 * semantics is char.
 *
 * @return [Fixnum]  NLS ratio
 * @since 2.1.0
 * @private
 */
static VALUE oci8_get_nls_ratio(VALUE klass)
{
    return INT2NUM(oci8_nls_ratio);
}

/*
 * call-seq:
 *    OCI8.encoding -> enc
 *
 * Returns the Oracle client encoding.
 *
 * When string data, such as SQL statements and bind variables,
 * are passed to Oracle, they are converted to +OCI8.encoding+
 * in advance.
 *
 * @example
 *   # When OCI8.encoding is ISO-8859-1,
 *   conn.exec('insert into country_code values(:1, :2, :3)',
 *             'AT', 'Austria', "\u00d6sterreichs")
 *   # "\u00d6sterreichs" is 'Österreichs' encoded by UTF-8.
 *   # It is converted to ISO-8859-1 before it is passed to
 *   # the Oracle C API.
 *
 *
 * When string data, such as fetched values and bind variable
 * for output, are retrieved from Oracle, they are encoded
 * by +OCI8.encoding+ if +Encoding.default_internal+ is +nil+.
 * If it isn't +nil+, they are converted from +OCI8.encoding+
 * to +Encoding.default_internal+.
 *
 * If +OCI8.encoding+ is ASCII-8BIT, no encoding conversions
 * are done.
 *
 * @return [Encoding]
 * @since 2.0.0 and ruby 1.9
 * @private
 * @see OCI8.client_charset_name
 */
static VALUE oci8_get_encoding(VALUE klass)
{
    return rb_enc_from_encoding(oci8_encoding);
}

/*
 * call-seq:
 *   OCI8.encoding = enc or nil
 *
 * Sets Oracle client encoding. You must not use this method.
 * You should set the environment variable NLS_LANG properly to
 * change +OCI8.encoding+.
 *
 * @param [Encoding]  enc
 * @since 2.0.0 and ruby 1.9
 * @private
 */
static VALUE oci8_set_encoding(VALUE klass, VALUE encoding)
{
    if (NIL_P(encoding)) {
        oci8_encoding = NULL;
        oci8_nls_ratio = 1;
    } else {
        oci8_encoding = rb_to_encoding(encoding);
        oci8_nls_ratio = rb_enc_mbmaxlen(oci8_encoding);
    }
    return encoding;
}

void Init_oci8_encoding(VALUE cOCI8)
{
#if 0
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif
    csid2name = rb_hash_new();
    rb_global_variable(&csid2name);

    id_upcase = rb_intern("upcase");
    csname2id = rb_hash_new();
    rb_global_variable(&csname2id);

    rb_define_method(cOCI8, "charset_name2id", oci8_charset_name2id, 1);
    rb_define_method(cOCI8, "charset_id2name", oci8_charset_id2name, 1);
    rb_define_singleton_method(cOCI8, "nls_ratio", oci8_get_nls_ratio, 0);
    rb_define_singleton_method(cOCI8, "encoding", oci8_get_encoding, 0);
    rb_define_singleton_method(cOCI8, "encoding=", oci8_set_encoding, 1);
}
