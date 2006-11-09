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
static ID id_civil;
static ID id_to_r;
static ID id_add;
static ID id_div;

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
    VALUE s;
    VALUE fs;
    VALUE tz;
    VALUE tzm;

    s = INT2FIX(sec);
    if (fsec != 0) {
        /* fs = fs.to_r */
        fs = rb_funcall(INT2FIX(fsec), id_to_r, 0);
        /* fs = fs / 1000000000 */
        fs = rb_funcall(fs, id_div, 1, INT2FIX(1000000000));
        /* s = s + fs */
        s = rb_funcall(s, id_add, 1, fs);
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
                      INT2FIX(hour), INT2FIX(minute), s, tz);
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
    return rb_funcall(cOCI8IntervalYM, oci8_id_new, 2, INT2FIX(year), INT2FIX(month));
}

VALUE oci8_make_interval_ds(OCIInterval *s)
{
    sb4 day;
    sb4 hour;
    sb4 minute;
    sb4 sec;
    sb4 fsec;

    oci_lc(OCIIntervalGetDaySecond(oci8_envhp, oci8_errhp, &day, &hour, &minute, &sec, &fsec, s));
    return rb_funcall(cOCI8IntervalDS, oci8_id_new, 5, INT2FIX(day), INT2FIX(hour), INT2FIX(minute), INT2FIX(sec), INT2FIX(fsec));
}

static VALUE bind_datetime_get(oci8_bind_t *b)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    return oci8_make_datetime_from_ocidatetime(bd->hp);
}

static void bind_datetime_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
    char tz_str[32];

    Check_Type(val, T_ARRAY);
    if (RARRAY(val)->len != 9) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    Check_Type(RARRAY(val)->ptr[0], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[1], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[2], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[3], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[4], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[5], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[6], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[7], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[8], T_FIXNUM);
    sprintf(tz_str, "%+02d:%02d", FIX2INT(RARRAY(val)->ptr[7]), FIX2INT(RARRAY(val)->ptr[8]));

    oci_lc(OCIDateTimeConstruct(oci8_envhp, oci8_errhp, bd->hp,
                                FIX2INT(RARRAY(val)->ptr[0]),
                                FIX2INT(RARRAY(val)->ptr[1]),
                                FIX2INT(RARRAY(val)->ptr[2]),
                                FIX2INT(RARRAY(val)->ptr[3]),
                                FIX2INT(RARRAY(val)->ptr[4]),
                                FIX2INT(RARRAY(val)->ptr[5]),
                                FIX2INT(RARRAY(val)->ptr[6]),
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

    Check_Type(val, T_ARRAY);
    if (RARRAY(val)->len != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    Check_Type(RARRAY(val)->ptr[0], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[1], T_FIXNUM);
    oci_lc(OCIIntervalSetYearMonth(oci8_envhp, oci8_errhp,
                                   FIX2INT(RARRAY(val)->ptr[0]),
                                   FIX2INT(RARRAY(val)->ptr[1]),
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

    Check_Type(val, T_ARRAY);
    if (RARRAY(val)->len != 5) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    Check_Type(RARRAY(val)->ptr[0], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[1], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[2], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[3], T_FIXNUM);
    Check_Type(RARRAY(val)->ptr[4], T_FIXNUM);
    oci_lc(OCIIntervalSetDaySecond(oci8_envhp, oci8_errhp,
                                   FIX2INT(RARRAY(val)->ptr[0]),
                                   FIX2INT(RARRAY(val)->ptr[1]),
                                   FIX2INT(RARRAY(val)->ptr[2]),
                                   FIX2INT(RARRAY(val)->ptr[3]),
                                   FIX2INT(RARRAY(val)->ptr[4]),
                                   bd->hp));
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
    id_to_r = rb_intern("to_r");
    id_add = rb_intern("+");
    id_div = rb_intern("/");

    oci8_define_bind_class("DateTime", &bind_datetime_class);
    oci8_define_bind_class("IntervalYM", &bind_interval_ym_class);
    oci8_define_bind_class("IntervalDS", &bind_interval_ds_class);
}
