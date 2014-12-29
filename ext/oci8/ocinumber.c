/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  ocinumber.c
 *
 * Copyright (C) 2005-2014 Kubo Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"
#include <orl.h>
#include <errno.h>
#include <math.h>
#include "oranumber_util.h"

#ifndef RB_NUM_COERCE_FUNCS_NEED_OPID
/* ruby 1.8 */
#define rb_num_coerce_cmp(x, y, id) rb_num_coerce_cmp((x), (y))
#define rb_num_coerce_bin(x, y, id) rb_num_coerce_bin((x), (y))
#endif

int oci8_float_conversion_type_is_ruby = 1;

#ifndef INFINITY
#define INFINITY (1.0/+0.0)
#endif

static ID id_power; /* rb_intern("**") */
static ID id_cmp;   /* rb_intern("<=>") */
static ID id_finite_p;
static ID id_split;
static ID id_numerator;
static ID id_denominator;
static ID id_Rational;
static ID id_BigDecimal;

#ifndef rb_Rational2
static VALUE cRational;
#endif
static VALUE cBigDecimal;

static VALUE cOCINumber;
static OCINumber const_p1;   /*  +1 */
static OCINumber const_p10;  /* +10 */
static OCINumber const_m1;   /*  -1 */
static OCINumber const_PI2;  /* +PI/2 */
static OCINumber const_mPI2; /* -PI/2 */

/* Dangerous macro! Don't use this if you are unsure that the val is an OCINumber. */
#define _NUMBER(val) ((OCINumber *)RTYPEDDATA_DATA(val))

#ifndef T_MASK
#define T_MASK 0x100 /* TODO: rboci8_type() should be changed to be more portable. */
#endif
#define RBOCI8_T_ORANUMBER (T_MASK + 1)
#define RBOCI8_T_BIGDECIMAL (T_MASK + 2)
#ifdef rb_Rational2
#define RBOCI8_T_RATIONAL T_RATIONAL
#else
#define RBOCI8_T_RATIONAL (T_MASK + 3)
#endif

static int rboci8_type(VALUE obj)
{
    int type = TYPE(obj);
    VALUE klass;

    switch (type) {
#ifndef rb_Rational2
    case T_OBJECT:
        klass = CLASS_OF(obj);
        if (cRational != 0) {
            if (klass == cRational) {
                return RBOCI8_T_RATIONAL;
            }
        } else {
            if (strcmp(rb_class2name(klass), "Rational") == 0) {
                cRational = rb_const_get(rb_cObject, id_Rational);
                return RBOCI8_T_RATIONAL;
            }
        }
        break;
#endif
    case T_DATA:
        klass = CLASS_OF(obj);
        if (klass == cOCINumber) {
            return RBOCI8_T_ORANUMBER;
        }
        if (cBigDecimal != 0) {
            if (klass == cBigDecimal) {
                return RBOCI8_T_BIGDECIMAL;
            }
        } else {
            if (strcmp(rb_class2name(klass), "BigDecimal") == 0) {
                cBigDecimal = rb_const_get(rb_cObject, id_BigDecimal);
                return RBOCI8_T_BIGDECIMAL;
            }
        }
    }
    return type;
}

static VALUE onum_to_f(VALUE self);
static VALUE onum_to_r(VALUE self);
static VALUE onum_to_d(VALUE self);
static VALUE onum_to_d_real(OCINumber *num, OCIError *errhp);

static size_t onum_memsize(const void *ptr)
{
    return sizeof(OCINumber);
}

static const rb_data_type_t onum_data_type = {
    "OraNumber",
    {NULL, RUBY_DEFAULT_FREE, onum_memsize,},
#ifdef RUBY_TYPED_FREE_IMMEDIATELY
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
#ifdef RUBY_TYPED_WB_PROTECTED
    | RUBY_TYPED_WB_PROTECTED
#endif
};

static VALUE onum_s_alloc(VALUE klass)
{
    VALUE obj;
    OCINumber *d;

    obj = TypedData_Make_Struct(klass, OCINumber, &onum_data_type, d);
    OCINumberSetZero(oci8_errhp, d);
    return obj;
}

/*
 * OCINumber (C datatype) -> OraNumber (ruby object)
 */
VALUE oci8_make_ocinumber(OCINumber *s, OCIError *errhp)
{
    VALUE obj;
    OCINumber *d;

    obj = TypedData_Make_Struct(cOCINumber, OCINumber, &onum_data_type, d);
    chkerr(OCINumberAssign(errhp, s, d));
    return obj;
}

/*
 * OCINumber (C datatype) -> Integer (ruby object)
 */
VALUE oci8_make_integer(OCINumber *s, OCIError *errhp)
{
    signed long sl;
    char buf[512];
    sword rv;

    if (OCINumberToInt(errhp, s, sizeof(sl), OCI_NUMBER_SIGNED, &sl) == OCI_SUCCESS) {
        return LONG2NUM(sl);
    }
    /* convert to Integer via String */
    rv = oranumber_to_str(s, buf, sizeof(buf));
    if (rv > 0) {
        return rb_cstr2inum(buf, 10);
    }
    oranumber_dump(s, buf);
    rb_raise(eOCIException, "Invalid internal number format: %s", buf);
}

/*
 * OCINumber (C datatype) -> Float (ruby object)
 */
VALUE oci8_make_float(OCINumber *s, OCIError *errhp)
{
    return rb_float_new(oci8_onum_to_dbl(s, errhp));
}

/*
 * String (ruby object) -> OCINumber (C datatype)
 */
static void set_oci_number_from_str(OCINumber *result, VALUE str, VALUE fmt, VALUE nls_params, OCIError *errhp)
{
    oratext *fmt_ptr;
    oratext *nls_params_ptr;
    ub4 fmt_len;
    ub4 nls_params_len;

    StringValue(str);
    /* set from string. */
    if (NIL_P(fmt)) {
        int rv = oranumber_from_str(result, RSTRING_PTR(str), RSTRING_LEN(str));
        if (rv == ORANUMBER_SUCCESS) {
            return; /* success */
        } else {
            const char *default_msg = NULL;
            switch (rv) {
            case ORANUMBER_INVALID_NUMBER:
                default_msg = "invalid number";
                break;
            case ORANUMBER_NUMERIC_OVERFLOW:
                default_msg = "numeric overflow";
                break;
            }
            oci8_raise_by_msgno(rv, default_msg);
        }
    }
    StringValue(fmt);
    fmt_ptr = RSTRING_ORATEXT(fmt);
    fmt_len = RSTRING_LEN(fmt);
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING_ORATEXT(nls_params);
        nls_params_len = RSTRING_LEN(nls_params);
    }
    chkerr(OCINumberFromText(errhp,
                             RSTRING_ORATEXT(str), RSTRING_LEN(str),
                             fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                             result));
}

/*
 * Numeric (ruby object) -> OCINumber (C datatype)
 *
 * @return 1 on success. Otherwise, 0.
 */
static int set_oci_number_from_num(OCINumber *result, VALUE num, int force, OCIError *errhp)
{
    signed long sl;

    if (!RTEST(rb_obj_is_kind_of(num, rb_cNumeric)))
        rb_raise(rb_eTypeError, "expect Numeric but %s", rb_class2name(CLASS_OF(num)));
    if (rb_respond_to(num, id_finite_p) && !RTEST(rb_funcall(num, id_finite_p, 0))) {
        rb_raise(rb_eTypeError, "cannot accept number which isn't finite.");
    }
    switch (rb_type(num)) {
    case T_FIXNUM:
        /* set from long. */
        sl = NUM2LONG(num);
        chkerr(OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, result));
        return 1;
    case T_FLOAT:
        /* set from double. */
        oci8_dbl_to_onum(result, NUM2DBL(num), errhp);
        return 1;
    case T_BIGNUM:
        /* change via string. */
        num = rb_big2str(num, 10);
        set_oci_number_from_str(result, num, Qnil, Qnil, errhp);
        return 1;
    }
    if (RTEST(rb_obj_is_instance_of(num, cOCINumber))) {
        /* OCI::Number */
        chkerr(OCINumberAssign(errhp, _NUMBER(num), result));
        return 1;
    }
    if (rb_respond_to(num, id_split)) {
        /* BigDecimal */
        volatile VALUE split = rb_funcall(num, id_split, 0);

        if (TYPE(split) == T_ARRAY && RARRAY_LEN(split) == 4) {
            /*
             * sign, significant_digits, base, exponent = num.split
             * onum = sign * "0.#{significant_digits}".to_f * (base ** exponent)
             */
            const VALUE *ary = RARRAY_CONST_PTR(split);
            int sign;
            OCINumber digits;
            int exponent;
            int digits_len;
            OCINumber work;

            /* check sign */
            if (TYPE(ary[0]) != T_FIXNUM) {
                goto is_not_big_decimal;
            }
            sign = FIX2INT(ary[0]);
            /* check digits */
            if (TYPE(ary[1]) != T_STRING) {
                goto is_not_big_decimal;
            }
            digits_len = RSTRING_LEN(ary[1]);
            set_oci_number_from_str(&digits, ary[1], Qnil, Qnil, errhp);
            /* check base */
            if (TYPE(ary[2]) != T_FIXNUM || FIX2LONG(ary[2]) != 10) {
                goto is_not_big_decimal;
            }
            /* check exponent */
            if (TYPE(ary[3]) != T_FIXNUM) {
                goto is_not_big_decimal;
            }
            exponent = FIX2INT(ary[3]);

            chkerr(OCINumberShift(errhp, &digits, exponent - digits_len, &work));
            if (sign >= 0) {
                chkerr(OCINumberAssign(errhp, &work, result));
            } else {
                chkerr(OCINumberNeg(errhp, &work, result));
            }
            return 1;
        }
    }
is_not_big_decimal:
    if (rb_respond_to(num, id_numerator) && rb_respond_to(num, id_denominator)) {
        /* Rational */
        OCINumber numerator;
        OCINumber denominator;

        if (set_oci_number_from_num(&numerator, rb_funcall(num, id_numerator, 0), 0, errhp) &&
            set_oci_number_from_num(&denominator, rb_funcall(num, id_denominator, 0), 0, errhp)) {
            chkerr(OCINumberDiv(errhp, &numerator, &denominator, result));
            return 1;
        }
    }
    if (force) {
        /* change via string as a last resort. */
        /* TODO: if error, raise TypeError instead of OCI::Error */
        set_oci_number_from_str(result, num, Qnil, Qnil, errhp);
        return 1;
    }
    return 0;
}

/*
 * Numeric (ruby object) -> OCINumber (C datatype)
 */
OCINumber *oci8_set_ocinumber(OCINumber *result, VALUE self, OCIError *errhp)
{
    set_oci_number_from_num(result, self, 1, errhp);
    return result;
}
#define TO_OCINUM oci8_set_ocinumber

/*
 * Numeric (ruby object) -> OCINumber (C datatype) as an integer
 */
OCINumber *oci8_set_integer(OCINumber *result, VALUE self, OCIError *errhp)
{
    OCINumber work;

    set_oci_number_from_num(&work, self, 1, errhp);
    chkerr(OCINumberTrunc(errhp, &work, 0, result));
    return result;
}

/*
 * OCINumber (C datatype) -> double (C datatype)
 */
double oci8_onum_to_dbl(OCINumber *s, OCIError *errhp)
{
    if (oci8_float_conversion_type_is_ruby) {
        char buf[256];
        sword rv;

        rv = oranumber_to_str(s, buf, sizeof(buf));
        if (rv <= 0) {
            char buf[ORANUMBER_DUMP_BUF_SIZ];

            oranumber_dump(s, buf);
            rb_raise(eOCIException, "Invalid internal number format: %s", buf);
        }
        if (strcmp(buf, "~") == 0) {
            return INFINITY;
        } else if (strcmp(buf, "-~") == 0) {
            return -INFINITY;
        }
        return rb_cstr_to_dbl(buf, Qtrue);
    } else {
        double dbl;

        chkerr(OCINumberToReal(errhp, s, sizeof(double), &dbl));
        return dbl;
    }
}

/*
 * double (C datatype) -> OCINumber (C datatype)
 */
OCINumber *oci8_dbl_to_onum(OCINumber *result, double dbl, OCIError *errhp)
{
    if (isnan(dbl)) {
        rb_raise(rb_eFloatDomainError, "NaN");
        /* never reach here */
    } else if (isinf(dbl)) {
        if (dbl > 0.0) {
            oranumber_from_str(result, "~", 1);
        } else {
            oranumber_from_str(result, "-~", 2);
        }
        return result;
    }

    if (oci8_float_conversion_type_is_ruby) {
        VALUE str;
        sword rv;

        str = rb_obj_as_string(rb_float_new(dbl));
        rv = oranumber_from_str(result, RSTRING_PTR(str), RSTRING_LEN(str));
        if (rv != 0) {
            oci8_raise_by_msgno(rv, NULL);
        }
    } else {
        chkerr(OCINumberFromReal(errhp, &dbl, sizeof(dbl), result));
    }
    return result;
}

/*
 *  Document-module: OCI8::Math
 *
 *  The <code>OCI8::Math</code> module contains module functions for basic
 *  trigonometric and transcendental functions. Their accuracy is
 *  same with {OraNumber}.
 */

/*
 *  call-seq:
 *    atan2(y, x)
 *
 *  Computes the principal value of the arc tangent of <i>y/x</i>,
 *  using the signs of both arguments to determine the quadrant of the
 *  return value.
 *
 *  @param [Numeric] y
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [-{PI}, {PI}]
 */
static VALUE omath_atan2(VALUE self, VALUE Ycoordinate, VALUE Xcoordinate)
{
    OCIError *errhp = oci8_errhp;
    OCINumber nY;
    OCINumber nX;
    OCINumber rv;
    boolean is_zero;
    sword sign;

    set_oci_number_from_num(&nX, Xcoordinate, 1, errhp);
    set_oci_number_from_num(&nY, Ycoordinate, 1, errhp);
    /* check zero */
    chkerr(OCINumberIsZero(errhp, &nX, &is_zero));
    if (is_zero) {
        chkerr(OCINumberSign(errhp, &nY, &sign));
        switch (sign) {
        case 0:
            return INT2FIX(0); /* atan2(0, 0) => 0 or ERROR? */
        case 1:
            return oci8_make_ocinumber(&const_PI2, errhp); /* atan2(positive, 0) => PI/2 */
        case -1:
            return oci8_make_ocinumber(&const_mPI2, errhp); /* atan2(negative, 0) => -PI/2 */
        }
    }
    /* atan2 */
    chkerr(OCINumberArcTan2(errhp, &nY, &nX, &rv));
    return oci8_make_ocinumber(&rv, errhp);
}

/*
 *  call-seq:
 *     cos(x)
 *
 *  Computes the cosine of <i>x</i>, measured in radians.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [-1, 1]
 */
static VALUE omath_cos(VALUE obj, VALUE radian)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;
    OCINumber rv;

    chkerr(OCINumberCos(errhp, TO_OCINUM(&r, radian, errhp), &rv));
    return oci8_make_ocinumber(&rv, errhp);
}

/*
 *  call-seq:
 *     sin(x)
 *
 *  Computes the sine of <i>x</i>, measured in radians.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [-1, 1]
 */
static VALUE omath_sin(VALUE obj, VALUE radian)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;
    OCINumber rv;

    chkerr(OCINumberSin(errhp, TO_OCINUM(&r, radian, errhp), &rv));
    return oci8_make_ocinumber(&rv, errhp);
}

/*
 *  call-seq:
 *     tan(x)
 *
 *  Computes the tangent of <i>x</i>, measured in radians.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_tan(VALUE obj, VALUE radian)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;
    OCINumber rv;

    chkerr(OCINumberTan(errhp, TO_OCINUM(&r, radian, errhp), &rv));
    return oci8_make_ocinumber(&rv, errhp);
}

/*
 *  call-seq:
 *     acos(x)
 *
 *  Computes the principal value of the arc cosine of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [0, {PI}]
 */
static VALUE omath_acos(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1, errhp);
    /* check upper bound */
    chkerr(OCINumberCmp(errhp, &n, &const_p1, &sign));
    if (sign > 0)
        rb_raise(rb_eRangeError, "out of range for acos");
    /* check lower bound */
    chkerr(OCINumberCmp(errhp, &n, &const_m1, &sign));
    if (sign < 0)
        rb_raise(rb_eRangeError, "out of range for acos");
    /* acos */
    chkerr(OCINumberArcCos(errhp, &n, &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     asin(x)
 *
 *  Computes the principal value of the arc sine of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [-{PI}/2, {PI}]/2]
 */
static VALUE omath_asin(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1, errhp);
    /* check upper bound */
    chkerr(OCINumberCmp(errhp, &n, &const_p1, &sign));
    if (sign > 0)
        rb_raise(rb_eRangeError, "out of range for asin");
    /* check lower bound */
    chkerr(OCINumberCmp(errhp, &n, &const_m1, &sign));
    if (sign < 0)
        rb_raise(rb_eRangeError, "out of range for asin");
    /* asin */
    chkerr(OCINumberArcSin(errhp, &n, &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     atan(x)
 *
 *  Computes the principal value of the arc tangent of their argument <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]  Computed value in the range [-{PI}/2, {PI}/2]
 */
static VALUE omath_atan(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    chkerr(OCINumberArcTan(errhp, TO_OCINUM(&n, num, errhp), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     cosh(x)
 *
 *  Computes the hyperbolic cosine of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_cosh(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    chkerr(OCINumberHypCos(errhp, TO_OCINUM(&n, num, errhp), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     sinh(x)
 *
 *  Computes the hyperbolic sine of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_sinh(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    chkerr(OCINumberHypSin(errhp, TO_OCINUM(&n, num, errhp), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     tanh(x)
 *
 *  Computes the hyperbolic tangent of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_tanh(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    chkerr(OCINumberHypTan(errhp, TO_OCINUM(&n, num, errhp), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     exp(x)
 *
 *  Computes the base- <i>e</i> exponential of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_exp(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    chkerr(OCINumberExp(errhp, TO_OCINUM(&n, num, errhp), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  @overload log(x)
 *
 *    Computes the natural logarithm of <i>x</i>.
 *
 *    @param [Numeric] x
 *    @return [OraNumber]
 *
 *  @overload log(x, y)
 *
 *    Computes the base <i>y</I> logarithm of <i>x</i>.
 *
 *    @param [Numeric] x
 *    @param [Numeric] y
 *    @return [OraNumber]
 */
static VALUE omath_log(int argc, VALUE *argv, VALUE obj)
{
    OCIError *errhp = oci8_errhp;
    VALUE num, base;
    OCINumber n;
    OCINumber b;
    OCINumber r;
    sword sign;

    rb_scan_args(argc, argv, "11", &num, &base);
    set_oci_number_from_num(&n, num, 1, errhp);
    chkerr(OCINumberSign(errhp, &n, &sign));
    if (sign <= 0)
        rb_raise(rb_eRangeError, "nonpositive value for log");
    if (NIL_P(base)) {
        chkerr(OCINumberLn(errhp, &n, &r));
    } else {
        set_oci_number_from_num(&b, base, 1, errhp);
        chkerr(OCINumberSign(errhp, &b, &sign));
        if (sign <= 0)
            rb_raise(rb_eRangeError, "nonpositive value for the base of log");
        chkerr(OCINumberCmp(errhp, &b, &const_p1, &sign));
        if (sign == 0)
            rb_raise(rb_eRangeError, "base 1 for log");
        chkerr(OCINumberLog(errhp, &b, &n, &r));
    }
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     log10(x)
 *
 *  Computes the base 10 logarithm of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_log10(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1, errhp);
    chkerr(OCINumberSign(errhp, &n, &sign));
    if (sign <= 0)
        rb_raise(rb_eRangeError, "nonpositive value for log10");
    chkerr(OCINumberLog(errhp, &const_p10, &n, &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     sqrt(x)
 *
 *  Computes the square root of <i>x</i>.
 *
 *  @param [Numeric] x
 *  @return [OraNumber]
 */
static VALUE omath_sqrt(VALUE obj, VALUE num)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    sword sign;

    set_oci_number_from_num(&n, num, 1, errhp);
    /* check whether num is negative */
    chkerr(OCINumberSign(errhp, &n, &sign));
    if (sign < 0) {
        errno = EDOM;
        rb_sys_fail("sqrt");
    }
    chkerr(OCINumberSqrt(errhp, &n, &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  Document-class: OraNumber
 *
 *  OraNumber is a ruby representation of
 *  {http://docs.oracle.com/cd/E11882_01/server.112/e17118/sql_elements001.htm#sthref118 Oracle NUMBER data type}.
 *  without precision and scale designators.
 */

/*
 *  call-seq:
 *    OraNumber(expr = nil, fmt = nil, nlsparam = nil)
 *
 *  Converts <i>expr</i> to a value of OraNumber. The <i>expr</i> can be a Numeric value
 *  or a String value. If it is a String value, optional <i>fmt</i> and <i>nlsparam</i>.
 *  is used as {http://docs.oracle.com/cd/E11882_01/server.112/e17118/functions211.htm Oracle SQL function TO_NUMBER}
 *  does.
 *
 *  @example
 *    # Numeric expr
 *    OraNumber(123456.789)   # -> 123456.789
 *    # String expr
 *    OraNumber('123456.789') # -> 123456.789
 *    # String expr with fmt
 *    OraNumber('123,456.789', '999,999,999.999') # -> 123456.789
 *    # String expr with fmt and nlsparam
 *    OraNumber('123.456,789', '999G999G999D999',  "NLS_NUMERIC_CHARACTERS = ',.'") # -> 123456.789
 *
 *  @param [String, Numeric] expr
 *  @param [String]          fmt
 *  @param [String]          nlsparam
 *  @return [OraNumber]
 *
 *  @since 2.0.3
 */
static VALUE onum_f_new(int argc, VALUE *argv, VALUE self)
{
    VALUE obj = rb_obj_alloc(cOCINumber);
    rb_obj_call_init(obj, argc, argv);
    return obj;
}

/*
 *  call-seq:
 *    initialize(expr = nil, fmt = nil, nlsparam = nil)
 *
 *  Creates a value of OraNumber from <i>expr</i>. The <i>expr</i> can be a Numeric value
 *  or a String value. If it is a String value, optional <i>fmt</i> and <i>nlsparam</i>
 *  is used as {http://docs.oracle.com/cd/E11882_01/server.112/e17118/functions211.htm Oracle SQL function TO_NUMBER}
 *  does.
 *
 *  @example
 *    # Numeric expr
 *    OraNumber.new(123456.789)   # -> 123456.789
 *    # String expr
 *    OraNumber.new('123456.789') # => #<OraNumber:123456.789>
 *    # String expr with fmt
 *    OraNumber.new('123,456.789', '999,999,999.999') # => #<OraNumber:123456.789>
 *    # String expr with fmt and nlsparam
 *    OraNumber.new('123.456,789', '999G999G999D999', "NLS_NUMERIC_CHARACTERS = ',.'") # => #<OraNumber:123456.789>
 *
 *  @param [String, Numeric] expr
 *  @param [String]          fmt
 *  @param [String]          nlsparam
 */
static VALUE onum_initialize(int argc, VALUE *argv, VALUE self)
{
    OCIError *errhp = oci8_errhp;
    VALUE val;
    VALUE fmt;
    VALUE nls_params;

    if (rb_scan_args(argc, argv, "03", &val /* 0 */, &fmt /* nil */, &nls_params /* nil */) == 0) {
        OCINumberSetZero(errhp, _NUMBER(self));
    } else if (RTEST(rb_obj_is_kind_of(val, rb_cNumeric))) {
        set_oci_number_from_num(_NUMBER(self), val, 1, errhp);
    } else {
        set_oci_number_from_str(_NUMBER(self), val, fmt, nls_params, errhp);
    }
    return Qnil;
}

/*
 *  call-seq:
 *    initialize_copy(obj)
 *
 *  Replaces <i>self</i> with <i>obj</i>. <code>Object#clone</code> and <code>Object#dup</code>
 *  call this method to copy data unknown by the ruby interpreter.
 *
 *  @param [OraNumber] obj
 *
 *  @private
 */
static VALUE onum_initialize_copy(VALUE lhs, VALUE rhs)
{
    if (!RTEST(rb_obj_is_instance_of(rhs, CLASS_OF(lhs)))) {
        rb_raise(rb_eTypeError, "invalid type: expected %s but %s",
                 rb_class2name(CLASS_OF(lhs)), rb_class2name(CLASS_OF(rhs)));
    }
    chkerr(OCINumberAssign(oci8_errhp, _NUMBER(rhs), _NUMBER(lhs)));
    return lhs;
}

/*
 *  @private
 */
static VALUE onum_coerce(VALUE self, VALUE other)
{
    signed long sl;
    OCINumber n;

    switch(rboci8_type(other)) {
    case T_FIXNUM:
        sl = NUM2LONG(other);
        chkerr(OCINumberFromInt(oci8_errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &n));
        return rb_assoc_new(oci8_make_ocinumber(&n, oci8_errhp), self);
    case T_BIGNUM:
        /* change via string. */
        other = rb_big2str(other, 10);
        set_oci_number_from_str(&n, other, Qnil, Qnil, oci8_errhp);
        return rb_assoc_new(oci8_make_ocinumber(&n, oci8_errhp), self);
    case T_FLOAT:
        return rb_assoc_new(other, onum_to_f(self));
    case RBOCI8_T_RATIONAL:
        return rb_assoc_new(other, onum_to_r(self));
    case RBOCI8_T_BIGDECIMAL:
        return rb_assoc_new(other, onum_to_d(self));
    }
    rb_raise(rb_eTypeError, "Can't coerce %s to %s",
             rb_class2name(CLASS_OF(other)), rb_class2name(cOCINumber));
}

/*
 *  Returns a negated value of <i>self</i>
 *
 *  @example
 *    -OraNumber(2)  # =>  #<OraNumber:-2>
 *
 *  @return [OraNumber]
 */
static VALUE onum_neg(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;

    chkerr(OCINumberNeg(errhp, _NUMBER(self), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *    self + other
 *
 *  Returns the sum of <i>self</i> and <i>other</i>.
 *  When <i>other</i>'s class is Integer, it returns an OraNumber value.
 *  Otherwise, it returns a value same with <i>other</i>'s class.
 *
 *  @example
 *    OraNumber(2) + 3   # => #<OraNumber:5>
 *    OraNumber(2) + 1.5 # => 3.5 (Float)
 *
 *  @param [Numeric] other
 *  @return [Numeric]
 */
static VALUE onum_add(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    switch (rboci8_type(rhs)) {
    case T_FIXNUM:
    case T_BIGNUM:
        if (set_oci_number_from_num(&n, rhs, 0, errhp)) {
            chkerr(OCINumberAdd(errhp, _NUMBER(lhs), &n, &r));
            return oci8_make_ocinumber(&r, errhp);
        }
        break;
    case RBOCI8_T_ORANUMBER:
        chkerr(OCINumberAdd(errhp, _NUMBER(lhs), _NUMBER(rhs), &r));
        return oci8_make_ocinumber(&r, errhp);
    case T_FLOAT:
        return rb_funcall(onum_to_f(lhs), oci8_id_add_op, 1, rhs);
    case RBOCI8_T_RATIONAL:
        return rb_funcall(onum_to_r(lhs), oci8_id_add_op, 1, rhs);
    case RBOCI8_T_BIGDECIMAL:
        return rb_funcall(onum_to_d(lhs), oci8_id_add_op, 1, rhs);
    }
    return rb_num_coerce_bin(lhs, rhs, oci8_id_add_op);
}

/*
 *  call-seq:
 *    self - other
 *
 *  Returns the difference of <i>self</i> and <i>other</i>.
 *  When <i>other</i>'s class is Integer, it returns an OraNumber value.
 *  Otherwise, it returns a value same with <i>other</i>'s class.
 *
 *  @example
 *    OraNumber(2) - 3   # => #<OraNumber:-1>
 *    OraNumber(2) - 1.5 # => 0.5 (Float)
 *
 *  @param [Numeric] other
 *  @return [Numeric]
 */
static VALUE onum_sub(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    switch (rboci8_type(rhs)) {
    case T_FIXNUM:
    case T_BIGNUM:
        if (set_oci_number_from_num(&n, rhs, 0, errhp)) {
            chkerr(OCINumberSub(errhp, _NUMBER(lhs), &n, &r));
            return oci8_make_ocinumber(&r, errhp);
        }
        break;
    case RBOCI8_T_ORANUMBER:
        chkerr(OCINumberSub(errhp, _NUMBER(lhs), _NUMBER(rhs), &r));
        return oci8_make_ocinumber(&r, errhp);
    case T_FLOAT:
        return rb_funcall(onum_to_f(lhs), oci8_id_sub_op, 1, rhs);
    case RBOCI8_T_RATIONAL:
        return rb_funcall(onum_to_r(lhs), oci8_id_sub_op, 1, rhs);
    case RBOCI8_T_BIGDECIMAL:
        return rb_funcall(onum_to_d(lhs), oci8_id_sub_op, 1, rhs);
    }
    return rb_num_coerce_bin(lhs, rhs, oci8_id_sub_op);
}

/*
 *  call-seq:
 *    self * other
 *
 *  Returns the product of <i>self</i> and <i>other</i>.
 *  When <i>other</i>'s class is Integer, it returns an OraNumber value.
 *  Otherwise, it returns a value same with <i>other</i>'s class.
 *
 *  @example
 *    OraNumber(2) * 3   # => #<OraNumber:6>
 *    OraNumber(2) * 1.5 # => 3.0 (Float)
 *
 *  @param [Numeric] other
 *  @return [Numeric]
 */
static VALUE onum_mul(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    switch (rboci8_type(rhs)) {
    case T_FIXNUM:
    case T_BIGNUM:
        if (set_oci_number_from_num(&n, rhs, 0, errhp)) {
            chkerr(OCINumberMul(errhp, _NUMBER(lhs), &n, &r));
            return oci8_make_ocinumber(&r, errhp);
        }
        break;
    case RBOCI8_T_ORANUMBER:
        chkerr(OCINumberMul(errhp, _NUMBER(lhs), _NUMBER(rhs), &r));
        return oci8_make_ocinumber(&r, errhp);
    case T_FLOAT:
        return rb_funcall(onum_to_f(lhs), oci8_id_mul_op, 1, rhs);
    case RBOCI8_T_RATIONAL:
        return rb_funcall(onum_to_r(lhs), oci8_id_mul_op, 1, rhs);
    case RBOCI8_T_BIGDECIMAL:
        return rb_funcall(onum_to_d(lhs), oci8_id_mul_op, 1, rhs);
    }
    return rb_num_coerce_bin(lhs, rhs, oci8_id_mul_op);
}

/*
 *  call-seq:
 *    self / other
 *
 *  Returns the result of dividing <i>self</i> by <i>other</i>.
 *  When <i>other</i>'s class is Integer, it returns an OraNumber value.
 *  Otherwise, it returns a value same with <i>other</i>'s class.
 *
 *  @example
 *    OraNumber(2) / 3   # => #<OraNumber:0.6666666666666666666666666666666666666667>
 *    OraNumber(2) / 1.5 # => 1.3333333333333333 (Float)
 *
 *  @param [Numeric] other
 *  @return [Numeric]
 */
static VALUE onum_div(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    switch (rboci8_type(rhs)) {
    case T_FIXNUM:
        if (rhs == INT2FIX(0)) {
            rb_num_zerodiv();
        }
    case T_BIGNUM:
        if (set_oci_number_from_num(&n, rhs, 0, errhp)) {
            chkerr(OCINumberDiv(errhp, _NUMBER(lhs), &n, &r));
            return oci8_make_ocinumber(&r, errhp);
        }
        break;
    case RBOCI8_T_ORANUMBER:
        chkerr(OCINumberIsZero(errhp, _NUMBER(rhs), &is_zero));
        if (is_zero) {
            rb_num_zerodiv();
        }
        chkerr(OCINumberDiv(errhp, _NUMBER(lhs), _NUMBER(rhs), &r));
        return oci8_make_ocinumber(&r, errhp);
    case T_FLOAT:
        return rb_funcall(onum_to_f(lhs), oci8_id_div_op, 1, rhs);
    case RBOCI8_T_RATIONAL:
        return rb_funcall(onum_to_r(lhs), oci8_id_div_op, 1, rhs);
    case RBOCI8_T_BIGDECIMAL:
        return rb_funcall(onum_to_d(lhs), oci8_id_div_op, 1, rhs);
    }
    return rb_num_coerce_bin(lhs, rhs, oci8_id_div_op);
}

/*
 *  call-seq:
 *    self % other
 *
 *  Returns the modulo after division of <i>self</i> by <i>other</i>.
 *
 *  @example
 *    OraNumber(13) % 5 # => #<OraNumber:3>
 *
 *  @param [Numeric] other
 *  @return [OraNumber]
 *
 *  @raise [ZeroDivisionError] when <i>other</i> is zero.
 */
static VALUE onum_mod(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;
    boolean is_zero;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0, errhp))
        return rb_num_coerce_bin(lhs, rhs, '%');
    /* check whether argument is not zero. */
    chkerr(OCINumberIsZero(errhp, &n, &is_zero));
    if (is_zero)
        rb_num_zerodiv();
    /* modulo */
    chkerr(OCINumberMod(errhp, _NUMBER(lhs), &n, &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *    self ** other
 *
 *  Raises <i>self</i> to the power of <i>other</i>.
 *
 *  @example
 *    OraNumber(2) ** 2   # => #<OraNumber:4>
 *    OraNumber(2) ** 2.5 # => #<OraNumber:5.65685424949238019520675489683879231435>
 *
 *  @param [Numeric] other
 *  @return [OraNumber]
 */
static VALUE onum_power(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    OCINumber r;

    if (FIXNUM_P(rhs)) {
        chkerr(OCINumberIntPower(errhp, _NUMBER(lhs), FIX2INT(rhs), &r));
    } else {
        /* change to OCINumber */
        if (!set_oci_number_from_num(&n, rhs, 0, errhp))
            return rb_num_coerce_bin(lhs, rhs, id_power);
        chkerr(OCINumberPower(errhp, _NUMBER(lhs), &n, &r));
    }
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *    self <=> other
 *
 *  Returns -1, 0, or +1 depending on whether <i>self</i> is less than,
 *  equal to, or greater than <i>other</i>. This is the basis for the
 *  tests in <code>Comparable</code>.
 *
 *  @example
 *    OraNumber(5) <=> 3   # => 1
 *    OraNumber(4) <=> 4   # => 0
 *    OraNumber(2) <=> 2.5 # => 1
 *
 *  @param [Numeric] other
 *  @return [-1, 0 or +1]
 */
static VALUE onum_cmp(VALUE lhs, VALUE rhs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber n;
    sword r;

    /* change to OCINumber */
    if (!set_oci_number_from_num(&n, rhs, 0, errhp))
        return rb_num_coerce_cmp(lhs, rhs, id_cmp);
    /* compare */
    chkerr(OCINumberCmp(errhp, _NUMBER(lhs), &n, &r));
    if (r > 0) {
        return INT2FIX(1);
    } else if (r == 0) {
        return INT2FIX(0);
    } else {
        return INT2FIX(-1);
    }
}

/*
 *  Returns the largest <code>Integer</code> less than or equal to <i>self</i>.
 *
 *  @example
 *    OraNumber(11.1).floor  # => 11
 *    OraNumber(25.8).floor  # => 25
 *    OraNumber(-25.8).floor # => -26
 *
 *  @return [Integer]
 */
static VALUE onum_floor(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;

    chkerr(OCINumberFloor(errhp, _NUMBER(self), &r));
    return oci8_make_integer(&r, errhp);
}

/*
 *  Returns the smallest <code>Integer</code> greater than or equal to <i>self</i>.
 *
 *  @example
 *    OraNumber(11.1).ceil  # => 12
 *    OraNumber(25.8).ceil  # => 26
 *    OraNumber(-25.8).ceil # => -25
 *
 *  @return [Integer]
 */
static VALUE onum_ceil(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;

    chkerr(OCINumberCeil(errhp, _NUMBER(self), &r));
    return oci8_make_integer(&r, errhp);
}

/*
 *  @overload round
 *
 *    Rounds <i>self</i> to the nearest <code>Integer</code>.
 *
 *    @example
 *      OraNumber(1.49).round     # => 1
 *      OraNumber(1.5).round      # => 2
 *      OraNumber(-1.49).round    # => -1
 *      OraNumber(-1.5).round     # => -2
 *
 *    @return [Integer]
 *
 *  @overload round(decplace)
 *
 *    Rounds <i>onum</i> to a specified decimal place <i>decplace</i>.
 *
 *    @example
 *      OraNumber(123.456).round(2)   # => #<OraNumber:123.46>
 *      OraNumber(123.456).round(1)   # => #<OraNumber:123.5>
 *      OraNumber(123.456).round(0)   # => #<OraNumber:123>
 *      OraNumber(123.456).round(-1)  # => #<OraNumber:120>
 *
 *    @param [Integer]
 *    @return [OraNumber]
 */
static VALUE onum_round(int argc, VALUE *argv, VALUE self)
{
    OCIError *errhp = oci8_errhp;
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    chkerr(OCINumberRound(errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    if (argc == 0) {
        return oci8_make_integer(&r, errhp);
    } else {
        return oci8_make_ocinumber(&r, errhp);
    }
}

/*
 *  call-seq:
 *    truncate(decplace = 0)
 *
 *  Truncates <i>self</i> to a specified decimal place <i>decplace</i>.
 *
 *  @example
 *    OraNumber(123.456).truncate      # => #<OraNumber:123>
 *    OraNumber(123.456).truncate(1)   # => #<OraNumber:123.4>
 *    OraNumber(123.456).truncate(2)   # => #<OraNumber:123.45>
 *    OraNumber(123.456).truncate(-1)  # => #<OraNumber:120>
 *    
 *    OraNumber(-123.456).truncate     # => #<OraNumber:-123>
 *    OraNumber(-123.456).truncate(1)  # => #<OraNumber:-123.4>
 *    OraNumber(-123.456).truncate(2)  # => #<OraNumber:-123.45>
 *    OraNumber(-123.456).truncate(-1) # => #<OraNumber:-120>
 *
 *  @param [Integer]
 *  @return [OraNumber]
 *  @todo returns <i>Integer</i> when <i>decplace</i> is not specified.
 */
static VALUE onum_trunc(int argc, VALUE *argv, VALUE self)
{
    OCIError *errhp = oci8_errhp;
    VALUE decplace;
    OCINumber r;

    rb_scan_args(argc, argv, "01", &decplace /* 0 */);
    chkerr(OCINumberTrunc(errhp, _NUMBER(self), NIL_P(decplace) ? 0 : NUM2INT(decplace), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     round_prec(digits)
 *
 *  Rounds <i>self</i> to a specified number of decimal digits.
 *  This method is available on Oracle 8.1 client or upper.
 *
 *  @example
 *    OraNumber(1.234).round_prec(2) # => #<OraNumber:1.2>
 *    OraNumber(12.34).round_prec(2) # => #<OraNumber:12>
 *    OraNumber(123.4).round_prec(2) # => #<OraNumber:120>
 *
 *  @return [OraNumber]
 */
static VALUE onum_round_prec(VALUE self, VALUE ndigs)
{
    OCIError *errhp = oci8_errhp;
    OCINumber r;

    chkerr(OCINumberPrec(errhp, _NUMBER(self), NUM2INT(ndigs), &r));
    return oci8_make_ocinumber(&r, errhp);
}

/*
 *  call-seq:
 *     onum.to_char(fmt = nil, nlsparam = nil)
 *
 *  Returns a string containing a representation of self.
 *  <i>fmt</i> and <i>nlsparam</i> are used as 
 *  {http://docs.oracle.com/cd/E11882_01/server.112/e17118/functions201.htm Oracle SQL function TO_CHAR(number)}
 *  does.
 *
 *  @example
 *    OraNumber(123456.789).to_char('FM999,999,999.999')  # => "123,456.789"
 *    OraNumber(123456.789).to_char('FM999G999G999D999',  "NLS_NUMERIC_CHARACTERS = ',.'") # => "123.456,789"
 *
 *  @param [String] fmt
 *  @param [String] nlsparam
 *  @return [String]
 */
static VALUE onum_to_char(int argc, VALUE *argv, VALUE self)
{
    OCIError *errhp = oci8_errhp;
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
        rv = oranumber_to_str(_NUMBER(self), buf, sizeof(buf));
        if (rv > 0) {
            return rb_usascii_str_new(buf, rv);
        }
        oranumber_dump(_NUMBER(self), buf);
        rb_raise(eOCIException, "Invalid internal number format: %s", buf);
    }
    StringValue(fmt);
    fmt_ptr = RSTRING_ORATEXT(fmt);
    fmt_len = RSTRING_LEN(fmt);
    if (NIL_P(nls_params)) {
        nls_params_ptr = NULL;
        nls_params_len = 0;
    } else {
        StringValue(nls_params);
        nls_params_ptr = RSTRING_ORATEXT(nls_params);
        nls_params_len = RSTRING_LEN(nls_params);
    }
    rv = OCINumberToText(errhp, _NUMBER(self),
                         fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                         &buf_size, TO_ORATEXT(buf));
    if (rv == OCI_ERROR) {
        sb4 errcode;
        OCIErrorGet(errhp, 1, NULL, &errcode, NULL, 0, OCI_HTYPE_ERROR);
        if (errcode == 22065) {
            /* OCI-22065: number to text translation for the given format causes overflow */
            if (NIL_P(fmt)) /* implicit conversion */
                return rb_usascii_str_new_cstr("overflow");
        }
        chkerr(rv);
    }
    return rb_usascii_str_new(buf, buf_size);
}

/*
 *  Returns a string containing a representation of self.
 *
 *  @return [String]
 *
 *  @see #to_char
 */
static VALUE onum_to_s(VALUE self)
{
    return onum_to_char(0, NULL, self);
}

/*
 *  Returns <i>self</i> truncated to an <code>Integer</code>.
 *
 *  @return [Integer]
 */
static VALUE onum_to_i(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    OCINumber num;

    chkerr(OCINumberTrunc(errhp, _NUMBER(self), 0, &num));
    return oci8_make_integer(&num, errhp);
}

/*
 *  Converts <i>self</i> to <code>Float</code>.
 *
 *  When {OCI8.properties OCI8.properties [:float_conversion_type\]}
 *  is <code>:ruby</code>, <i>self</I> is converted by <code>self.to_s.to_f</code>.
 *  When it is <code>:oracle</code>, <i>self</I> is converted by the Oracle
 *  OCI function OCINumberToReal().
 *
 *  @return [Float]
 */
static VALUE onum_to_f(VALUE self)
{
    return rb_float_new(oci8_onum_to_dbl(_NUMBER(self), oci8_errhp));
}

/*
 *  Returns <i>self</i> as a <code>Rational</code>.
 *
 *  @return [Rational]
 */
static VALUE onum_to_r(VALUE self)
{
    VALUE x, y;
    int nshift = 0;
    OCINumber onum[2];
    int current = 0;
    boolean is_int;

    chkerr(OCINumberAssign(oci8_errhp, _NUMBER(self), &onum[0]));

    for (;;) {
        chkerr(OCINumberIsInt(oci8_errhp, &onum[current], &is_int));
        if (is_int) {
            break;
        }
        nshift++;
        chkerr(OCINumberShift(oci8_errhp, &onum[current], 1, &onum[1 - current]));
        current = 1 - current;
    }
    x = oci8_make_integer(&onum[current], oci8_errhp);
    if (nshift == 0) {
        y = INT2FIX(1);
    } else {
        y = rb_funcall(INT2FIX(10), rb_intern("**"), 1, INT2FIX(nshift));
    }
#ifdef rb_Rational2
    return rb_Rational(x, y);
#else
    if (!cRational) {
        rb_require("rational");
        cRational = rb_const_get(rb_cObject, id_Rational);
    }
    return rb_funcall(rb_cObject, id_Rational, 2, x, y);
#endif
}

/*
 *  Returns <i>self</i> as a <code>BigDecimal</code>.
 *
 *  @return [BigDecimal]
 */
static VALUE onum_to_d(VALUE self)
{
    return onum_to_d_real(_NUMBER(self), oci8_errhp);
}

/* Converts to BigDecimal via number in scientific notation */
static VALUE onum_to_d_real(OCINumber *num, OCIError *errhp)
{
    char buf[64];
    ub4 buf_size = sizeof(buf);
    const char *fmt = "FM9.99999999999999999999999999999999999999EEEE";

    if (!cBigDecimal) {
        rb_require("bigdecimal");
        cBigDecimal = rb_const_get(rb_cObject, id_BigDecimal);
    }
    chkerr(OCINumberToText(errhp, num, (const oratext *)fmt, strlen(fmt),
                           NULL, 0, &buf_size, TO_ORATEXT(buf)));
    return rb_funcall(rb_cObject, id_BigDecimal, 1, rb_usascii_str_new(buf, buf_size));
}

/*
 *  Returns <code>true</code> if <i>self</i> has a decimal part.
 *
 *  @example
 *    OraNumber(10).has_decimal_part?   # => false
 *    OraNumber(10.1).has_decimal_part? # => true
 *
 *  @return [true or false]
 *  @since 2.0.5
 */
static VALUE onum_has_decimal_part_p(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    boolean result;

    chkerr(OCINumberIsInt(errhp, _NUMBER(self), &result));
    return result ? Qfalse : Qtrue;
}

/*
 *  Returns self.
 *
 *  @return [OraNumber]
 */
static VALUE onum_to_onum(VALUE self)
{
    return self;
}

/*
 *  Returns <code>true</code> if <i>self</i> is zero.
 *
 *  @return [true or false]
 */
static VALUE onum_zero_p(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    boolean result;

    chkerr(OCINumberIsZero(errhp, _NUMBER(self), &result));
    return result ? Qtrue : Qfalse;
}

/*
 *  Returns the absolute value of <i>self</i>.
 *
 *  @return [OraNumber]
 */
static VALUE onum_abs(VALUE self)
{
    OCIError *errhp = oci8_errhp;
    OCINumber result;

    chkerr(OCINumberAbs(errhp, _NUMBER(self), &result));
    return oci8_make_ocinumber(&result, errhp);
}

/*
 *  call-seq:
 *     shift(ndigits)
 *
 *  Returns <i>self</i> shifted by <i>ndigits</i>
 *  This method is available on Oracle 8.1 client or upper.
 *
 *  @example
 *     OraNumber(123).shift(3)  # => #<OraNumber:123000>
 *     OraNumber(123).shift(-3) # => #<OraNumber:0.123>
 *
 *  @return [OraNumber]
 */
static VALUE onum_shift(VALUE self, VALUE exp)
{
    OCIError *errhp = oci8_errhp;
    OCINumber result;

    chkerr(OCINumberShift(errhp, _NUMBER(self), NUM2INT(exp), &result));
    return oci8_make_ocinumber(&result, errhp);
}

/*
 *  Returns internal representation whose format is same with the return value of
 *  {http://docs.oracle.com/cd/E11882_01/server.112/e17118/functions055.htm Oracle SQL function DUMP}.
 *
 *  @example
 *    OraNumber.new(100).dump  #=> "Typ=2 Len=2: 194,2"
 *    OraNumber.new(123).dump  #=> "Typ=2 Len=3: 194,2,24"
 *    OraNumber.new(0.1).dump  #=> "Typ=2 Len=2: 192,11"
 *
 *  @return [String]
 *  @since 2.0.4
 */
static VALUE onum_dump(VALUE self)
{
    char buf[ORANUMBER_DUMP_BUF_SIZ];
    int rv = oranumber_dump(_NUMBER(self), buf);
    return rb_usascii_str_new(buf, rv);
}

/*
 *  @private
 */
static VALUE onum_hash(VALUE self)
{
    char *c  = RTYPEDDATA_DATA(self);
    int size = c[0] + 1;
    long i, hash;

    /* assert(size <= 22); ?*/
    if (size > 22)
        size = 22;

    for (hash = 0, i = 1; i< size; i++) {
        hash += c[i] * 971;
    }
    if (hash < 0) hash = -hash;
    return INT2FIX(hash);
}

/*
 *  @private
 */
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
 *    _dump
 *
 *  Serializes <i>self</i>.
 *  This method is called by Marshal.dump().
 *
 *  @return [String] a byte stream
 *  @see OraNumber._load
 */
static VALUE onum__dump(int argc, VALUE *argv, VALUE self)
{
    char *c  = RTYPEDDATA_DATA(self);
    int size = c[0] + 1;
    VALUE dummy;

    rb_scan_args(argc, argv, "01", &dummy);
    return rb_str_new(c, size);
}

/*
 *  call-seq:
 *    _load(bytes)
 *
 *  Restores a byte stream serialized by {OraNumber#_dump}.
 *  This method is called by Marshal.load() to deserialize a byte stream
 *  created by Marshal.dump().
 *
 *  @param [String] bytes a byte stream
 *  @return [OraNumber] an deserialized object
 */
static VALUE
onum_s_load(VALUE klass, VALUE str)
{
    unsigned char *c;
    size_t size;
    OCINumber num;

    Check_Type(str, T_STRING);
    c = RSTRING_ORATEXT(str);
    size = RSTRING_LEN(str);
    if (size == 0 || size != c[0] + 1u || size > sizeof(num)) {
        rb_raise(rb_eTypeError, "marshaled OCI::Number format differ");
    }
    memset(&num, 0, sizeof(num));
    memcpy(&num, c, size);
    return oci8_make_ocinumber(&num, oci8_errhp);
}

/*
 *  Document-class: OCI8::BindType::OraNumber
 */

/*
 *  Document-class: OCI8::BindType::Integer
 */

/*
 *  Document-class: OCI8::BindType::Float
 */

static VALUE bind_ocinumber_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return oci8_make_ocinumber((OCINumber*)data, oci8_errhp);
}

static VALUE bind_integer_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return oci8_make_integer((OCINumber*)data, oci8_errhp);
}

static VALUE bind_float_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return oci8_make_float((OCINumber*)data, oci8_errhp);
}

static void bind_ocinumber_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    set_oci_number_from_num((OCINumber*)data, val, 1, oci8_errhp);
}

static void bind_integer_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    OCIError *errhp = oci8_errhp;
    OCINumber num;

    set_oci_number_from_num(&num, val, 1, errhp);
    chker2(OCINumberTrunc(errhp, &num, 0, (OCINumber*)data),
           &obind->base);
}

static void bind_ocinumber_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(OCINumber);
    obind->alloc_sz = sizeof(OCINumber);
}

static void bind_ocinumber_init_elem(oci8_bind_t *obind, VALUE svc)
{
    OCIError *errhp = oci8_errhp;
    ub4 idx = 0;

    do {
        OCINumberSetZero(errhp, (OCINumber*)obind->valuep + idx);
    } while (++idx < obind->maxar_sz);
}

static const oci8_bind_data_type_t bind_ocinumber_data_type = {
    {
        {
            "OCI8::BindType::OraNumber",
            {
                NULL,
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
    bind_ocinumber_get,
    bind_ocinumber_set,
    bind_ocinumber_init,
    bind_ocinumber_init_elem,
    NULL,
    SQLT_VNU,
};

static VALUE bind_ocinumber_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_ocinumber_data_type.base);
}

static const oci8_bind_data_type_t bind_integer_data_type = {
    {
        {
            "OCI8::BindType::Integer",
            {
                NULL,
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
    bind_integer_get,
    bind_integer_set,
    bind_ocinumber_init,
    bind_ocinumber_init_elem,
    NULL,
    SQLT_VNU,
};

static VALUE bind_integer_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_integer_data_type.base);
}

static const oci8_bind_data_type_t bind_float_data_type = {
    {
        {
            "OCI8::BindType::Float",
            {
                NULL,
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
    bind_float_get,
    bind_ocinumber_set,
    bind_ocinumber_init,
    bind_ocinumber_init_elem,
    NULL,
    SQLT_VNU,
};

static VALUE bind_float_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_float_data_type.base);
}

void
Init_oci_number(VALUE cOCI8, OCIError *errhp)
{
    VALUE mMath;
    OCINumber num1, num2;
    VALUE obj_PI;
    signed long sl;

    id_power = rb_intern("**");
    id_cmp = rb_intern("<=>");
    id_finite_p = rb_intern("finite?");
    id_split = rb_intern("split");
    id_numerator = rb_intern("numerator");
    id_denominator = rb_intern("denominator");
    id_Rational = rb_intern("Rational");
    id_BigDecimal = rb_intern("BigDecimal");

    cOCINumber = rb_define_class("OraNumber", rb_cNumeric);
    mMath = rb_define_module_under(cOCI8, "Math");

    /* constants for internal use. */
    /* set const_p1 */
    sl = 1;
    OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_p1);
    /* set const_p10 */
    sl = 10;
    OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_p10);
    /* set const_m1 */
    sl = -1;
    OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &const_m1);
    /* set const_PI2 */
    sl = 2;
    OCINumberSetPi(errhp, &num1);
    OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, &num2);
    OCINumberDiv(errhp, &num1 /* PI */, &num2 /* 2 */, &const_PI2);
    /* set const_mPI2 */
    OCINumberNeg(errhp, &const_PI2 /* PI/2 */, &const_mPI2);

    /* PI */
    OCINumberSetPi(errhp, &num1);
    obj_PI = oci8_make_ocinumber(&num1, errhp);

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
    rb_define_global_function("OraNumber", onum_f_new, -1);
    rb_define_method(cOCINumber, "initialize", onum_initialize, -1);
    rb_define_method(cOCINumber, "initialize_copy", onum_initialize_copy, 1);
    rb_define_method(cOCINumber, "coerce", onum_coerce, 1);

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
    rb_define_method(cOCINumber, "round_prec", onum_round_prec, 1);

    rb_define_method(cOCINumber, "to_s", onum_to_s, 0);
    rb_define_method(cOCINumber, "to_char", onum_to_char, -1);
    rb_define_method(cOCINumber, "to_i", onum_to_i, 0);
    rb_define_method(cOCINumber, "to_f", onum_to_f, 0);
    rb_define_method(cOCINumber, "to_r", onum_to_r, 0);
    rb_define_method(cOCINumber, "to_d", onum_to_d, 0);
    rb_define_method(cOCINumber, "has_decimal_part?", onum_has_decimal_part_p, 0);
    rb_define_method(cOCINumber, "to_onum", onum_to_onum, 0);

    rb_define_method(cOCINumber, "zero?", onum_zero_p, 0);
    rb_define_method(cOCINumber, "abs", onum_abs, 0);
    rb_define_method(cOCINumber, "shift", onum_shift, 1);
    rb_define_method(cOCINumber, "dump", onum_dump, 0);

    rb_define_method(cOCINumber, "hash", onum_hash, 0);
    rb_define_method(cOCINumber, "inspect", onum_inspect, 0);

    /* methods for marshaling */
    rb_define_method(cOCINumber, "_dump", onum__dump, -1);
    rb_define_singleton_method(cOCINumber, "_load", onum_s_load, 1);

    oci8_define_bind_class("OraNumber", &bind_ocinumber_data_type, bind_ocinumber_alloc);
    oci8_define_bind_class("Integer", &bind_integer_data_type, bind_integer_alloc);
    oci8_define_bind_class("Float", &bind_float_data_type, bind_float_alloc);

#if 0 /* for rdoc/yard */
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", oci8_cOCIHandle);
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", oci8_cOCIHandle);

    dummy1 = rb_define_class_under(mOCI8BindType, "OraNumber", cOCI8BindTypeBase);
    dummy2 = rb_define_class_under(mOCI8BindType, "Integer", cOCI8BindTypeBase);
    dummy3 = rb_define_class_under(mOCI8BindType, "Float", cOCI8BindTypeBase);
#endif
}

/*
 * OraNumber (ruby object) -> OCINumber (C datatype)
 */
OCINumber *oci8_get_ocinumber(VALUE num)
{
    if (!rb_obj_is_kind_of(num, cOCINumber)) {
        rb_raise(rb_eTypeError, "invalid argument %s (expect a subclass of %s)", rb_class2name(CLASS_OF(num)), rb_class2name(cOCINumber));
    }
    return _NUMBER(num);
}
