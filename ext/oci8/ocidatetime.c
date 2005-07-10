/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  ocidatetime.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2005 KUBO Takehiro <kubo@jiubao.org>
 *
 */
#include "oci8.h"

static VALUE cOCIDateTime;
static VALUE cOCIInterval;

static dvoid *odt_get_hndl(VALUE ses)
{
    if (NIL_P(ses)) {
        return oci8_envhp;
    } else {
        /* TODO: get session handle. */
        rb_raise(rb_eNotImpError, "must be nil (fix in the future)");
    }
}

static OCIDateTime *odt_get_datetime(VALUE obj)
{
    oci8_base_t *base;
    if (!rb_obj_is_kind_of(obj, cOCIDateTime)) { 
        rb_raise(rb_eTypeError, "invalid argument %s (expect a subclass of %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(cOCIDateTime));
    }
    base = DATA_PTR(obj);
    return base->hp;
}

static OCIInterval *odt_get_interval(VALUE obj)
{
    oci8_base_t *base;
    if (!rb_obj_is_kind_of(obj, cOCIInterval)) { 
        rb_raise(rb_eTypeError, "invalid argument %s (expect a subclass of %s)", rb_class2name(CLASS_OF(obj)), rb_class2name(cOCIInterval));
    }
    base = DATA_PTR(obj);
    return base->hp;
}

#define TO_HNDL odt_get_hndl
#define TO_DATETIME odt_get_datetime
#define TO_INTERVAL odt_get_interval

static VALUE odt_initialize(VALUE self, VALUE vtype)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 type;
    sword rv;

    Check_Type(vtype, T_FIXNUM);
    type = FIX2INT(vtype);

	rv = OCIDescriptorAlloc(oci8_envhp, (void*)&base->hp, type, 0, 0);
	if (rv != OCI_SUCCESS)
	    oci8_env_raise(oci8_envhp, rv);
    base->type = type;
    return Qnil;
}

static VALUE odt_get_time(VALUE self, VALUE ses)
{
    ub1 hr;
    ub1 mm;
    ub1 ss;
    ub4 fsec;

    oci_lc(OCIDateTimeGetTime(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                              &hr, &mm, &ss, &fsec));
    return rb_ary_new3(4, INT2FIX(hr), INT2FIX(mm), INT2FIX(ss), INT2FIX(fsec));
}

static VALUE odt_get_date(VALUE self, VALUE ses)
{
    sb2 yr;
    ub1 mnth;
    ub1 dy;

    oci_lc(OCIDateTimeGetDate(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                              &yr, &mnth, &dy));
    return rb_ary_new3(3, INT2FIX(yr), INT2FIX(mnth), INT2FIX(dy));
}

static VALUE odt_get_timezone_offset(VALUE self, VALUE ses)
{
    sb1 hr;
    sb1 mm;

    oci_lc(OCIDateTimeGetTimeZoneOffset(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                                        &hr, &mm));
    return rb_ary_new3(2, INT2FIX(hr), INT2FIX(mm));
}

static VALUE odt_construct(VALUE self, VALUE ses, VALUE yr, VALUE mnth, VALUE dy, VALUE hr, VALUE mm, VALUE ss, VALUE fsec, VALUE timezone)
{
    CHECK_STRING(timezone);
    oci_lc(OCIDateTimeConstruct(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                                NUM2INT(yr), NUM2INT(mnth), NUM2INT(dy),
                                NUM2INT(hr), NUM2INT(mm), NUM2INT(ss), NUM2INT(fsec),
                                TO_STRING_PTR(timezone), TO_STRING_LEN(timezone)));
    return self;
}

static VALUE odt_sys_time_stamp(VALUE self, VALUE ses)
{
    oci_lc(OCIDateTimeSysTimeStamp(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self)));
    return self;
}

static VALUE odt_assign(VALUE self, VALUE ses, VALUE from)
{
    oci_lc(OCIDateTimeAssign(TO_HNDL(ses), oci8_errhp, TO_DATETIME(from), TO_DATETIME(self)));
    return self;
}

static VALUE odt_to_text(VALUE self, VALUE ses, VALUE fmt, VALUE fsprec, VALUE lang)
{
    char buf[512];
    ub4 bufsize = sizeof(buf);
    CHECK_STRING(fmt);
    Check_Type(fsprec, T_FIXNUM);
    CHECK_STRING(lang);
    oci_lc(OCIDateTimeToText(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                             TO_STRING_PTR(fmt), TO_STRING_LEN(fmt), FIX2INT(fsprec),
                             TO_STRING_PTR(lang), TO_STRING_LEN(lang),
                             &bufsize, buf));
    return rb_str_new(buf, bufsize);
}

static VALUE odt_from_text(VALUE self, VALUE ses, VALUE dstr, VALUE fmt, VALUE lang)
{
    StringValue(dstr);
    CHECK_STRING(fmt);
    CHECK_STRING(lang);
    oci_lc(OCIDateTimeFromText(TO_HNDL(ses), oci8_errhp, RSTRING(dstr)->ptr,
                               RSTRING(dstr)->len, TO_STRING_PTR(fmt), TO_STRING_LEN(fmt),
                               TO_STRING_PTR(lang), TO_STRING_LEN(lang), TO_DATETIME(self)));
    return self;
}

static VALUE odt_compare(VALUE lhs, VALUE ses, VALUE rhs)
{
    sword result;
    oci_lc(OCIDateTimeCompare(TO_HNDL(ses), oci8_errhp, DATA_PTR(lhs),
                              TO_DATETIME(rhs), &result));
    return INT2FIX(result);
}

static VALUE odt_check(VALUE self, VALUE ses)
{
    ub4 valid;
    oci_lc(OCIDateTimeCheck(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self), &valid));
    return INT2FIX(valid);
}

static VALUE odt_convert(VALUE self, VALUE ses, VALUE from)
{
    oci_lc(OCIDateTimeConvert(TO_HNDL(ses), oci8_errhp, TO_DATETIME(from),
                              TO_DATETIME(self)));
    return self;
}

static VALUE odt_subtract(VALUE self, VALUE ses, VALUE dt, VALUE out)
{
    oci_lc(OCIDateTimeSubtract(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                               TO_DATETIME(dt), TO_INTERVAL(out)));
    return out;
}

static VALUE odt_interval_add(VALUE self, VALUE ses, VALUE iv, VALUE out)
{
    oci_lc(OCIDateTimeIntervalAdd(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                                  TO_INTERVAL(iv), TO_DATETIME(out)));
    return out;
}

static VALUE odt_interval_sub(VALUE self, VALUE ses, VALUE iv, VALUE out)
{
    oci_lc(OCIDateTimeIntervalSub(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self),
                                  TO_INTERVAL(iv), TO_DATETIME(out)));
    return out;
}

static VALUE oiv_subtract(VALUE self, VALUE ses, VALUE iv, VALUE out)
{
    oci_lc(OCIIntervalSubtract(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                               TO_INTERVAL(iv), TO_INTERVAL(out)));
    return out;
}

static VALUE oiv_add(VALUE self, VALUE ses, VALUE iv, VALUE out)
{
    oci_lc(OCIIntervalAdd(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                          TO_INTERVAL(iv), TO_INTERVAL(out)));
    return out;
}

static VALUE oiv_mul(VALUE self, VALUE ses, VALUE num, VALUE out)
{
    oci_lc(OCIIntervalMultiply(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                               oci8_get_ocinumber(num), TO_INTERVAL(out)));
    return out;
}

static VALUE oiv_div(VALUE self, VALUE ses, VALUE num, VALUE out)
{
    oci_lc(OCIIntervalDivide(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                             oci8_get_ocinumber(num), TO_INTERVAL(out)));
    return out;
}

static VALUE oiv_compare(VALUE self, VALUE ses, VALUE iv)
{
    sword result;
    oci_lc(OCIIntervalCompare(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                              TO_INTERVAL(iv), &result));
    return INT2FIX(result);
}

static VALUE oiv_from_number(VALUE self, VALUE ses, VALUE num)
{
    oci_lc(OCIIntervalFromNumber(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                                 oci8_get_ocinumber(num)));
    return self;
}

static VALUE oiv_from_text(VALUE self, VALUE ses, VALUE str)
{
    StringValue(str);
    oci_lc(OCIIntervalFromText(TO_HNDL(ses), oci8_errhp, RSTRING(str)->ptr,
                               RSTRING(str)->len, TO_INTERVAL(self)));
    return self;
}

static VALUE oiv_to_text(VALUE self, VALUE ses, VALUE lfprec, VALUE fsprec)
{
    char buf[512];
    size_t buflen = sizeof(buf);
    Check_Type(lfprec, T_FIXNUM);
    Check_Type(fsprec, T_FIXNUM);
    oci_lc(OCIIntervalToText(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                             FIX2INT(lfprec), FIX2INT(fsprec),
                             buf, buflen, &buflen));
    return rb_str_new(buf, buflen);
}

static VALUE oiv_to_number(VALUE self, VALUE ses, VALUE out)
{
    oci_lc(OCIIntervalToNumber(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                               oci8_get_ocinumber(out)));
    return out;
}

static VALUE oiv_check(VALUE self, VALUE ses)
{
    ub4 valid;
    oci_lc(OCIIntervalCheck(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self), &valid));
    return FIX2INT(valid);
}

static VALUE oiv_assign(VALUE self, VALUE ses, VALUE out)
{
    oci_lc(OCIIntervalAssign(TO_HNDL(ses), oci8_errhp, TO_INTERVAL(self),
                             TO_INTERVAL(out)));
    return out;
}

static VALUE oiv_set_year_month(VALUE self, VALUE ses, VALUE yr, VALUE mnth)
{
    oci_lc(OCIIntervalSetYearMonth(TO_HNDL(ses), oci8_errhp, NUM2INT(yr), NUM2INT(mnth),
                                   TO_INTERVAL(self)));
    return self;
}

static VALUE oiv_get_year_month(VALUE self, VALUE ses)
{
    sb4 yr;
    sb4 mnth;
    oci_lc(OCIIntervalGetYearMonth(TO_HNDL(ses), oci8_errhp, &yr, &mnth, TO_INTERVAL(self)));
    return rb_ary_new3(2, INT2FIX(yr), INT2FIX(mnth));
}

static VALUE oiv_set_day_second(VALUE self, VALUE ses, VALUE dy, VALUE hr, VALUE mm, VALUE ss, VALUE fsec)
{
    oci_lc(OCIIntervalSetDaySecond(TO_HNDL(ses), oci8_errhp, NUM2INT(dy), NUM2INT(hr),
                                   NUM2INT(mm), NUM2INT(ss), NUM2INT(fsec), TO_INTERVAL(self)));
    return self;
}

static VALUE oiv_get_day_second(VALUE self, VALUE ses)
{
    sb4 dy;
    sb4 hr;
    sb4 mm;
    sb4 ss;
    sb4 fsec;
    oci_lc(OCIIntervalGetDaySecond(TO_HNDL(ses), oci8_errhp, &dy, &hr,
                                   &mm, &ss, &fsec, TO_INTERVAL(self)));
    return rb_ary_new3(5, INT2FIX(dy), INT2FIX(hr), INT2FIX(mm), INT2FIX(ss), INT2FIX(fsec));
}

static VALUE odt_get_timezone_name(VALUE self, VALUE ses)
{
    char buf[512];
    ub4 buflen = sizeof(buf);
    oci_lc(OCIDateTimeGetTimeZoneName(TO_HNDL(ses), oci8_errhp, TO_DATETIME(self), buf, &buflen));
    return rb_str_new(buf, buflen);
}

static VALUE oiv_from_tz(VALUE self, VALUE ses, VALUE str)
{
    StringValue(str);
    oci_lc(OCIIntervalFromTZ(TO_HNDL(ses), oci8_errhp, RSTRING(str)->ptr,
                             RSTRING(str)->len, TO_INTERVAL(self)));
    return self;
}

static inline ub4 sqlt_to_datetime_dtype(ub2 dty)
{
    switch (dty) {
    case SQLT_DATE:
        return OCI_DTYPE_DATE;
    case SQLT_TIME:
        return OCI_DTYPE_TIME;
    case SQLT_TIME_TZ:
        return OCI_DTYPE_TIME_TZ;
    case SQLT_TIMESTAMP:
        return OCI_DTYPE_TIMESTAMP;
    case SQLT_TIMESTAMP_TZ:
        return OCI_DTYPE_TIMESTAMP_TZ;
    case SQLT_TIMESTAMP_LTZ:
        return OCI_DTYPE_TIMESTAMP_LTZ;
    }
}

static inline ub4 sqlt_to_interval_dtype(ub2 dty)
{
    switch (dty) {
    case SQLT_INTERVAL_YM:
        return OCI_DTYPE_INTERVAL_YM;
    case SQLT_INTERVAL_DS:
        return OCI_DTYPE_INTERVAL_DS;
    }
}

typedef struct {
    oci8_bind_handle_t base;
    ub2 dty;
} oci8_bind_datetime_t;

static VALUE bind_datetime_get(oci8_bind_t *b)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t*)b;
    ub4 type = sqlt_to_datetime_dtype(bd->dty);
    VALUE obj = rb_obj_alloc(cOCIDateTime);
    odt_initialize(obj, INT2FIX(type));
    oci_lc(OCIDateTimeAssign(oci8_envhp, oci8_errhp, bd->base.hp, TO_DATETIME(obj)));
    return obj;
}

static VALUE bind_interval_get(oci8_bind_t *b)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t*)b;
    ub4 type = sqlt_to_interval_dtype(bd->dty);
    VALUE obj = rb_obj_alloc(cOCIInterval);
    odt_initialize(obj, INT2FIX(type));
    oci_lc(OCIIntervalAssign(oci8_envhp, oci8_errhp, bd->base.hp, TO_INTERVAL(obj)));
    return obj;
}

static void bind_datetime_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t*)b;
    ub4 type = sqlt_to_datetime_dtype(bd->dty);
    oci8_base_t *base;

    if (!rb_obj_is_kind_of(val, cOCIDateTime)) { 
        rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(val)), rb_class2name(cOCIDateTime));
    }
    base = DATA_PTR(val);
    if (base->type == type) {
        oci_lc(OCIDateTimeAssign(oci8_envhp, oci8_errhp, base->hp, bd->base.hp));
    } else {
        oci_lc(OCIDateTimeConvert(oci8_envhp, oci8_errhp, base->hp, bd->base.hp));
    }
}

static void bind_interval_set(oci8_bind_t *b, VALUE val)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t*)b;
    oci8_base_t *base;

    if (!rb_obj_is_kind_of(val, cOCIInterval)) { 
        rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(val)), rb_class2name(cOCIInterval));
    }
    base = DATA_PTR(val);
    oci_lc(OCIIntervalAssign(oci8_envhp, oci8_errhp, base->hp, bd->base.hp));
}

static void bind_datetime_init(oci8_bind_t *b, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t *)b;

    b->valuep = &bd->base.hp;
    b->value_sz = sizeof(bd->base.hp);
    if (NIL_P(*val)) {
        VALUE obj = rb_obj_alloc(cOCIDateTime);
        odt_initialize(obj, INT2FIX(sqlt_to_datetime_dtype(bd->dty)));
        *val = obj;
    }
}

static void bind_interval_init(oci8_bind_t *b, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_datetime_t *bd = (oci8_bind_datetime_t *)b;

    b->valuep = &bd->base.hp;
    b->value_sz = sizeof(bd->base.hp);
    if (NIL_P(*val)) {
        VALUE obj = rb_obj_alloc(cOCIInterval);
        odt_initialize(obj, INT2FIX(sqlt_to_interval_dtype(bd->dty)));
        *val = obj;
    }
}

static oci8_bind_class_t bind_date_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_datetime_get,
    bind_datetime_set,
    bind_datetime_init,
    SQLT_DATE
};

static oci8_bind_class_t bind_timestamp_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_datetime_get,
    bind_datetime_set,
    bind_datetime_init,
    SQLT_TIMESTAMP
};

static oci8_bind_class_t bind_timestamp_tz_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_datetime_get,
    bind_datetime_set,
    bind_datetime_init,
    SQLT_TIMESTAMP_TZ
};

static oci8_bind_class_t bind_timestamp_ltz_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_datetime_get,
    bind_datetime_set,
    bind_datetime_init,
    SQLT_TIMESTAMP_LTZ
};

static oci8_bind_class_t bind_interval_ym_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_interval_get,
    bind_interval_set,
    bind_interval_init,
    SQLT_INTERVAL_YM
};

static oci8_bind_class_t bind_interval_ds_class = {
    {
        oci8_bind_handle_mark,
        oci8_bind_free,
        sizeof(oci8_bind_datetime_t)
    },
    bind_interval_get,
    bind_interval_set,
    bind_interval_init,
    SQLT_INTERVAL_DS
};

void Init_oci_datetime(void)
{
    cOCIDateTime = oci8_define_class("OCIDateTime", &oci8_base_class);
    cOCIInterval = oci8_define_class("OCIInterval", &oci8_base_class);
    oci8_define_bind_class("OCIDate", &bind_date_class);
    oci8_define_bind_class("OCITimestamp", &bind_timestamp_class);
    oci8_define_bind_class("OCITimestampTZ", &bind_timestamp_tz_class);
    oci8_define_bind_class("OCITimestampLTZ", &bind_timestamp_ltz_class);
    oci8_define_bind_class("OCIIntervalYM", &bind_interval_ym_class);
    oci8_define_bind_class("OCIIntervalDS", &bind_interval_ds_class);

    rb_define_private_method(cOCIDateTime, "__initialize", odt_initialize, 1);
    rb_define_private_method(cOCIInterval, "__initialize", odt_initialize, 1);

    rb_define_private_method(cOCIDateTime, "__get_time", odt_get_time, 1);
    rb_define_private_method(cOCIDateTime, "__get_date", odt_get_date, 1);
    rb_define_private_method(cOCIDateTime, "__get_timezone_offset", odt_get_timezone_offset, 1);
    rb_define_private_method(cOCIDateTime, "__construct", odt_construct, 9);
    rb_define_private_method(cOCIDateTime, "__sys_time_stamp", odt_sys_time_stamp, 1);
    rb_define_private_method(cOCIDateTime, "__assign", odt_assign, 2);
    rb_define_private_method(cOCIDateTime, "__to_text", odt_to_text, 4);
    rb_define_private_method(cOCIDateTime, "__from_text", odt_from_text, 4);
    rb_define_private_method(cOCIDateTime, "__compare", odt_compare, 2);
    rb_define_private_method(cOCIDateTime, "__check", odt_check, 1);
    rb_define_private_method(cOCIDateTime, "__convert", odt_convert, 2);
    rb_define_private_method(cOCIDateTime, "__subtract", odt_subtract, 3);
    rb_define_private_method(cOCIDateTime, "__interval_add", odt_interval_add, 3);
    rb_define_private_method(cOCIDateTime, "__interval_sub", odt_interval_sub, 3);
    rb_define_private_method(cOCIInterval, "__subtract", oiv_subtract, 3);
    rb_define_private_method(cOCIInterval, "__add", oiv_add, 3);
    rb_define_private_method(cOCIInterval, "__mul", oiv_mul, 3);
    rb_define_private_method(cOCIInterval, "__div", oiv_div, 3);
    rb_define_private_method(cOCIInterval, "__compare", oiv_compare, 2);
    rb_define_private_method(cOCIInterval, "__from_number", oiv_from_number, 2);
    rb_define_private_method(cOCIInterval, "__from_text", oiv_from_text, 2);
    rb_define_private_method(cOCIInterval, "__to_text", oiv_to_text, 3);
    rb_define_private_method(cOCIInterval, "__to_number", oiv_to_number, 2);
    rb_define_private_method(cOCIInterval, "__check", oiv_check, 1);
    rb_define_private_method(cOCIInterval, "__assign", oiv_assign, 2);
    rb_define_private_method(cOCIInterval, "__set_year_month", oiv_set_year_month, 3);
    rb_define_private_method(cOCIInterval, "__get_year_month", oiv_get_year_month, 1);
    rb_define_private_method(cOCIInterval, "__set_day_second", oiv_set_day_second, 6);
    rb_define_private_method(cOCIInterval, "__get_day_second", oiv_get_day_second, 1);
    rb_define_private_method(cOCIDateTime, "__get_timezone_name", odt_get_timezone_name, 1);
    rb_define_private_method(cOCIInterval, "__from_tz", oiv_from_tz, 2);
}
