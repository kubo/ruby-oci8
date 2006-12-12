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
static ID id_offset;
static ID id_utc_offset;
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

/*
 * Document-class: OCI8::BindType::DateTime
 *
 * This is a helper class to bind ruby's
 * DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]
 * object as Oracle's <tt>TIMESTAMP WITH TIME ZONE</tt> datatype.
 *
 * == Select
 *
 * The fetched value for a <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH
 * TIME ZONE</tt> or <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt> column
 * is a DateTime[http://www.ruby-doc.org/core/classes/DateTime.html].
 * The time zone part is a session time zone if the Oracle datatype doesn't
 * have time zone information. The session time zone is the client machine's
 * time zone by default.
 *
 * You can change the session time zone by executing the following SQL.
 *
 *   ALTER SESSION SET TIME_ZONE='-05:00'
 *
 * == Bind
 *
 * To bind a DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]
 * value implicitly:
 *
 *   conn.exec("INSERT INTO lunar_landings(ship_name, landing_time) VALUES(:1, :2)",
 *             'Apollo 11',
 *             DateTime.parse('1969-7-20 20:17:40 00:00'))
 *
 * The bind variable <code>:2</code> is bound as <tt>TIMESTAMP WITH TIME ZONE</tt> on Oracle.
 *
 * To bind explicitly:
 *
 *   cursor = conn.exec("INSERT INTO lunar_landings(ship_name, landing_time) VALUES(:1, :2)")
 *   cursor.bind_param(':1', nil, String, 60)
 *   cursor.bind_param(':2', nil, DateTime)
 *   [['Apollo 11', DateTime.parse('1969-07-20 20:17:40 00:00'))],
 *    ['Apollo 12', DateTime.parse('1969-11-19 06:54:35 00:00'))],
 *    ['Apollo 14', DateTime.parse('1971-02-05 09:18:11 00:00'))],
 *    ['Apollo 15', DateTime.parse('1971-07-30 22:16:29 00:00'))],
 *    ['Apollo 16', DateTime.parse('1972-04-21 02:23:35 00:00'))],
 *    ['Apollo 17', DateTime.parse('1972-12-11 19:54:57 00:00'))]
 *   ].each do |ship_name, landing_time|
 *     cursor[':1'] = ship_name
 *     cursor[':2'] = landing_time
 *     cursor.exec
 *   end
 *   cursor.close
 *
 * On setting a object to the bind variable, you can use any object
 * which has at least three instance methods _year_, _mon_ (or _month_)
 * and _mday_ (or _day_). If the object responses to _hour_, _min_,
 * _sec_ or _sec_fraction_, the responsed values are used for hour,
 * minute, second or fraction of a second respectively.
 * If not, zeros are set. If the object responses to _offset_ or
 * _utc_offset_, it is used for time zone. If not, the session time
 * zone is used.
 *
 * The acceptable value are listed below.
 * _year_:: -4712 to 9999 [excluding year 0]
 * _mon_ (or _month_):: 0 to 12
 * _mday_ (or _day_):: 0 to 31 [depends on the month]
 * _hour_:: 0 to 23
 * _min_:: 0 to 59
 * _sec_:: 0 to 59
 * _sec_fraction_:: 0 to (999_999_999.to_r / (24*60*60* 1_000_000_000)) [999,999,999 nanoseconds]
 * _offset_:: (-12.to_r / 24) to (14.to_r / 24) [-12:00 to +14:00]
 * _utc_offset_:: -12*3600 <= utc_offset <= 24*3600 [-12:00 to +14:00]
 *
 * The output value of the bind varible is always a
 * DateTime[http://www.ruby-doc.org/core/classes/DateTime.html].
 *
 *   cursor = conn.exec("BEGIN :ts := current_timestamp; END")
 *   cursor.bind_param(:ts, nil, DateTime)
 *   cursor.exec
 *   cursor[:ts] # => a DateTime.
 *   cursor.close
 *
 */
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
        if (RTEST(rb_funcall(fs, id_less, 1, INT2FIX(0)))) {
            /* if fs < 0 */
            rb_raise(rb_eRangeError, "sec_fraction is less then zero.");
        }
        fs = rb_funcall(fs, id_mul, 1, fsec_base);
        if (RTEST(rb_funcall(INT2FIX(1000000000), id_less, 1, fs))) {
            /* if 1000000000 < fs */
            rb_raise(rb_eRangeError, "sec_fraction is greater than or equals to one second.");
        }
        fsec = NUM2UINT(fs);
    } else {
        fsec = 0;
    }
    /* time zone */
    if (rb_respond_to(val, id_offset)) {
        VALUE of = rb_funcall(val, id_offset, 0);
        char sign = '+';
        ival = NUM2INT(rb_funcall(of, id_mul, 1, INT2FIX(1440)));
        if (ival < 0) {
            sign = '-';
            ival = - ival;
        }
        sprintf(tz_str, "%c%02d:%02d", sign, ival / 60, ival % 60);
    } else if (rb_respond_to(val, id_utc_offset)) {
        char sign = '+';
        ival = NUM2INT(rb_funcall(val, id_utc_offset, 0));
        if (ival < 0) {
            sign = '-';
            ival = -ival;
        }
        ival /= 60;
        sprintf(tz_str, "%c%02d:%02d", sign, ival / 60, ival % 60);
    } else {
        /* use session timezone. */
        tz_str[0] = '\0';
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

static void bind_datetime_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length)
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

/*
 * Document-class: OCI8::BindType::IntervalYM
 *
 * This is a helper class to bind ruby's
 * Integer[http://www.ruby-doc.org/core/classes/Integer.html]
 * object as Oracle's <tt>INTERVAL YEAR TO MONTH</tt> datatype.
 *
 * == Select
 *
 * The fetched value for a <tt>INTERVAL YEAR TO MONTH</tt> column
 * is an Integer[http://www.ruby-doc.org/core/classes/Integer.html]
 * which means the months between two timestamps.
 *
 * == Bind
 *
 * You cannot bind as <tt>INTERVAL YEAR TO MONTH</tt> implicitly.
 * It must be bound explicitly with :interval_ym.
 *
 *   # output bind variable
 *   cursor = conn.parse(<<-EOS)
 *     BEGIN
 *       :interval := (:ts1 - :ts2) YEAR TO MONTH;
 *     END;
 *   EOS
 *   cursor.bind_param(:interval, nil, :interval_ym)
 *   cursor.bind_param(:ts1, DateTime.parse('1969-11-19 06:54:35 00:00'))
 *   cursor.bind_param(:ts2, DateTime.parse('1969-07-20 20:17:40 00:00'))
 *   cursor.exec
 *   cursor[:interval] # => 4 (months)
 *   cursor.close
 *
 *   # input bind variable
 *   cursor = conn.parse(<<-EOS)
 *     BEGIN
 *       :ts1 := :ts2 + :interval;
 *     END;
 *   EOS
 *   cursor.bind_param(:ts1, nil, DateTime)
 *   cursor.bind_param(:ts2, Date.parse('1969-11-19'))
 *   cursor.bind_param(:interval, 4, :interval_ym)
 *   cursor.exec
 *   cursor[:ts1].strftime('%Y-%m-%d') # => 1970-03-19
 *   cursor.close
 *
 */
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

static void bind_interval_ym_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length)
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

/*
 * Document-class: OCI8::BindType::IntervalDS
 *
 * This is a helper class to bind ruby's
 * Rational[http://www.ruby-doc.org/core/classes/Rational.html]
 * object as Oracle's <tt>INTERVAL DAY TO SECOND</tt> datatype.
 *
 * == Select
 *
 * The fetched value for a <tt>INTERVAL DAY TO SECOND</tt> column
 * is a Rational[http://www.ruby-doc.org/core/classes/Rational.html]
 * or an Integer[http://www.ruby-doc.org/core/classes/Integer.html].
 * The value is usable to apply to
 * DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]#+ and
 * DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]#-.
 *
 * == Bind
 *
 * You cannot bind as <tt>INTERVAL YEAR TO MONTH</tt> implicitly.
 * It must be bound explicitly with :interval_ds.
 *
 *   # output
 *   ts1 = DateTime.parse('1969-11-19 06:54:35 00:00')
 *   ts2 = DateTime.parse('1969-07-20 20:17:40 00:00')
 *   cursor = conn.parse(<<-EOS)
 *     BEGIN
 *       :itv := (:ts1 - :ts2) DAY TO SECOND;
 *     END;
 *   EOS
 *   cursor.bind_param(:itv, nil, :interval_ds)
 *   cursor.bind_param(:ts1, ts1)
 *   cursor.bind_param(:ts2, ts2)
 *   cursor.exec
 *   cursor[:itv] # == ts1 - ts2
 *   cursor.close
 *
 *   # input
 *   ts2 = DateTime.parse('1969-07-20 20:17:40 00:00')
 *   itv = 121 + 10.to_r/24 + 36.to_r/(24*60) + 55.to_r/(24*60*60)
 *   # 121 days, 10 hours,    36 minutes,       55 seconds
 *   cursor = conn.parse(<<-EOS)
 *     BEGIN
 *       :ts1 := :ts2 + :itv;
 *     END;
 *   EOS
 *   cursor.bind_param(:ts1, nil, DateTime)
 *   cursor.bind_param(:ts2, ts2)
 *   cursor.bind_param(:itv, itv, :interval_ds)
 *   cursor.exec
 *   cursor[:ts1].strftime('%Y-%m-%d %H:%M:%S') # => 1969-11-19 06:54:35
 *   cursor.close
 */
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
    if (RARRAY_LEN(ary) != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    day = NUM2INT(RARRAY_PTR(ary)[0]);
    /* hour */
    val = rb_funcall(RARRAY_PTR(ary)[1], id_mul, 1, INT2FIX(24));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY_LEN(ary) != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    hour = NUM2INT(RARRAY_PTR(ary)[0]);
    /* minute */
    val = rb_funcall(RARRAY_PTR(ary)[1], id_mul, 1, INT2FIX(60));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY_LEN(ary) != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    minute = NUM2INT(RARRAY_PTR(ary)[0]);
    /* second */
    val = rb_funcall(RARRAY_PTR(ary)[1], id_mul, 1, INT2FIX(60));
    ary = rb_funcall(val, id_divmod, 1, INT2FIX(1));
    Check_Type(ary, T_ARRAY);
    if (RARRAY_LEN(ary) != 2) {
        rb_raise(rb_eRuntimeError, "invalid array size");
    }
    sec = NUM2INT(RARRAY_PTR(ary)[0]);
    /* fsec */
    val = rb_funcall(RARRAY_PTR(ary)[1], id_mul, 1, INT2FIX(1000000000));
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

static void bind_interval_ds_init(oci8_bind_t *b, VALUE svc, VALUE *val, VALUE length)
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
#if 0 /* for rdoc */
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
    mOCI8BindType = rb_define_module_under(cOCI8, "BindType");
    cOCI8BindTypeBase = rb_define_class_under(mOCI8BindType, "Base", cOCIHandle);
#endif
    rb_require("date");

    cDateTime = rb_eval_string("DateTime");
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
    id_offset = rb_intern("offset");
    id_utc_offset = rb_intern("utc_offset");

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
#if 0 /* for rdoc */
    cOCI8BindTypeDateTime = rb_define_class_under(mOCI8BindType, "DateTime", cOCI8BindTypeBase);
    cOCI8BindTypeIntervalYS = rb_define_class_under(mOCI8BindType, "IntervalYM", cOCI8BindTypeBase);
    cOCI8BindTypeIntervalDS = rb_define_class_under(mOCI8BindType, "IntervalDS", cOCI8BindTypeBase);
#endif
}
