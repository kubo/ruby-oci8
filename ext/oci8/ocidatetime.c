/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  ocidatetime.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2005-2006 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

static VALUE cDateTime;
static VALUE cOCI8IntervalYM;
static VALUE cOCI8IntervalDS;
static ID id_parse;
static ID id_civil;
static ID id_to_r;
static ID id_add;
static ID id_mul;
static ID id_div;
static ID id_divmod;
static ID id_less;
static ID id_uminus;
static ID id_year;
static ID id_mon;
static ID id_month;
static ID id_mday;
static ID id_day;
static ID id_hour;
static ID id_min;
static ID id_sec;
static ID id_sec_fraction;
static ID id_of;
static VALUE hour_base;
static VALUE minute_base;
static VALUE sec_base;
static VALUE fsec_base;

typedef struct {
    oci8_bind_t bind;
    ub4 type;
    void *hp;
} oci8_bind_dsc_t;

void oci8_bind_dsc_free(oci8_base_t *base)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t *)base;
    oci8_bind_free(base);
    if (bd->type != 0) {
        OCIDescriptorFree(bd->hp, bd->type);
        bd->type = 0;
    }
}

static VALUE make_datetime(sb2 year, ub1 month, ub1 day, ub1 hour, ub1 minute, ub1 sec, ub4 fsec, sb1 tz_hour, sb1 tz_minute)
{
    VALUE tz;
    VALUE tzm;

    if (fsec != 0) {
        /* make a DateTime object via String. */
        char buf[60];
        char sign = '+';

        if (tz_hour < 0) {
            sign = '-';
            tz_hour = - tz_hour;
            tz_minute = - tz_minute;
        }
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%09d%c%02d:%02d",
                 year, month, day, hour, minute, sec, fsec, sign, 
                 tz_hour, tz_minute);
        return rb_funcall(cDateTime, id_parse, 1, rb_str_new2(buf));
    }
    if (tz_hour == 0 && tz_minute == 0) {
        tz = INT2FIX(0);
    } else {
        /* tz = tz.to_r */
        tz = rb_funcall(INT2FIX(tz_hour), id_to_r, 0);
        /* tz = tz / 24 */
        tz = rb_funcall(tz, id_div, 1, INT2FIX(24));
        if (tz_minute != 0) {
            /* tzm = tzm.to_r */
            tzm = rb_funcall(INT2FIX(tz_minute), id_to_r, 0);
            /* tzm = tzm / 1440 */
            tzm = rb_funcall(tzm, id_div, 1, INT2FIX(1440));
            /* tz = tz + tzm */
            tz = rb_funcall(tz, id_add, 1, tzm);
        }
    }
    return rb_funcall(cDateTime, id_civil, 7,
                      INT2FIX(year), INT2FIX(month), INT2FIX(day),
                      INT2FIX(hour), INT2FIX(minute), INT2FIX(sec), tz);
}

VALUE oci8_make_datetime_from_ocidate(OCIDate *s)
{
    return make_datetime(s->OCIDateYYYY, s->OCIDateMM, s->OCIDateDD, s->OCIDateTime.OCITimeHH, s->OCIDateTime.OCITimeMI, s->OCIDateTime.OCITimeSS, 0, 0, 0);
}

VALUE oci8_make_datetime_from_ocidatetime(OCIDateTime *s)
{
    sb2 year;
    ub1 month;
    ub1 day;
    ub1 hour;
    ub1 minute;
    ub1 sec;
    ub4 fsec;
    sb1 tz_hour;
    sb1 tz_minute;
    sword rv;

    oci_lc(OCIDateTimeGetDate(oci8_envhp, oci8_errhp, s, &year, &month, &day));
    oci_lc(OCIDateTimeGetTime(oci8_envhp, oci8_errhp, s, &hour, &minute, &sec, &fsec));
    rv = OCIDateTimeGetTimeZoneOffset(oci8_envhp, oci8_errhp, s, &tz_hour, &tz_minute);
    if (rv != OCI_SUCCESS) {
        tz_hour = tz_minute = 0;
    }
    return make_datetime(year, month, day, hour, minute, sec, fsec, tz_hour, tz_minute);
}

VALUE oci8_make_interval_ym(OCIInterval *s)
{
    sb4 year;
    sb4 month;

    oci_lc(OCIIntervalGetYearMonth(oci8_envhp, oci8_errhp, &year, &month, s));
    return INT2NUM(year * 12 + month);
}

VALUE oci8_make_interval_ds(OCIInterval *s)
{
    sb4 day;
    sb4 hour;
    sb4 minute;
    sb4 sec;
    sb4 fsec;
    VALUE days;

    oci_lc(OCIIntervalGetDaySecond(oci8_envhp, oci8_errhp, &day, &hour, &minute, &sec, &fsec, s));
    days = INT2NUM(day);
    if (hour != 0) {
        days = rb_funcall(days, id_add, 1, rb_funcall(INT2FIX(hour), id_div, 1, hour_base));
    }
    if (minute != 0) {
        days = rb_funcall(days, id_add, 1, rb_funcall(INT2FIX(minute), id_div, 1, minute_base));
    }
    if (sec != 0) {
        days = rb_funcall(days, id_add, 1, rb_funcall(INT2FIX(sec), id_div, 1, sec_base));
    }
    if (fsec != 0) {
        days = rb_funcall(days, id_add, 1, rb_funcall(INT2FIX(fsec), id_div, 1, fsec_base));
    }
    return days;
}

static VALUE bind_datetime_get(oci8_bind_t *b)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    return oci8_make_datetime_from_ocidatetime(bd->hp);
}

static void bind_datetime_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    int ival;
    sb2 year;
    ub1 month;
    ub1 day;
    ub1 hour;
    ub1 minute;
    ub1 sec;
    ub4 fsec;
    char tz_str[32];

    /* year */
    ival = NUM2INT(rb_funcall(val, id_year, 0));
    if (ival < -4712 || 9999 < ival) {
        rb_raise(rb_eRuntimeError, "out of year range: %d", ival);
    }
    year = (sb2)ival;
    /* month */
    if (rb_respond_to(val, id_mon)) {
        ival = NUM2INT(rb_funcall(val, id_mon, 0));
    } else if (rb_respond_to(val, id_month)) {
        ival = NUM2INT(rb_funcall(val, id_month, 0));
    } else {
        rb_raise(rb_eRuntimeError, "expect Time, Date or DateTime but %s",
                 rb_class_name(CLASS_OF(val)));
    }
    if (ival < 0 || 12 < ival) {
        rb_raise(rb_eRuntimeError, "out of month range: %d", ival);
    }
    month = (ub1)ival;
    /* day */
    if (rb_respond_to(val, id_mday)) {
        ival = NUM2INT(rb_funcall(val, id_mday, 0));
    } else if (rb_respond_to(val, id_day)) {
        ival = NUM2INT(rb_funcall(val, id_day, 0));
    } else {
        rb_raise(rb_eRuntimeError, "expect Time, Date or DateTime but %s",
                 rb_class_name(CLASS_OF(val)));
    }
    if (ival < 0 || 31 < ival) {
        rb_raise(rb_eRuntimeError, "out of day range: %d", ival);
    }
    day = (ub1)ival;
    /* hour */
    if (rb_respond_to(val, id_hour)) {
        ival = NUM2INT(rb_funcall(val, id_hour, 0));
    } else {
        ival = 0;
    }
    if (ival < 0 || 24 < ival) {
        rb_raise(rb_eRuntimeError, "out of hour range: %d", ival);
    }
    hour = (ub1)ival;
    /* minute */
    if (rb_respond_to(val, id_min)) {
        ival = NUM2INT(rb_funcall(val, id_min, 0));
        if (ival < 0 || 60 < ival) {
            rb_raise(rb_eRuntimeError, "out of minute range: %d", ival);
        }
        minute = (ub1)ival;
    } else {
        minute = 0;
    }
    /* second */
    if (rb_respond_to(val, id_sec)) {
        ival = NUM2INT(rb_funcall(val, id_sec, 0));
        if (ival < 0 || 61 < ival) {
            rb_raise(rb_eRuntimeError, "out of second range: %d", ival);
        }
        sec = (ub1)ival;
    } else {
        sec = 0;
    }
    /* sec_fraction */
    if (rb_respond_to(val, id_sec_fraction)) {
        VALUE fs = rb_funcall(val, id_sec_fraction, 0);
        fsec = NUM2UINT(rb_funcall(fs, id_mul, 1, fsec_base));
    } else {
        fsec = 0;
    }
    /* time zone */
    if (rb_respond_to(val, id_of)) {
        VALUE of = rb_funcall(val, id_of, 0);
        char sign = '+';
        ival = NUM2INT(rb_funcall(of, id_mul, 1, INT2FIX(1440)));
        if (ival < 0) {
            sign = '-';
            ival = - ival;
        }
        sprintf(tz_str, "%c%02d:%02d", sign, ival / 60, ival % 60);
    } else {
        strcpy(tz_str, "+00:00");
    }
    oci_lc(OCIDateTimeConstruct(oci8_envhp, oci8_errhp, bd->hp,
                                year,
                                month,
                                day,
                                hour,
                                minute,
                                sec,
                                fsec,
                                (OraText *)tz_str,
                                strlen(tz_str)));
}

static void bind_datetime_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    sword rv;

    b->valuep = &bd->hp;
    b->value_sz = sizeof(bd->hp);
	rv = OCIDescriptorAlloc(oci8_envhp, (void*)&bd->hp, OCI_DTYPE_TIMESTAMP_TZ, 0, 0);
	if (rv != OCI_SUCCESS)
	    oci8_env_raise(oci8_envhp, rv);
    bd->type = OCI_DTYPE_TIMESTAMP_TZ;
}

static VALUE bind_interval_ym_get(oci8_bind_t *b)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;

    return oci8_make_interval_ym(bd->hp);
}

static void bind_interval_ym_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    int months = NUM2INT(val);
    int sign = 1;

    if (months < 0) {
        sign = -1;
        months = - months;
    }
    oci_lc(OCIIntervalSetYearMonth(oci8_envhp, oci8_errhp,
                                   (months / 12) * sign,
                                   (months % 12) * sign,
                                   bd->hp));
}

static void bind_interval_ym_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t *)b;
    sword rv;

    b->valuep = &bd->hp;
    b->value_sz = sizeof(bd->hp);
    rv = OCIDescriptorAlloc(oci8_envhp, (void*)&bd->hp, OCI_DTYPE_INTERVAL_YM, 0, 0);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    bd->type = OCI_DTYPE_INTERVAL_YM;
}

static VALUE bind_interval_ds_get(oci8_bind_t *b)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;

    return oci8_make_interval_ds(bd->hp);
}

static void bind_interval_ds_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    sb4 day;
    sb4 hour;
    sb4 minute;
    sb4 sec;
    sb4 fsec;
    VALUE ary;
    int is_negative = 0;

    if (!rb_obj_is_kind_of(val, rb_cNumeric)) {
        rb_raise(rb_eTypeError, "expected numeric but %s", rb_class_name(CLASS_OF(val)));
    }
    /* sign */
    if (RTEST(rb_funcall(val, id_less, 1, INT2FIX(0)))) {
        is_negative = 1;
        val = rb_funcall(val, id_uminus, 0);
    }
    /* day */
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY(ary)->len != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    day = NUM2INT(RARRAY(ary)->ptr[0]);
    /* hour */
    val = rb_funcall(RARRAY(ary)->ptr[1], id_mul, 1, INT2FIX(24));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY(ary)->len != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    hour = NUM2INT(RARRAY(ary)->ptr[0]);
    /* minute */
    val = rb_funcall(RARRAY(ary)->ptr[1], id_mul, 1, INT2FIX(60));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY(ary)->len != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    minute = NUM2INT(RARRAY(ary)->ptr[0]);
    /* second */
    val = rb_funcall(RARRAY(ary)->ptr[1], id_mul, 1, INT2FIX(60));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY(ary)->len != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    sec = NUM2INT(RARRAY(ary)->ptr[0]);
    /* fsec */
    val = rb_funcall(RARRAY(ary)->ptr[1], id_mul, 1, INT2FIX(1000000000));
    fsec = NUM2INT(val);
    if (is_negative) {
        day = - day;
        hour = - hour;
        minute = - minute;
        sec = - sec;
        fsec = - fsec;
    }
    oci_lc(OCIIntervalSetDaySecond(oci8_envhp, oci8_errhp,
                                   day, hour, minute, sec, fsec, bd->hp));
}

static void bind_interval_ds_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t *)b;
    sword rv;

    b->valuep = &bd->hp;
    b->value_sz = sizeof(bd->hp);
	rv = OCIDescriptorAlloc(oci8_envhp, (void*)&bd->hp, OCI_DTYPE_INTERVAL_DS, 0, 0);
	if (rv != OCI_SUCCESS)
	    oci8_env_raise(oci8_envhp, rv);
    bd->type = OCI_DTYPE_INTERVAL_DS;
}

static oci8_bind_class_t bind_datetime_class = {
    {
        NULL,
        oci8_bind_dsc_free,
        sizeof(oci8_bind_dsc_t)
    },
    bind_datetime_get,
    bind_datetime_set,
    bind_datetime_init,
    NULL,
    NULL,
    SQLT_TIMESTAMP_TZ
};

static oci8_bind_class_t bind_interval_ym_class = {
    {
        NULL,
        oci8_bind_dsc_free,
        sizeof(oci8_bind_dsc_t)
    },
    bind_interval_ym_get,
    bind_interval_ym_set,
    bind_interval_ym_init,
    NULL,
    NULL,
    SQLT_INTERVAL_YM
};

static oci8_bind_class_t bind_interval_ds_class = {
    {
        NULL,
        oci8_bind_dsc_free,
        sizeof(oci8_bind_dsc_t)
    },
    bind_interval_ds_get,
    bind_interval_ds_set,
    bind_interval_ds_init,
    NULL,
    NULL,
    SQLT_INTERVAL_DS
};

void Init_oci_datetime(void)
{
    rb_require("date");
    rb_require("oci8/interval");

    cDateTime = rb_eval_string("DateTime");
    cOCI8IntervalYM = rb_eval_string("OCI8::IntervalYM");
    cOCI8IntervalDS = rb_eval_string("OCI8::IntervalDS");
    id_civil = rb_intern("civil");
    id_parse = rb_intern("parse");
    id_to_r = rb_intern("to_r");
    id_add = rb_intern("+");
    id_mul = rb_intern("*");
    id_div = rb_intern("/");
    id_divmod = rb_intern("divmod");
    id_less = rb_intern("<");
    id_uminus = rb_intern("-@");
    id_year = rb_intern("year");
    id_mon = rb_intern("mon");
    id_month = rb_intern("month");
    id_mday = rb_intern("mday");
    id_day = rb_intern("day");
    id_hour = rb_intern("hour");
    id_min = rb_intern("min");
    id_sec = rb_intern("sec");
    id_sec_fraction = rb_intern("sec_fraction");
    id_of = rb_intern("of");

    hour_base = rb_funcall(INT2FIX(24), id_to_r, 0);
    minute_base = rb_funcall(hour_base, id_mul, 1, INT2FIX(60));
    sec_base = rb_funcall(minute_base, id_mul, 1, INT2FIX(60));
    fsec_base = rb_funcall(sec_base, id_mul, 1, INT2FIX(1000000000));
    rb_global_variable(&hour_base);
    rb_global_variable(&minute_base);
    rb_global_variable(&sec_base);
    rb_global_variable(&fsec_base);

    oci8_define_bind_class("DateTime", &bind_datetime_class);
    oci8_define_bind_class("IntervalYM", &bind_interval_ym_class);
    oci8_define_bind_class("IntervalDS", &bind_interval_ds_class);
}
