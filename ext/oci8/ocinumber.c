/*
  ocinumber.c - part of ruby-oci8
  copy from ocinumber.c in ruby-oci8 0.2.
*/

#include "oci8.h"
#include <orl.h>

/* use for local call */
#define oci_lc(rv) do { \
    sword __rv = (rv); \
    if (__rv != OCI_SUCCESS) { \
        oci8_raise(errhp, __rv, NULL); \
    } \
} while(0)

#define NUMBER_FORMAT1 "FM9999999999999999999999990.9999999999999999999999999999999999999"
#define NUMBER_FORMAT1_LEN (sizeof(NUMBER_FORMAT1) - 1)
#define NUMBER_FORMAT2 "FM99999999999999999999999999999999999990.999999999999999999999999"
#define NUMBER_FORMAT2_LEN (sizeof(NUMBER_FORMAT2) - 1)
#define NUMBER_FORMAT2_DECIMAL                          (sizeof("999999999999999999999999") - 1)
#define NUMBER_FORMAT_INT "FM99999999999999999999999999999999999990"
#define NUMBER_FORMAT_INT_LEN (sizeof(NUMBER_FORMAT_INT) - 1)

/* fill C structure (OCINumber) from a string. */
static void set_oci_number_from_str(OCINumber *result, VALUE str, VALUE fmt, VALUE nls_params, OCIError *errhp)
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
    oci_lc(OCINumberFromText(errhp,
                             RSTRING(str)->ptr, RSTRING(str)->len,
                             fmt_ptr, fmt_len, nls_params_ptr, nls_params_len,
                             result));
}

/* fill C structure (OCINumber) from a numeric object. */
/* 1 - success, 0 - error */
static int set_oci_number_from_num(OCINumber *result, VALUE num, OCIError *errhp)
{
    signed long sl;
    double dbl;

    if (!RTEST(rb_obj_is_kind_of(num, rb_cNumeric)))
        rb_raise(rb_eTypeError, "expect Numeric but %s", rb_class2name(CLASS_OF(num)));
    switch (rb_type(num)) {
    case T_FIXNUM:
        /* set from long. */
        sl = NUM2LONG(num);
        oci_lc(OCINumberFromInt(errhp, &sl, sizeof(sl), OCI_NUMBER_SIGNED, result));
        return 1;
    case T_FLOAT:
        /* set from double. */
        dbl = NUM2DBL(num);
        oci_lc(OCINumberFromReal(errhp, &dbl, sizeof(dbl), result));
        return 1;
    case T_BIGNUM:
        /* change via string. */
        num = rb_big2str(num, 10);
        set_oci_number_from_str(result, num, Qnil, Qnil, errhp);
        return 1;
    }
    if (RTEST(rb_obj_is_instance_of(num, cOraNumber))) {
        /* OCI::Number */
        oci_lc(OCINumberAssign(errhp, DATA_PTR(num), result));
        return 1;
    }
    return 0;
}

int set_oci_vnumber(ora_vnumber_t *result, VALUE num, OCIError *errhp)
{
    return set_oci_number_from_num((OCINumber*)result, num, errhp);
}
