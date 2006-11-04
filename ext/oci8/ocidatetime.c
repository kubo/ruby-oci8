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

static VALUE bind_datetime_get(oci8_bind_t *b)
{
    oci8_bind_dsc_t *bd = (oci8_bind_dsc_t*)b;
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

    oci_lc(OCIDateTimeGetDate(oci8_envhp, oci8_errhp, bd->hp, &year, &month, &day));
    oci_lc(OCIDateTimeGetTime(oci8_envhp, oci8_errhp, bd->hp, &hour, &minute, &sec, &fsec));
    oci_lc(rv = OCIDateTimeGetTimeZoneOffset(oci8_envhp, oci8_errhp, bd->hp, &tz_hour, &tz_minute));
    return rb_ary_new3(9, INT2FIX(year), INT2FIX(month), INT2FIX(day), INT2FIX(hour), INT2FIX(minute), INT2FIX(sec), UB4_TO_NUM(fsec), INT2FIX(tz_hour), INT2FIX(tz_minute));
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
    sb4 year;
    sb4 month;

    oci_lc(OCIIntervalGetYearMonth(oci8_envhp, oci8_errhp, &year, &month, bd->hp));
    return rb_ary_new3(2, INT2FIX(year), INT2FIX(month));
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
    sb4 day;
    sb4 hour;
    sb4 minute;
    sb4 sec;
    sb4 fsec;

    oci_lc(OCIIntervalGetDaySecond(oci8_envhp, oci8_errhp, &day, &hour, &minute, &sec, &fsec, bd->hp));
    return rb_ary_new3(5, INT2FIX(day), INT2FIX(hour), INT2FIX(minute), INT2FIX(sec), INT2FIX(fsec));
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

    oci8_define_bind_class("DateTime", &bind_datetime_class);
    oci8_define_bind_class("IntervalYM", &bind_interval_ym_class);
    oci8_define_bind_class("IntervalDS", &bind_interval_ds_class);
}
