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

rb_encoding *oci8_encoding;

/*
 * call-seq:
 *   charset_id2name(charset_id) -> charset_name
 *
 * Returns the Oracle character set name from the specified
 * character set ID if it is valid. Otherwise, +nil+ is returned.
 *
 * @param [Integer] charset_id  Oracle character set id
 * @return [String]             Oracle character set name or nil
 * @since 2.2.0
 */
VALUE oci8_s_charset_id2name(VALUE klass, VALUE csid)
{
    char buf[OCI_NLS_MAXBUFSZ];
    sword rv;

    Check_Type(csid, T_FIXNUM);
    rv = OCINlsCharSetIdToName(oci8_envhp, TO_ORATEXT(buf), sizeof(buf), (ub2)FIX2INT(csid));
    if (rv != OCI_SUCCESS) {
        return Qnil;
    }
    return rb_usascii_str_new_cstr(buf);
}

/*
 * call-seq:
 *   charset_name2id(charset_name) -> charset_id
 *
 * Returns the Oracle character set ID for the specified Oracle
 * character set name if it is valid. Othewise, +nil+ is returned.
 *
 * @param [String] charset_name   Oracle character set name
 * @return [Integer]              Oracle character set id or nil
 * @since 2.2.0
 */
static VALUE oci8_s_charset_name2id(VALUE klass, VALUE name)
{
    ub2 rv;

    rv = OCINlsCharSetNameToId(oci8_envhp, TO_ORATEXT(StringValueCStr(name)));
    if (rv == 0) {
        return Qnil;
    }
    return INT2FIX(rv);
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
 * @return [Integer] NLS ratio
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

/*
 * call-seq:
 *   charset_name2id(charset_name) -> charset_id
 *
 * Returns the Oracle character set ID for the specified Oracle
 * character set name if it is valid. Othewise, +nil+ is returned.
 *
 * @param [String] charset_name   Oracle character set name
 * @return [Integer]              Oracle character set id or nil
 * @since 2.0.0
 * @deprecated Use {OCI8.charset_name2id} instead.
 */
static VALUE oci8_charset_name2id(VALUE svc, VALUE name)
{
    rb_warning("Use OCI8.charset_name2id instead of OCI8#charset_name2id.");
    return oci8_s_charset_name2id(Qnil, name);
}

/*
 * call-seq:
 *   charset_id2name(charset_id) -> charset_name
 *
 * Returns the Oracle character set name from the specified
 * character set ID if it is valid. Otherwise, +nil+ is returned.
 *
 * @param [Integer] charset_id  Oracle character set id
 * @return [String]             Oracle character set name or nil
 * @since 2.0.0
 * @deprecated Use {OCI8.charset_id2name} instead.
 */
static VALUE oci8_charset_id2name(VALUE svc, VALUE name)
{
    rb_warning("Use OCI8.charset_id2name instead of OCI8#charset_id2name.");
    return oci8_s_charset_id2name(Qnil, name);
}


void Init_oci8_encoding(VALUE cOCI8)
{
#if 0
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
#endif

    rb_define_singleton_method(cOCI8, "charset_name2id", oci8_s_charset_name2id, 1);
    rb_define_singleton_method(cOCI8, "charset_id2name", oci8_s_charset_id2name, 1);
    rb_define_singleton_method(cOCI8, "nls_ratio", oci8_get_nls_ratio, 0);
    rb_define_singleton_method(cOCI8, "encoding", oci8_get_encoding, 0);
    rb_define_singleton_method(cOCI8, "encoding=", oci8_set_encoding, 1);
    rb_define_method(cOCI8, "charset_name2id", oci8_charset_name2id, 1);
    rb_define_method(cOCI8, "charset_id2name", oci8_charset_id2name, 1);
}
