/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  ocinumber.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2005-2008 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
#include <orl.h>
#include <errno.h>

#ifndef RUBY_VM
/* ruby 1.8 */
#define rb_num_coerce_cmp(x, y, id) rb_num_coerce_cmp((x), (y))
#define rb_num_coerce_bin(x, y, id) rb_num_coerce_bin((x), (y))
#endif

static ID id_power; /* rb_intern("**") */
static ID id_cmp;   /* rb_intern("<=>") */

static VALUE cOCINumber;
static OCINumber const_p1;   /*  +1 */
static OCINumber const_p10;  /* +10 */
static OCINumber const_m1;   /*  -1 */
static OCINumber const_PI2;  /* +PI/2 */
static OCINumber const_mPI2; /* -PI/2 */

#define SHRESHOLD_FMT_STR "00000000000000000000000000"
#define SHRESHOLD_FMT (OraText*)SHRESHOLD_FMT_STR
#define SHRESHOLD_FMT_LEN (sizeof(SHRESHOLD_FMT_STR) - 1)
#define SHRESHOLD_VAL_STR "10000000000000000000000000"
#define SHRESHOLD_VAL (OraText*)SHRESHOLD_VAL_STR
#define SHRESHOLD_VAL_LEN (sizeof(SHRESHOLD_VAL_STR) - 1)
static OCINumber const_shreshold;

/* TODO: scale: -84 - 127 */
#define NUMBER_FORMAT1_STR "FM9999999999999999999999990.9999999999999999999999999999999999999"
#define NUMBER_FORMAT1 (OraText*)NUMBER_FORMAT1_STR
#define NUMBER_FORMAT1_LEN (sizeof(NUMBER_FORMAT1_STR) - 1)
#define NUMBER_FORMAT2_STR "FM99999999999999999999999999999999999990.999999999999999999999999"
#define NUMBER_FORMAT2 (OraText*)NUMBER_FORMAT2_STR
#define NUMBER_FORMAT2_LEN (sizeof(NUMBER_FORMAT2_STR) - 1)
#define NUMBER_FORMAT2_DECIMAL                              (sizeof("999999999999999999999999") - 1)
#define NUMBER_FORMAT_INT_STR "FM99999999999999999999999999999999999990"
#define NUMBER_FORMAT_INT (OraText*)NUMBER_FORMAT_INT_STR
#define NUMBER_FORMAT_INT_LEN (sizeof(NUMBER_FORMAT_INT_STR) - 1)

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
VALUE oci8_make_ocinumber(OCINumber *s)
{
    VALUE obj;
    OCINumber *d;

    obj = Data_Make_Struct(cOCINumber, OCINumber, NULL, xfree, d);
    oci_lc(OCINumberAssign(oci8_errhp, s, d));
    return obj;
}

VALUE oci8_make_integer(OCINumber *s)
{
    signed long sl;
    char buf[512];
    ub4 buf_size = sizeof(buf);

    if (OCINumberToInt(oci8_errhp, s, sizeof(sl), OCI_NUMBER_SIGNED, &sl) == OCI_SUCCESS) {
        return LONG2NUM(sl);
    }
    oci_lc(OCINumberToText(oci8_errhp, s, NUMBER_FORMAT2, NUMBER_FORMAT2_LEN,
                           NULL, 0, &buf_size, TO_ORATEXT(buf)));
    return rb_cstr2inum(buf, 10);
}

VALUE oci8_make_float(OCINumber *s)
{
    double dbl;

    oci_lc(OCINumberToReal(oci8_errhp, s, sizeof(double), &dbl));
    return rb_float_new(dbl);
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
        for (i = RSTRING_LEN(str) - 1; i >= 0; i--) {
            if (RSTRING_PTR(str)[i] != ' ')
                cnt++;
            if (RSTRING_PTR(str)[i] == '.') {
                i = RSTRING_LEN(str) - i;
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
        fmt_ptr = RSTRING_ORATEXT(fmt);
        fmt_len = RSTRING_LEN(fmt);
    }
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING_ORATEXT(nls_params);
        nls_params_len = RSTRING_LEN(nls_params);
    }
    oci_lc(OCINumberFromText(oci8_errhp,
                             RSTRING_ORATEXT(str), RSTRING_LEN(str),
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

OCINumber *oci8_set_ocinumber(OCINumber *result, VALUE self)
{
    set_oci_number_from_num(result, self, 1);
    return result;
}
#define TO_OCINUM(on, val) oci8_set_ocinumber((on), (val))

OCINumber *oci8_set_integer(OCINumber *result, VALUE self)
{
    OCINumber work;
    set_oci_number_from_num(&work, self, 1);
    oci_lc(OCINumberTrunc(oci8_errhp, &work, 0, result));
    return result;
}

/*
 *  call-seq:
 *     OCI8::Math.atan2(y, x) -> oranumber
 *
 *  Computes the arc tangent given <i>y</i> and <i>x</i>. Returns
 *  -PI..PI.
 */
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
            return oci8_make_ocinumber(&const_PI2); /* atan2(positive, 0) => PI/2 */
        case -1:
            return oci8_make_ocinumber(&const_mPI2); /* atan2(negative, 0) => -PI/2 */
        }
    }
    /* atan2 */
    oci_lc(OCINumberArcTan2(oci8_errhp, &nY, &nX, &rv));
    return oci8_make_ocinumber(&rv);
}

/*
 *  call-seq:
 *     OCI8::Math.cos(x) -> oranumber
 *
 *  Computes the cosine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static VALUE omath_cos(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberCos(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return oci8_make_ocinumber(&rv);
}

/*
 *  call-seq:
 *     OCI8::Math.sin(x)    -> oranumber
 *
 *  Computes the sine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static VALUE omath_sin(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberSin(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return oci8_make_ocinumber(&rv);
}

/*
 *  call-seq:
 *     OCI8::Math.tan(x)    -> oranumber
 *
 *  Returns the tangent of <i>x</i> (expressed in radians).
 */
static VALUE omath_tan(VALUE obj, VALUE radian)
{
    OCINumber r;
    OCINumber rv;

    oci_lc(OCINumberTan(oci8_errhp, TO_OCINUM(&r, radian), &rv));
    return oci8_make_ocinumber(&rv);
}

/*
 *  call-seq:
 *     OCI8::Math.acos(x)    -> oranumber
 *
 *  Computes the arc cosine of <i>x</i>. Returns 0..PI.
 */
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
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.asin(x)    -> oranumber
 *
 *  Computes the arc sine of <i>x</i>. Returns 0..PI.
 */
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
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.atan(x)    -> oranumber
 *
 *  Computes the arc tangent of <i>x</i>. Returns -{PI/2} .. {PI/2}.
 */
static VALUE omath_atan(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberArcTan(oci8_errhp, TO_OCINUM(&n, num), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.cosh(x)    -> oranumber
 *
 *  Computes the hyperbolic cosine of <i>x</i> (expressed in radians).
 */
static VALUE omath_cosh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypCos(oci8_errhp, TO_OCINUM(&n, num), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.sinh(x)    -> oranumber
 *
 *  Computes the hyperbolic sine of <i>x</i> (expressed in
 *  radians).
 */
static VALUE omath_sinh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypSin(oci8_errhp, TO_OCINUM(&n, num), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.tanh()    -> oranumber
 *
 *  Computes the hyperbolic tangent of <i>x</i> (expressed in
 *  radians).
 */
static VALUE omath_tanh(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberHypTan(oci8_errhp, TO_OCINUM(&n, num), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.exp(x)    -> oranumber
 *
 *  Returns e**x.
 */
static VALUE omath_exp(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;

    oci_lc(OCINumberExp(oci8_errhp, TO_OCINUM(&n, num), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.log(numeric)    -> oranumber
 *     OCI8::Math.log(numeric, base_num)  -> oranumber
 *
 *  Returns the natural logarithm of <i>numeric</i> for one argument.
 *  Returns the base <i>base_num</i> logarithm of <i>numeric</i> for two arguments.
 */
static VALUE omath_log(int argc, VALUE *argv, VALUE obj)
{
    VALUE num, base;
    OCINumber n;
    OCINumber b;
    OCINumber r;
    sword sign;

    rb_scan_args(argc, argv, "11", &num, &base);
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
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.log10(numeric)    -> oranumber
 *
 *  Returns the base 10 logarithm of <i>numeric</i>.
 */
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
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     OCI8::Math.sqrt(numeric)    -> oranumber
 *
 *  Returns the non-negative square root of <i>numeric</i>.
 */
static VALUE omath_sqrt(VALUE obj, VALUE num)
{
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1);
    /* check whether num is negative */
    oci_lc(OCINumberSign(oci8_errhp, &n, &sign));
    if (sign < 0) {
        errno = EDOM;
        rb_sys_fail("sqrt");
    }
    oci_lc(OCINumberSqrt(oci8_errhp, &n, &r));
    return oci8_make_ocinumber(&r);
}

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

static VALUE onum_initialize_copy(VALUE lhs, VALUE rhs)
{
    if (!RTEST(rb_obj_is_instance_of(rhs, CLASS_OF(lhs)))) {
        rb_raise(rb_eTypeError, "invalid type: expected %s but %s",
                 rb_class2name(CLASS_OF(lhs)), rb_class2name(CLASS_OF(rhs)));
    }
    oci_lc(OCINumberAssign(oci8_errhp, _NUMBER(rhs), _NUMBER(lhs)));
    return lhs;
}

static VALUE onum_coerce(VALUE self, VALUE other)
{
    OCINumber n;

    if (RTEST(rb_obj_is_kind_of(other, rb_cNumeric)))
        if (set_oci_number_from_num(&n, other, 0))
            return rb_assoc_new(oci8_make_ocinumber(&n), self);
    rb_raise(rb_eTypeError, "Can't coerce %s to %s",
             rb_class2name(CLASS_OF(other)), rb_class2name(cOCINumber));
}

/*
 *  call-seq:
 *     -onum   -> oranumber
 *
 *  Returns an <code>OraNumber</code>, negated.
 */
static VALUE onum_neg(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberNeg(oci8_errhp, _NUMBER(self), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum + other   -> oranumber
 *
 *  Returns a new <code>OraNumber</code> which is the sum of <i>onum</i>
 *  and <i>other</i>.
 */
static VALUE onum_add(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs, '+');
    /* add */
    oci_lc(OCINumberAdd(oci8_errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum - other   -> oranumber
 *
 *  Returns a new <code>OraNumber</code> which is the difference of <i>onum</i>
 *  and <i>other</i>.
 */
static VALUE onum_sub(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs, '-');
    /* subtracting */
    oci_lc(OCINumberSub(oci8_errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum * other   -> oranumber
 *
 *  Returns a new <code>OraNumber</code> which is the product of <i>onum</i>
 *  and <i>other</i>.
 */
static VALUE onum_mul(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs, '*');
    /* multiply */
    oci_lc(OCINumberMul(oci8_errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum / other   -> oranumber
 *
 *  Returns a new <code>OraNumber</code> which is the result of dividing
 *  <i>onum</i> by <i>other</i>.
 */
static VALUE onum_div(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs, '/');
    /* check whether argument is not zero. */
    oci_lc(OCINumberIsZero(oci8_errhp, &n, &is_zero));
    if (is_zero)
        rb_num_zerodiv();
    /* division */
    oci_lc(OCINumberDiv(oci8_errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum % other   -> oranumber
 *
 *  Returns the modulo after division of <i>onum</i> by <i>other</i>.
 */
static VALUE onum_mod(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_bin(lhs, rhs, '%');
    /* check whether argument is not zero. */
    oci_lc(OCINumberIsZero(oci8_errhp, &n, &is_zero));
    if (is_zero)
        rb_num_zerodiv();
    /* modulo */
    oci_lc(OCINumberMod(oci8_errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum ** other   -> oranumber
 *
 *  Raises <i>onum</i> the <i>other</i> power.
 */
static VALUE onum_power(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    OCINumber r;

    if (FIXNUM_P(rhs)) {
        oci_lc(OCINumberIntPower(oci8_errhp, _NUMBER(lhs), FIX2INT(rhs), &r));
    } else {
        /* change to OCINumber */
        if (!set_oci_number_from_num(&n, rhs, 0))
            return rb_num_coerce_bin(lhs, rhs, id_power);
        oci_lc(OCINumberPower(oci8_errhp, _NUMBER(lhs), &n, &r));
    }
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *    onum <=> other   -> -1, 0, +1
 *
 *  Returns -1, 0, or +1 depending on whether <i>onum</i> is less than,
 *  equal to, or greater than <i>other</i>. This is the basis for the
 *  tests in <code>Comparable</code>.
 */
static VALUE onum_cmp(VALUE lhs, VALUE rhs)
{
    OCINumber n;
    sword r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0))
        return rb_num_coerce_cmp(lhs, rhs, id_cmp);
    /* compare */
    oci_lc(OCINumberCmp(oci8_errhp, _NUMBER(lhs), &n, &r));
    if (r > 0) {
        return INT2FIX(1);
    } else if (r == 0) {
        return INT2FIX(0);
    } else {
        return INT2FIX(-1);
    }
}

/*
 *  call-seq:
 *     onum.floor   -> integer
 *
 *  Returns the largest <code>Integer</code> less than or equal to <i>onum</i>.
 */
static VALUE onum_floor(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberFloor(oci8_errhp, _NUMBER(self), &r));
    return oci8_make_integer(&r);
}

/*
 *  call-seq:
 *     onum.ceil    -> integer
 *
 *  Returns the smallest <code>Integer</code> greater than or equal to
 *  <i>onum</i>.
 */
static VALUE onum_ceil(VALUE self)
{
    OCINumber r;

    oci_lc(OCINumberCeil(oci8_errhp, _NUMBER(self), &r));
    return oci8_make_integer(&r);
}

/*
 *  call-seq:
 *     onum.round      -> integer
 *     onum.round(decplace) -> oranumber
 *
 *  Rounds <i>onum</i> to the nearest <code>Integer</code> when no argument.
 *  Rounds <i>onum</i> to a specified decimal place <i>decplace</i> when one argument.
 *
 *   OraNumber.new(1.234).round(1)  #=> 1.2
 *   OraNumber.new(1.234).round(2)  #=> 1.23
 *   OraNumber.new(1.234).round(3)  #=> 1.234
 */
static VALUE onum_round(int argc, VALUE *argv, VALUE self)
{
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    oci_lc(OCINumberRound(oci8_errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    if (argc == 0) {
        return oci8_make_integer(&r);
    } else {
        return oci8_make_ocinumber(&r);
    }
}

/*
 *  call-seq:
 *     onum.truncate     -> integer
 *     onum.truncate(decplace) -> oranumber
 *
 *  Truncates <i>onum</i> to the <code>Integer</code> when no argument.
 *  Truncates <i>onum</i> to a specified decimal place <i>decplace</i> when one argument.
 */
static VALUE onum_trunc(int argc, VALUE *argv, VALUE self)
{
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    oci_lc(OCINumberTrunc(oci8_errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     onum.round_prec(digits) -> oranumber
 *
 *  Rounds <i>onum</i> to a specified number of decimal digits.
 *  This method is available on Oracle 8.1 client or upper.
 *
 *   OraNumber.new(1.234).round_prec(2)  #=> 1.2
 *   OraNumber.new(12.34).round_prec(2)  #=> 12
 *   OraNumber.new(123.4).round_prec(2)  #=> 120
 */
static VALUE onum_round_prec(VALUE self, VALUE ndigs)
{
    OCINumber r;

    oci_lc(OCINumberPrec(oci8_errhp, _NUMBER(self), NUM2INT(ndigs), &r));
    return oci8_make_ocinumber(&r);
}

/*
 *  call-seq:
 *     onum.to_char(fmt = nil, nls_params = nil)  -> string
 *
 *  Returns a string containing a representation of self.
 *  <i>fmt</i> and <i>nls_params</i> are same meanings with
 *  <code>TO_CHAR</code> of Oracle function.
 */
static VALUE onum_to_char(int argc, VALUE *argv, VALUE self)
{
    VALUE fmt;
    VALUE nls_params;
    char buf[512];
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
        fmt_ptr = RSTRING_ORATEXT(fmt);
        fmt_len = RSTRING_LEN(fmt);
    }
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING_ORATEXT(nls_params);
        nls_params_len = RSTRING_LEN(nls_params);
    }
    rv = OCINumberToText(oci8_errhp, _NUMBER(self),
                         fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                         &buf_size, TO_ORATEXT(buf));
    if (rv == OCI_ERROR) {
        sb4 errcode;
        OCIErrorGet(oci8_errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
        if (errcode == 22065) {
            /* OCI-22065: number to text translation for the given format causes overflow */
            if (NIL_P(fmt)) /* implicit conversion */
                return rb_usascii_str_new_cstr("overflow");
        }
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return rb_usascii_str_new(buf, buf_size);
}

/*
 *  call-seq:
 *     onum.to_s    -> string
 *
 *  Returns a string containing a representation of self.
 */
static VALUE onum_to_s(VALUE self)
{
    return onum_to_char(0, NULL, self);
}

/*
 *  call-seq:
 *     onum.to_i       -> integer
 *
 *  Returns <i>onm</i> truncated to an <code>Integer</code>.
 */
static VALUE onum_to_i(VALUE self)
{
    OCINumber num;

    oci_lc(OCINumberTrunc(oci8_errhp, _NUMBER(self), 0, &num));
    return oci8_make_integer(&num);
}

/*
 *  call-seq:
 *     onum.to_f -> float
 *
 *  Converts <i>onum</i> to a <code>Float</code>.
 *
 */
static VALUE onum_to_f(VALUE self)
{
    double dbl;

    oci_lc(OCINumberToReal(oci8_errhp, _NUMBER(self), sizeof(dbl), &dbl));
    return rb_float_new(dbl);
}

/*
 *  call-seq:
 *     onum.to_onum -> oranumber
 *
 */
static VALUE onum_to_onum(VALUE self)
{
    return self;
}

/*
 *  call-seq:
 *     onum.zero?    -> true or false
 *
 *  Returns <code>true</code> if <i>onum</i> is zero.
 *
 */
static VALUE onum_zero_p(VALUE self)
{
    boolean result;

    oci_lc(OCINumberIsZero(oci8_errhp, _NUMBER(self), &result));
    return result ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     onum.abs -> oranumber
 *
 *  Returns the absolute value of <i>onum</i>.
 *
 */
static VALUE onum_abs(VALUE self)
{
    OCINumber result;

    oci_lc(OCINumberAbs(oci8_errhp, _NUMBER(self), &result));
    return oci8_make_ocinumber(&result);
}

/*
 *  call-seq:
 *     onum.shift(fixnum)    -> oranumber
 *
 *  Returns <i>onum</i> * 10**<i>fixnum</i>
 *  This method is available on Oracle 8.1 client or upper.
 */
static VALUE onum_shift(VALUE self, VALUE exp)
{
    OCINumber result;

    oci_lc(OCINumberShift(oci8_errhp, _NUMBER(self), NUM2INT(exp), &result));
    return oci8_make_ocinumber(&result);
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
    const char *name = rb_class2name(CLASS_OF(self));
    volatile VALUE s = onum_to_s(self);
    size_t len = strlen(name) + RSTRING_LEN(s) + 5;
    char *str = ALLOCA_N(char, len);

    snprintf(str, len, "#<%s:%s>", name, RSTRING_PTR(s));
    str[len - 1] = '\0';
    return rb_usascii_str_new_cstr(str);
}

/*
 *  call-seq:
 *    onum._dump   -> string
 *
 *  Dump <i>onum</i> for marshaling.
 */
static VALUE onum_dump(int argc, VALUE *argv, VALUE self)
{
    char *c  = DATA_PTR(self);
    int size = c[0] + 1;
    VALUE dummy;

    rb_scan_args(argc, argv, "01", &dummy);
    return rb_str_new(c, size);
}

/*
 *  call-seq:
 *    OraNumber._load(string)   -> oranumber
 *
 *  Unmarshal a dumped <code>OraNumber</code> object.
 */
static VALUE
onum_s_load(VALUE klass, VALUE str)
{
    unsigned char *c;
    int size;
    OCINumber num;

    Check_Type(str, T_STRING);
    c = RSTRING_ORATEXT(str);
    size = RSTRING_LEN(str);
    if (size == 0 || size != c[0] + 1 || size > sizeof(num)) {
        rb_raise(rb_eTypeError, "marshaled OCI::Number format differ");
    }
    memset(&num, 0, sizeof(num));
    memcpy(&num, c, size);
    return oci8_make_ocinumber(&num);
}

/*
 * bind_ocinumber
 */
static VALUE bind_ocinumber_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return oci8_make_ocinumber((OCINumber*)data);
}

static VALUE bind_integer_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return oci8_make_integer((OCINumber*)data);
}

static void bind_ocinumber_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    set_oci_number_from_num((OCINumber*)data, val, 1);
}

static void bind_integer_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    OCINumber num;

    set_oci_number_from_num(&num, val, 1);
    oci_lc(OCINumberTrunc(oci8_errhp, &num, 0, (OCINumber*)data));
}

static void bind_ocinumber_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(OCINumber);
    obind->alloc_sz = sizeof(OCINumber);
}

static void bind_ocinumber_init_elem(oci8_bind_t *obind, VALUE svc)
{
    ub4 idx = 0;

    do {
        OCINumberSetZero(oci8_errhp, (OCINumber*)obind->valuep + idx);
    } while (++idx < obind->maxar_sz);
}

static const oci8_bind_class_t bind_ocinumber_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_ocinumber_get,
    bind_ocinumber_set,
    bind_ocinumber_init,
    bind_ocinumber_init_elem,
    NULL,
    NULL,
    NULL,
    SQLT_VNU,
};

static const oci8_bind_class_t bind_integer_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_integer_get,
    bind_integer_set,
    bind_ocinumber_init,
    bind_ocinumber_init_elem,
    NULL,
    NULL,
    NULL,
    SQLT_VNU,
};

void
Init_oci_number(VALUE cOCI8)
{
    VALUE mMath;
    OCINumber num1, num2;
    VALUE obj_PI;
    signed long sl;

    id_power = rb_intern("**");
    id_cmp = rb_intern("<=>");

    cOCINumber = rb_define_class("OraNumber", rb_cNumeric);
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
    obj_PI = oci8_make_ocinumber(&num1);

    /* The ratio of the circumference of a circle to its diameter. */
    rb_define_const(mMath, "PI", obj_PI);

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

    rb_define_alloc_func(cOCINumber, onum_s_alloc);

    /* methods of OCI::Number */
    rb_define_method_nodoc(cOCINumber, "initialize", onum_initialize, -1);
    rb_define_method_nodoc(cOCINumber, "initialize_copy", onum_initialize_copy, 1);
    rb_define_method_nodoc(cOCINumber, "coerce", onum_coerce, 1);

    rb_include_module(cOCINumber, rb_mComparable);

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
    rb_define_method(cOCINumber, "truncate", onum_trunc, -1);
    if (have_OCINumberPrec) {
        rb_define_method(cOCINumber, "round_prec", onum_round_prec, 1);
    }

    rb_define_method(cOCINumber, "to_s", onum_to_s, 0);
    rb_define_method(cOCINumber, "to_char", onum_to_char, -1);
    rb_define_method(cOCINumber, "to_i", onum_to_i, 0);
    rb_define_method(cOCINumber, "to_f", onum_to_f, 0);
    rb_define_method_nodoc(cOCINumber, "to_onum", onum_to_onum, 0);

    rb_define_method(cOCINumber, "zero?", onum_zero_p, 0);
    rb_define_method(cOCINumber, "abs", onum_abs, 0);
    if (have_OCINumberShift) {
        rb_define_method(cOCINumber, "shift", onum_shift, 1);
    }

    rb_define_method_nodoc(cOCINumber, "hash", onum_hash, 0);
    rb_define_method_nodoc(cOCINumber, "inspect", onum_inspect, 0);

    /* methods for marshaling */
    rb_define_method(cOCINumber, "_dump", onum_dump, -1);
    rb_define_singleton_method(cOCINumber, "_load", onum_s_load, 1);

    oci8_define_bind_class("OraNumber", &bind_ocinumber_class);
    oci8_define_bind_class("Integer", &bind_integer_class);
}

OCINumber *oci8_get_ocinumber(VALUE num)
{
    if (!rb_obj_is_kind_of(num, cOCINumber)) {
        rb_raise(rb_eTypeError, "invalid argument %s (expect a subclass of %s)", rb_class2name(CLASS_OF(num)), rb_class2name(cOCINumber));
    }
    return _NUMBER(num);
}
