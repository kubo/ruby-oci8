/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  ocinumber.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2005 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
#include <orl.h>

static VALUE cOCINumber;
static OCINumber const_p1;   /*  +1 */
static OCINumber const_p10;  /* +10 */
static OCINumber const_m1;   /*  -1 */
static OCINumber const_PI2;  /* +PI/2 */
static OCINumber const_mPI2; /* -PI/2 */

#define SHRESHOLD_FMT "00000000000000000000000000"
#define SHRESHOLD_FMT_LEN (sizeof(SHRESHOLD_FMT) - 1)
#define SHRESHOLD_VAL "10000000000000000000000000"
#define SHRESHOLD_VAL_LEN (sizeof(SHRESHOLD_VAL) - 1)
static OCINumber const_shreshold;

/* TODO: scale: -84 - 127 */
#define NUMBER_FORMAT1 "FM9999999999999999999999990.9999999999999999999999999999999999999"
#define NUMBER_FORMAT1_LEN (sizeof(NUMBER_FORMAT1) - 1)
#define NUMBER_FORMAT2 "FM99999999999999999999999999999999999990.999999999999999999999999"
#define NUMBER_FORMAT2_LEN (sizeof(NUMBER_FORMAT2) - 1)
#define NUMBER_FORMAT2_DECIMAL                          (sizeof("999999999999999999999999") - 1)
#define NUMBER_FORMAT_INT "FM99999999999999999999999999999999999990"
#define NUMBER_FORMAT_INT_LEN (sizeof(NUMBER_FORMAT_INT) - 1)

#define _NUMBER(val) ((OCINumber *)DATA_PTR(val)) /* dangerous macro */

static VALUE onum_s_alloc(VALUE klass)
{
    VALUE obj;
    OCINumber *d;

    obj = Data_Make_Struct(klass, OCINumber, NULL, xfree, d);
    OCINumberSetZero(oci8_errhp, d);
    return obj;
}

/* construct an ruby object(OCI::Number) from C structure (OCINumber). */
static VALUE make_oci_number(const OCINumber *s)
{
    VALUE obj;
    OCINumber *d;

    obj = Data_Make_Struct(cOCINumber, OCINumber, NULL, xfree, d);
    oci_lc(OCINumberAssign(oci8_errhp, s, d));
    return obj;
}

/* fill C structure (OCINumber) from a string. */
static void set_oci_number_from_str(OCINumber *result, VALUE str, VALUE fmt, VALUE nls_params)
{
    oratext *fmt_ptr;
    oratext *nls_params_ptr;
    ub4 fmt_len;
    ub4 nls_params_len;

    StringValue(str);
    /* set from string. */
    if (NIL_P(fmt)) {
        int i, cnt = 0;
        for (i = RSTRING(str)->len - 1; i >= 0; i--) {
            if (RSTRING(str)->ptr[i] != ' ')
                cnt++;
            if (RSTRING(str)->ptr[i] == '.') {
                i = RSTRING(str)->len - i;
                break;
            }
        }
        if (i == -1)
            cnt = 0;
        if (cnt <= NUMBER_FORMAT2_DECIMAL) {
            fmt_ptr = NUMBER_FORMAT2;
            fmt_len = NUMBER_FORMAT2_LEN;
        } else {
            fmt_ptr = NUMBER_FORMAT1;
            fmt_len = NUMBER_FORMAT1_LEN;
        }
    } else {
        StringValue(fmt);
        fmt_ptr = RSTRING(fmt)->ptr;
        fmt_len = RSTRING(fmt)->len;
    }
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING(nls_params)->ptr;
        nls_params_len = RSTRING(nls_params)->len;
    }
    oci_lc(OCINumberFromText(oci8_errhp,
                             RSTRING(str)->ptr, RSTRING(str)->len,
                             fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                             result));
}

/* fill C structure (OCINumber) from a numeric object. */
/* 1 - success, 0 - error */
static int set_oci_number_from_num(OCINumber *result, VALUE num, int force)
{
    signed long sl;
    double dbl;

    if (!RTEST(rb_obj_is_kind_of(num, rb_cNumeric)))
        rb_raise(rb_eTypeError, "expect Numeric but %s", rb_class2name(CLASS_OF(num)));
    switch (rb_type(num)) {
    case T_FIXNUM:
        /* set from long. */
        sl = NUM2LONG(num);
        oci_lc(OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, result));
        return 1;
    case T_FLOAT:
        /* set from double. */
        dbl = NUM2DBL(num);
        oci_lc(OCINumberFromReal(oci8_errhp, &dbl, sizeof(dbl), result));
        return 1;
    case T_BIGNUM:
        /* change via string. */
        num = rb_big2str(num, 10);
        set_oci_number_from_str(result, num, Qnil, Qnil);
        return 1;
    }
    if (RTEST(rb_obj_is_instance_of(num, cOCINumber))) {
        /* OCI::Number */
        oci_lc(OCINumberAssign(oci8_errhp, DATA_PTR(num), result));
        return 1;
    }
    if (force) {
        /* change via string as a last resort. */
        /* TODO: if error, raise TypeError instead of OCI::Error */
        set_oci_number_from_str(result, num, Qnil, Qnil);
        return 1;
    }
    return 0;
}

static const OCINumber *get_oci_number(OCINumber *result, VALUE self)
{
    set_oci_number_from_num(result, self, 1);
    return result;
}
#define TO_OCINUM(on, val) get_oci_number((on), (val))

static VALUE omath_atan2(VALUE self, VALUE Ycoordinate, VALUE Xcoordinate)
{
    OCINumber nY;
    OCINumber nX;
    OCINumber rv;
    boolean is_zero;
    sword sign;

    set_oci_number_from_num(&nX, Xcoordinate, 1);
    set_oci_number_from_num(&nY, Ycoordinate, 1);
    /* check zero */
    oci_lc(OCINumberIsZero(oci8_errhp, &nX, &is_zero));
    if (is_zero) {
        oci_lc(OCINumberSign(oci8_errhp, &nY, &sign));
        switch (sign) {
        case 0:
            return INT2FIX(0); /* atan2(0, 0) => 0 or ERROR? */
        case 1:
            return make_oci_number(&const_PI2); /* atan2(positive, 0) => PI/2 */
        case -1:
            return make_oci_number(&const_mPI2); /* atan2(negative, 0) => -PI/2 */
        }
    }
    /* atan2 */
    oci_lc(OCINumberArcTan2(oci8_errhp, &nY, &nX, &rv));
    return make_oci_number(&rv);
}

static VALUE omath_cos(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberCos(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return make_oci_number(&rv);
}

static VALUE omath_sin(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberSin(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return make_oci_number(&rv);
}

static VALUE omath_tan(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberTan(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return make_oci_number(&rv);
}

static VALUE omath_acos(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1);
    /* check upper bound */
    oci_lc(OCINumberCmp(oci8_errhp, &n, &const_p1, &sign));
    if (sign > 0)
        rb_raise(rb_eRangeError, "out of range for acos");
    /* check lower bound */
    oci_lc(OCINumberCmp(oci8_errhp, &n, &const_m1, &sign));
    if (sign < 0)
        rb_raise(rb_eRangeError, "out of range for acos");
    /* acos */
    oci_lc(OCINumberArcCos(oci8_errhp, &n, &r));
    return make_oci_number(&r);
}

static VALUE omath_asin(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1);
    /* check upper bound */
    oci_lc(OCINumberCmp(oci8_errhp, &n, &const_p1, &sign));
    if (sign > 0)
        rb_raise(rb_eRangeError, "out of range for asin");
    /* check lower bound */
    oci_lc(OCINumberCmp(oci8_errhp, &n, &const_m1, &sign));
    if (sign < 0)
        rb_raise(rb_eRangeError, "out of range for asin");
    /* asin */
    oci_lc(OCINumberArcSin(oci8_errhp, &n, &r));
    return make_oci_number(&r);
}

static VALUE omath_atan(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberArcTan(oci8_errhp, TO_OCINUM(&n, num), &r));
    return make_oci_number(&r);
}

static VALUE omath_cosh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypCos(oci8_errhp, TO_OCINUM(&n, num), &r));
    return make_oci_number(&r);
}

static VALUE omath_sinh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypSin(oci8_errhp, TO_OCINUM(&n, num), &r));
    return make_oci_number(&r);
}

static VALUE omath_tanh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypTan(oci8_errhp, TO_OCINUM(&n, num), &r));
    return make_oci_number(&r);
}

static VALUE omath_exp(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberExp(oci8_errhp, TO_OCINUM(&n, num), &r));
    return make_oci_number(&r);
}

static VALUE omath_log(int argc, VALUE *argv, VALUE obj)
{
    VALUE num, base;
    OCINumber n;
    OCINumber b;
    OCINumber r;
    sword sign;

    rb_scan_args(argc, argv, "11", &num, &base /* OCI::Math::E */);
    set_oci_number_from_num(&n, num, 1);
    oci_lc(OCINumberSign(oci8_errhp, &n, &sign));
    if (sign <= 0)
        rb_raise(rb_eRangeError, "nonpositive value for log");
    if (NIL_P(base)) {
        oci_lc(OCINumberLn(oci8_errhp, &n, &r));
    } else {
        set_oci_number_from_num(&b, base, 1);
        oci_lc(OCINumberSign(oci8_errhp, &b, &sign));
        if (sign <= 0)
            rb_raise(rb_eRangeError, "nonpositive value for the base of log");
        oci_lc(OCINumberCmp(oci8_errhp, &b, &const_p1, &sign));
        if (sign == 0)
            rb_raise(rb_eRangeError, "base 1 for log");
        oci_lc(OCINumberLog(oci8_errhp, &b, &n, &r));
    }
    return make_oci_number(&r);
}

static VALUE omath_log10(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1);
    oci_lc(OCINumberSign(oci8_errhp, &n, &sign));
    if (sign <= 0)
        rb_raise(rb_eRangeError, "nonpositive value for log10");
    oci_lc(OCINumberLog(oci8_errhp, &const_p10, &n, &r));
    return make_oci_number(&r);
}

static VALUE omath_sqrt(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1);
    /* check whether num is negative */
    oci_lc(OCINumberSign(oci8_errhp, &n, &sign));
    if (sign < 0)
        rb_raise(rb_eRangeError, "negative value for sqrt");
    oci_lc(OCINumberSqrt(oci8_errhp, &n, &r));
    return make_oci_number(&r);
}

#ifndef HAVE_RB_DEFINE_ALLOC_FUNC /* emulate allocation framework. */
static VALUE onum_s_new(int argc, VALUE *argv, VALUE klass)
{
    VALUE obj = onum_s_alloc(klass);
    rb_obj_call_init(obj, argc, argv);
    return obj;
}
#endif

static VALUE onum_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE val;
    VALUE fmt;
    VALUE nls_params;

    if (rb_scan_args(argc, argv, "03", &val /* 0 */, &fmt /* nil */, &nls_params /* nil */) == 0) {
        OCINumberSetZero(oci8_errhp, _NUMBER(self));
    } else if (RTEST(rb_obj_is_kind_of(val, rb_cNumeric))) {
        set_oci_number_from_num(_NUMBER(self), val, 1);
    } else {
        set_oci_number_from_str(_NUMBER(self), val, fmt, nls_params);
    }
    return Qnil;
}

static VALUE onum_coerce(VALUE self, VALUE other)
{
    OCINumber n;

    if (RTEST(rb_obj_is_kind_of(other, rb_cNumeric)))
        if (set_oci_number_from_num(&n, other, 0))
            return rb_assoc_new(make_oci_number(&n), self);
    rb_raise(rb_eTypeError, "Can't coerce %s to %s",
             rb_class2name(CLASS_OF(other)), rb_class2name(cOCINumber));
}

static VALUE onum_neg(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberNeg(oci8_errhp, _NUMBER(self), &r));
    return make_oci_number(&r);
}

static VALUE onum_add(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* add */
    oci_lc(OCINumberAdd(oci8_errhp, _NUMBER(lhs), &n, &r));
    return make_oci_number(&r);
}

static VALUE onum_sub(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* subtracting */
    oci_lc(OCINumberSub(oci8_errhp, _NUMBER(lhs), &n, &r));
    return make_oci_number(&r);
}

static VALUE onum_mul(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* multiply */
    oci_lc(OCINumberMul(oci8_errhp, _NUMBER(lhs), &n, &r));
    return make_oci_number(&r);
}

static VALUE onum_div(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* check whether argument is not zero. */
    oci_lc(OCINumberIsZero(oci8_errhp, &n, &is_zero));
    if (is_zero)
        rb_num_zerodiv();
    /* division */
    oci_lc(OCINumberDiv(oci8_errhp, _NUMBER(lhs), &n, &r));
    return make_oci_number(&r);
}

static VALUE onum_mod(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* check whether argument is not zero. */
    oci_lc(OCINumberIsZero(oci8_errhp, &n, &is_zero));
    if (is_zero)
        rb_num_zerodiv();
    /* modulo */
    oci_lc(OCINumberMod(oci8_errhp, _NUMBER(lhs), &n, &r));
    return make_oci_number(&r);
}

static VALUE onum_power(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;
    sword sign;
    boolean is_int;

    if (FIXNUM_P(rhs)) {
        oci_lc(OCINumberIntPower(oci8_errhp, _NUMBER(lhs), FIX2INT(rhs), &r));
    } else {
        /* change to OCINumber */
        if (!set_oci_number_from_num(&n, rhs, 0))
            return rb_num_coerce_bin(lhs, rhs);
        /* check whether num1 is negative. */
        oci_lc(OCINumberSign(oci8_errhp, _NUMBER(lhs), &sign));
        /* check whether num2 is an integral value. */
        oci_lc(OCINumberIsInt(oci8_errhp, &n, &is_int));
        if (sign < 0 && !is_int)
            rb_raise(rb_eRangeError, "base is negative and exponent part is not an integral value");
        oci_lc(OCINumberPower(oci8_errhp, _NUMBER(lhs), &n, &r));
    }
    return make_oci_number(&r); 
}

static VALUE onum_cmp(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    sword r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs);
    /* compare */
    oci_lc(OCINumberCmp(oci8_errhp, _NUMBER(lhs), &n, &r));
    return INT2NUM(r);
}

static VALUE onum_floor(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberFloor(oci8_errhp, _NUMBER(self), &r));
    return make_oci_number(&r);
}

static VALUE onum_ceil(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberCeil(oci8_errhp, _NUMBER(self), &r));
    return make_oci_number(&r);
}

static VALUE onum_round(int argc, VALUE *argv, VALUE self)
{
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    oci_lc(OCINumberRound(oci8_errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    return make_oci_number(&r);
}

static VALUE onum_trunc(int argc, VALUE *argv, VALUE self)
{
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    oci_lc(OCINumberTrunc(oci8_errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    return make_oci_number(&r);
}

static VALUE onum_round_prec(VALUE self, VALUE ndigs)
{
    OCINumber r;

    oci_lc(OCINumberPrec(oci8_errhp, _NUMBER(self), NUM2INT(ndigs), &r));
    return make_oci_number(&r);
}

static VALUE onum_to_char(int argc, VALUE *argv, VALUE self)
{
    VALUE fmt;
    VALUE nls_params;
    text buf[512];
    ub4 buf_size = sizeof(buf);
    oratext *fmt_ptr;
    oratext *nls_params_ptr;
    ub4 fmt_len;
    ub4 nls_params_len;
    sword rv;

    rb_scan_args(argc, argv, "02", &fmt /* nil */, &nls_params /* nil */);
    if (NIL_P(fmt)) {
        OCINumber absval;
        sword sign;
        boolean is_int;

        oci_lc(OCINumberIsInt(oci8_errhp, _NUMBER(self), &is_int));
        if (is_int) {
            fmt_ptr = NUMBER_FORMAT_INT;
            fmt_len = NUMBER_FORMAT_INT_LEN;
        } else {
            oci_lc(OCINumberAbs(oci8_errhp, _NUMBER(self), &absval));
            oci_lc(OCINumberCmp(oci8_errhp, &absval, &const_shreshold, &sign));
            if (sign >= 0) {
                fmt_ptr = NUMBER_FORMAT2;
                fmt_len = NUMBER_FORMAT2_LEN;
            } else {
                fmt_ptr = NUMBER_FORMAT1;
                fmt_len = NUMBER_FORMAT1_LEN;
            }
        }
    } else {
        StringValue(fmt);
        fmt_ptr = RSTRING(fmt)->ptr;
        fmt_len = RSTRING(fmt)->len;
    }
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING(nls_params)->ptr;
        nls_params_len = RSTRING(nls_params)->len;
    }
    rv = OCINumberToText(oci8_errhp, _NUMBER(self),
                         fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                         &buf_size, buf);
    if (rv == OCI_ERROR) {
        ub4 errcode;
        OCIErrorGet(oci8_errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
        if (errcode == 22065) {
            /* OCI-22065: number to text translation for the given format causes overflow */
            if (NIL_P(fmt)) /* implicit conversion */
                return rb_str_new2("overflow");
        }
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return rb_str_new(buf, buf_size);
}

static VALUE onum_to_s(VALUE self)
{
    return onum_to_char(0, NULL, self);
}

static VALUE onum_to_i(VALUE self)
{
    OCINumber num;
    signed long sl;
    text buf[512];
    ub4 buf_size = sizeof(buf);

    oci_lc(OCINumberTrunc(oci8_errhp, _NUMBER(self), 0, &num));
    if (OCINumberToInt(oci8_errhp, &num, sizeof(sl), OCI_NUMBER_SIGNED, &sl) == OCI_SUCCESS) {
        return LONG2NUM(sl);
    }
    oci_lc(OCINumberToText(oci8_errhp, &num, NUMBER_FORMAT2, NUMBER_FORMAT2_LEN,
                           NULL, 0, &buf_size, buf));
    return rb_cstr2inum(buf, 10);
}

static VALUE onum_to_f(VALUE self)
{
    double dbl;

    oci_lc(OCINumberToReal(oci8_errhp, _NUMBER(self), sizeof(dbl), &dbl));
    return rb_float_new(dbl);
}

static VALUE onum_sign(VALUE self)
{
    sword result;

    oci_lc(OCINumberSign(oci8_errhp, _NUMBER(self), &result));
    return INT2FIX(result);
}

static VALUE onum_zero_p(VALUE self)
{
    boolean result;

    oci_lc(OCINumberIsZero(oci8_errhp, _NUMBER(self), &result));
    return result ? Qtrue : Qfalse;
}

static VALUE onum_abs(VALUE self)
{
    OCINumber result;

    oci_lc(OCINumberAbs(oci8_errhp, _NUMBER(self), &result));
    return make_oci_number(&result);
}

static VALUE onum_shift(VALUE self, VALUE exp)
{
    OCINumber result;

    oci_lc(OCINumberShift(oci8_errhp, _NUMBER(self), NUM2INT(exp), &result));
    return make_oci_number(&result);
}

static VALUE onum_hash(VALUE self)
{
    char *c  = DATA_PTR(self);
    int size = c[0] + 1;
    int i, hash;

    /* assert(size <= 22); ?*/
    if (size > 22)
        size = 22;

    for (hash = 0, i = 1; i< size; i++) {
        hash += c[i] * 971;
    }
    if (hash < 0) hash = -hash;
    return INT2FIX(hash);
}

static VALUE onum_inspect(VALUE self)
{
    unsigned char *c  = DATA_PTR(self);
    int size = c[0] + 1;
    int i;
    char *name = rb_class2name(CLASS_OF(self));
    VALUE s = onum_to_s(self);
    VALUE rv = rb_str_new(0, strlen(name) + RSTRING(s)->len + size * 3 + 32);
    char *ptr = RSTRING(rv)->ptr;

    sprintf(ptr, "#<%s:%s(%u", name, RSTRING(s)->ptr, c[0]);
    ptr += strlen(ptr);
    for (i = 1; i < size; i++) {
        sprintf(ptr, ".%u", c[i]);
        ptr += strlen(ptr);
    }
    strcpy(ptr, ")>");
    RSTRING(rv)->len = strlen(RSTRING(rv)->ptr);
    return rv;
}

static VALUE onum_dump(int argc, VALUE *argv, VALUE self)
{
    char *c  = DATA_PTR(self);
    int size = c[0] + 1;
    VALUE dummy;

    rb_scan_args(argc, argv, "01", &dummy);
    return rb_str_new(c, size);
}

static VALUE
onum_s_load(VALUE klass, VALUE str)
{
    unsigned char *c;
    int size;
    OCINumber num;

    Check_Type(str, T_STRING);
    c = RSTRING(str)->ptr;
    size = RSTRING(str)->len;
    if (size == 0 || size != c[0] + 1 || size > sizeof(num)) {
        rb_raise(rb_eTypeError, "marshaled OCI::Number format differ");
    }
    memset(&num, 0, sizeof(num));
    memcpy(&num, c, size);
    return make_oci_number(&num);
}

/*
 * bind_ocinumber
 */
typedef struct {
    oci8_bind_t base;
    OCINumber on;
} oci8_bind_ocinumber_t;

static VALUE bind_ocinumber_get(oci8_bind_t *bb)
{
    oci8_bind_ocinumber_t *bo = (oci8_bind_ocinumber_t *)bb;
    return make_oci_number(&bo->on);
}

static void bind_ocinumber_set(oci8_bind_t *bb, VALUE val)
{
    oci8_bind_ocinumber_t *bo = (oci8_bind_ocinumber_t *)bb;
    set_oci_number_from_num(&bo->on, val, 1);
}

static void bind_ocinumber_init(oci8_bind_t *bb, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_ocinumber_t *bo = (oci8_bind_ocinumber_t *)bb;
    bb->valuep = &bo->on;
    bb->value_sz = sizeof(OCINumber);
}

static oci8_bind_class_t bind_ocinumber_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_ocinumber_t)
    },
    bind_ocinumber_get,
    bind_ocinumber_set,
    bind_ocinumber_init,
    SQLT_VNU,
};

void
Init_oci_number(VALUE cOCI8)
{
    VALUE mMath;
    OCINumber num1, num2;
    VALUE obj_PI, obj_E;
    signed long sl;

    cOCINumber = rb_define_class("OCINumber", rb_cNumeric);
    mMath = rb_define_module_under(cOCI8, "Math");

    /* constants for internal use. */
    /* set const_p1 */
    sl = 1;
    OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_p1);
    /* set const_p10 */
    sl = 10;
    OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_p10);
    /* set const_m1 */
    sl = -1;
    OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_m1);
    /* set const_PI2 */
    sl = 2;
    OCINumberSetPi(oci8_errhp, &num1);
    OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &num2);
    OCINumberDiv(oci8_errhp, &num1 /* PI */, &num2 /* 2 */, &const_PI2);
    /* set const_mPI2 */
    OCINumberNeg(oci8_errhp, &const_PI2 /* PI/2 */, &const_mPI2);
    /* set const_shreshold */
    OCINumberFromText(oci8_errhp, SHRESHOLD_VAL, SHRESHOLD_VAL_LEN, SHRESHOLD_FMT, SHRESHOLD_FMT_LEN,
                      NULL, 0, &const_shreshold);

    /* PI */
    OCINumberSetPi(oci8_errhp, &num1);
    obj_PI = make_oci_number(&num1);

    /* E */
    sl = 1;
    OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &num1); /* num1 := 1 */
    OCINumberExp(oci8_errhp, &num1, &num2); /* num2 := exp(1) */
    obj_E = make_oci_number(&num2);

    /*
     * constans of OCI::Math.
     */
    rb_define_const(mMath, "PI", obj_PI);
    rb_define_const(mMath, "E", obj_E);

    /*
     * module functions of OCI::Math.
     */
    rb_define_module_function(mMath, "atan2", omath_atan2, 2);

    rb_define_module_function(mMath, "cos", omath_cos, 1);
    rb_define_module_function(mMath, "sin", omath_sin, 1);
    rb_define_module_function(mMath, "tan", omath_tan, 1);

    rb_define_module_function(mMath, "acos", omath_acos, 1);
    rb_define_module_function(mMath, "asin", omath_asin, 1);
    rb_define_module_function(mMath, "atan", omath_atan, 1);

    rb_define_module_function(mMath, "cosh", omath_cosh, 1);
    rb_define_module_function(mMath, "sinh", omath_sinh, 1);
    rb_define_module_function(mMath, "tanh", omath_tanh, 1);

    rb_define_module_function(mMath, "exp", omath_exp, 1);
    rb_define_module_function(mMath, "log", omath_log, -1);
    rb_define_module_function(mMath, "log10", omath_log10, 1);
    rb_define_module_function(mMath, "sqrt", omath_sqrt, 1);

#ifdef HAVE_RB_DEFINE_ALLOC_FUNC
    /* ruby 1.8 */
    rb_define_alloc_func(cOCINumber, onum_s_alloc);
#else
    /* ruby 1.6 */
    rb_define_singleton_method(cOCINumber, "new", onum_s_new, -1);
#endif

    /* methods of OCI::Number */
    rb_define_method_nodoc(cOCINumber, "initialize", onum_initialize, -1);
    rb_define_method_nodoc(cOCINumber, "coerce", onum_coerce, 1);

    rb_define_method(cOCINumber, "-@", onum_neg, 0);
    rb_define_method(cOCINumber, "+", onum_add, 1);
    rb_define_method(cOCINumber, "-", onum_sub, 1);
    rb_define_method(cOCINumber, "*", onum_mul, 1);
    rb_define_method(cOCINumber, "/", onum_div, 1);
    rb_define_method(cOCINumber, "%", onum_mod, 1);
    rb_define_method(cOCINumber, "**", onum_power, 1);
    rb_define_method(cOCINumber, "<=>", onum_cmp, 1);

    rb_define_method(cOCINumber, "floor", onum_floor, 0);
    rb_define_method(cOCINumber, "ceil", onum_ceil, 0);
    rb_define_method(cOCINumber, "round", onum_round, -1);
    rb_define_method(cOCINumber, "trunc", onum_trunc, -1);
    rb_define_method(cOCINumber, "round_prec", onum_round_prec, 1);

    rb_define_method(cOCINumber, "to_s", onum_to_s, 0);
    rb_define_method(cOCINumber, "to_char", onum_to_char, -1);
    rb_define_method(cOCINumber, "to_i", onum_to_i, 0);
    rb_define_method(cOCINumber, "to_f", onum_to_f, 0);

    rb_define_method(cOCINumber, "sign", onum_sign, 0);
    rb_define_method(cOCINumber, "zero?", onum_zero_p, 0);
    rb_define_method(cOCINumber, "abs", onum_abs, 0);
    rb_define_method(cOCINumber, "shift", onum_shift, 1);

    rb_define_method_nodoc(cOCINumber, "hash", onum_hash, 0);
    rb_define_method_nodoc(cOCINumber, "inspect", onum_inspect, 0);

    /* methods for marshaling */
    rb_define_method(cOCINumber, "_dump", onum_dump, -1);
    rb_define_singleton_method(cOCINumber, "_load", onum_s_load, 1);

    oci8_define_bind_class("OCINumber", &bind_ocinumber_class);
}

OCINumber *oci8_get_ocinumber(VALUE num)
{
    if (!rb_obj_is_kind_of(num, cOCINumber)) { 
        rb_raise(rb_eTypeError, "invalid argument %s (expect a subclass of %s)", rb_class2name(CLASS_OF(num)), rb_class2name(cOCINumber));
    }
    return _NUMBER(num);
}
