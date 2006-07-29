/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  oradate.c
 *
 * $Author$
 * $Date$
 *
 * Copyright (C) 2002-2005 KUBO Takehiro <kubo@jiubao.org>
 *
 * date and time between 4712 B.C. and 9999 A.D.
 */
#include "oci8.h"
#include <time.h>

static VALUE cOraDate;

/* OraDate - Internal format of DATE */
struct ora_date {
    unsigned char century;
    unsigned char year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
};
typedef struct ora_date ora_date_t;

#define Set_year(od, y) (od)->century = y / 100 + 100, (od)->year = y % 100 + 100
#define Set_month(od, m) (od)->month = m
#define Set_day(od, d)   (od)->day = d
#define Set_hour(od, h)  (od)->hour = h + 1
#define Set_minute(od, m) (od)->minute = m + 1
#define Set_second(od, s) (od)->second = s + 1

#define Get_year(od) (((od)->century - 100) * 100 + ((od)->year - 100))
#define Get_month(od) ((od)->month)
#define Get_day(od) ((od)->day)
#define Get_hour(od) ((od)->hour - 1)
#define Get_minute(od) ((od)->minute - 1)
#define Get_second(od) ((od)->second - 1)

#define Check_year(year) \
    if (year < -4712 || 9999 < year) \
        rb_raise(rb_eRangeError, "Out of range for year %d (expect -4712 .. 9999)", year)
#define Check_month(month) \
    if (month < 1 || 12 < month) \
        rb_raise(rb_eRangeError, "Out of range for month %d (expect 1 .. 12)", month)
#define Check_day(day) \
    if (day < 1 || 31 < day) \
        rb_raise(rb_eRangeError, "Out of range for day %d (expect 1 .. 31)", day)
#define Check_hour(hour) \
    if (hour < 0 || 23 < hour) \
        rb_raise(rb_eRangeError, "Out of range for hour %d (expect 0 .. 24)", hour)
#define Check_minute(min) \
    if (min < 0 || 59 < min) \
        rb_raise(rb_eRangeError, "Out of range for minute %d (expect 0 .. 59)", min)
#define Check_second(sec) \
    if (sec < 0 || 59 < sec) \
        rb_raise(rb_eRangeError, "Out of range for second %d (expect 0 .. 59)", sec)

static void oci8_set_ora_date(ora_date_t *od, int year, int month, int day, int hour, int minute, int second)
{
    Set_year(od, year);
    Set_month(od, month);
    Set_day(od, day);
    Set_hour(od, hour);
    Set_minute(od, minute);
    Set_second(od, second);
}

static VALUE ora_date_s_allocate(VALUE klass)
{
    ora_date_t *od;
    return Data_Make_Struct(klass, ora_date_t, NULL, xfree, od);
}

/*
=begin
--- OraDate.new([year [, month [, day [, hour [, min [,sec]]]]]])
=end
*/
static VALUE ora_date_initialize(int argc, VALUE *argv, VALUE self)
{
    VALUE vyear, vmonth, vday, vhour, vmin, vsec;
    ora_date_t *od = DATA_PTR(self);
    int year, month, day, hour, min, sec;

    rb_scan_args(argc, argv, "06", &vyear, &vmonth, &vday, &vhour, &vmin, &vsec);
    /* set year */
    if (argc > 0) {
        year = NUM2INT(vyear);
        Check_year(year);
    } else {
        year = 1;
    }
    /* set month */
    if (argc > 1) {
        month = NUM2INT(vmonth);
        Check_month(month);
    } else {
        month = 1;
    }
    /* set day */
    if (argc > 2) {
        day = NUM2INT(vday);
        Check_day(day);
    } else {
        day = 1;
    }
    /* set hour */
    if (argc > 3) {
        hour = NUM2INT(vhour);
        Check_hour(hour);
    } else {
        hour = 0;
    }
    /* set minute */
    if (argc > 4) {
        min = NUM2INT(vmin);
        Check_minute(min);
    } else {
        min = 0;
    }
    /* set second */
    if (argc > 5) {
        sec = NUM2INT(vsec);
        Check_second(sec);
    } else {
        sec = 0;
    }

    oci8_set_ora_date(od, year, month, day, hour, min, sec);
    return Qnil;
}


static VALUE ora_date_initialize_copy(VALUE lhs, VALUE rhs)
{
    ora_date_t *l, *r;

    rb_obj_init_copy(lhs, rhs);
    Data_Get_Struct(lhs, ora_date_t, l);
    Data_Get_Struct(rhs, ora_date_t, r);
    memcpy(r, l, sizeof(ora_date_t));
    return lhs;
}  

/*
=begin
--- OraDate.now()
=end
*/
static VALUE ora_date_s_now(int argc, VALUE *argv, VALUE klass)
{
    VALUE obj = ora_date_s_allocate(klass);
    ora_date_t *od = DATA_PTR(obj);
    time_t tm = time(0);
    int year, month, day, hour, min, sec;
#ifdef HAVE_LOCALTIME_R
    struct tm t;
    localtime_r(&tm, &t);
#define tp (&t)
#else
    struct tm *tp;
    tp = localtime(&tm);
#endif
    year = tp->tm_year + 1900;
    month = tp->tm_mon + 1;
    day = tp->tm_mday;
    hour = tp->tm_hour;
    min = tp->tm_min;
    sec = tp->tm_sec;

    oci8_set_ora_date(od, year, month, day, hour, min, sec);
    return obj;
}

/*
=begin
--- OraDate#to_s()
=end
*/
static VALUE ora_date_to_s(VALUE self)
{
    ora_date_t *od;
    char buf[30];

    Data_Get_Struct(self, ora_date_t, od);
    sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", Get_year(od), Get_month(od),
            Get_day(od), Get_hour(od), Get_minute(od), Get_second(od));
    return rb_str_new2(buf);
}

/*
=begin
--- OraDate#to_a()
=end
*/
static VALUE ora_date_to_a(VALUE self)
{
    ora_date_t *od;
    VALUE ary[6];
  
    Data_Get_Struct(self, ora_date_t, od);
    ary[0] = INT2FIX(Get_year(od));
    ary[1] = INT2FIX(Get_month(od));
    ary[2] = INT2FIX(Get_day(od));
    ary[3] = INT2FIX(Get_hour(od));
    ary[4] = INT2FIX(Get_minute(od));
    ary[5] = INT2FIX(Get_second(od));
    return rb_ary_new4(6, ary);
}

#define DEFINE_GETTER_FUNC(where) \
static VALUE ora_date_##where(VALUE self) \
{ \
    ora_date_t *od; \
 \
    Data_Get_Struct(self, ora_date_t, od); \
    return INT2FIX(Get_##where(od)); \
}

#define DEFINE_SETTER_FUNC(where) \
static VALUE ora_date_set_##where(VALUE self, VALUE val) \
{ \
    ora_date_t *od; \
    int v; \
 \
    v = NUM2INT(val); \
    Check_##where(v); \
    Data_Get_Struct(self, ora_date_t, od); \
    Set_##where(od, v); \
    return self; \
}

/*
=begin
--- OraDate#year
=end
*/
DEFINE_GETTER_FUNC(year)
DEFINE_SETTER_FUNC(year)

/*
=begin
--- OraDate#month
=end
*/
DEFINE_GETTER_FUNC(month)
DEFINE_SETTER_FUNC(month)

/*
=begin
--- OraDate#day
=end
*/
DEFINE_GETTER_FUNC(day)
DEFINE_SETTER_FUNC(day)

/*
=begin
--- OraDate#hour
=end
*/
DEFINE_GETTER_FUNC(hour)
DEFINE_SETTER_FUNC(hour)

/*
=begin
--- OraDate#minute
=end
*/
DEFINE_GETTER_FUNC(minute)
DEFINE_SETTER_FUNC(minute)

/*
=begin
--- OraDate#second
=end
*/
DEFINE_GETTER_FUNC(second)
DEFINE_SETTER_FUNC(second)

/*
=begin
--- OraDate#trunc()
=end
*/
static VALUE ora_date_trunc(VALUE self)
{
  ora_date_t *od;

  Data_Get_Struct(self, ora_date_t, od);
  od->hour = 1;
  od->minute = 1;
  od->second = 1;
  return self;
}

/*
=begin
--- OraDate#<=>(other)
=end
*/
static VALUE ora_date_cmp(VALUE self, VALUE val)
{
    ora_date_t *od1, *od2;
    Data_Get_Struct(self, ora_date_t, od1);
    Check_Object(val, cOraDate);
    Data_Get_Struct(val, ora_date_t, od2);
    if (od1->century < od2->century) return INT2FIX(-1);
    if (od1->century > od2->century) return INT2FIX(1);
    if (od1->year < od2->year) return INT2FIX(-1);
    if (od1->year > od2->year) return INT2FIX(1);
    if (od1->month < od2->month) return INT2FIX(-1);
    if (od1->month > od2->month) return INT2FIX(1);
    if (od1->day < od2->day) return INT2FIX(-1);
    if (od1->day > od2->day) return INT2FIX(1);
    if (od1->hour < od2->hour) return INT2FIX(-1);
    if (od1->hour > od2->hour) return INT2FIX(1);
    if (od1->minute < od2->minute) return INT2FIX(-1);
    if (od1->minute > od2->minute) return INT2FIX(1);
    if (od1->second < od2->second) return INT2FIX(-1);
    if (od1->second > od2->second) return INT2FIX(1);
    return INT2FIX(0);
}

static VALUE ora_date_dump(int argc, VALUE *argv, VALUE self)
{
    ora_date_t *od;
    Data_Get_Struct(self, ora_date_t, od);
    return rb_str_new((const char*)od, sizeof(ora_date_t));
}  

static VALUE ora_date_s_load(VALUE klass, VALUE str)
{
    ora_date_t *od;
    VALUE obj;

    Check_Type(str, T_STRING);
    if (RSTRING(str)->len != sizeof(ora_date_t)) {
        rb_raise(rb_eTypeError, "marshaled OraDate format differ");
    }
    obj = Data_Make_Struct(cOraDate, ora_date_t, NULL, xfree, od);
    memcpy(od, RSTRING(str)->ptr, sizeof(ora_date_t));
    return obj;
}  

/*
 * bind_oradate
 */
typedef struct {
    oci8_bind_t base;
    ora_date_t od;
} oci8_bind_oradate_t;

static VALUE bind_oradate_get(oci8_bind_t *bb)
{
    ora_date_t *od;

    VALUE obj = Data_Make_Struct(cOraDate, ora_date_t, NULL, xfree, od);
    memcpy(od, bb->valuep, sizeof(ora_date_t));
    return obj;
}

static void bind_oradate_set(oci8_bind_t *bb, VALUE val)
{
    ora_date_t *od;

    Check_Object(val, cOraDate);
    Data_Get_Struct(val, ora_date_t, od);
    memcpy(bb->valuep, od, sizeof(ora_date_t));
}

static void bind_oradate_init(oci8_bind_t *bb, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_oradate_t *bo = (oci8_bind_oradate_t *)bb;

    bb->valuep = &bo->od;
    bb->value_sz = sizeof(ora_date_t);
}

static oci8_bind_class_t bind_oradate_class = {
    {
        NULL,
        oci8_bind_free,
        sizeof(oci8_bind_oradate_t)
    },
    bind_oradate_get,
    bind_oradate_set,
    bind_oradate_init,
    SQLT_DAT,
};

void Init_ora_date(void)
{
    cOraDate = rb_define_class("OraDate", rb_cObject);

    rb_define_alloc_func(cOraDate, ora_date_s_allocate);
    rb_define_method(cOraDate, "initialize", ora_date_initialize, -1);
    rb_define_method(cOraDate, "initialize_copy", ora_date_initialize_copy, 1);
    rb_define_singleton_method(cOraDate, "now", ora_date_s_now, 0);
    rb_define_method(cOraDate, "to_s", ora_date_to_s, 0);
    rb_define_method(cOraDate, "to_a", ora_date_to_a, 0);

    rb_define_method(cOraDate, "year", ora_date_year, 0);
    rb_define_method(cOraDate, "year=", ora_date_set_year, 1);

    rb_define_method(cOraDate, "month", ora_date_month, 0);
    rb_define_method(cOraDate, "month=", ora_date_set_month, 1);

    rb_define_method(cOraDate, "day", ora_date_day, 0);
    rb_define_method(cOraDate, "day=", ora_date_set_day, 1);

    rb_define_method(cOraDate, "hour", ora_date_hour, 0);
    rb_define_method(cOraDate, "hour=", ora_date_set_hour, 1);

    rb_define_method(cOraDate, "minute", ora_date_minute, 0);
    rb_define_method(cOraDate, "minute=", ora_date_set_minute, 1);

    rb_define_method(cOraDate, "second", ora_date_second, 0);
    rb_define_method(cOraDate, "second=", ora_date_set_second, 1);

    rb_define_method(cOraDate, "trunc", ora_date_trunc, 0);

    rb_define_method(cOraDate, "<=>", ora_date_cmp, 1);
    rb_include_module(cOraDate, rb_mComparable);

    rb_define_method(cOraDate, "_dump", ora_date_dump, -1);
    rb_define_singleton_method(cOraDate, "_load", ora_date_s_load, 1);

    oci8_define_bind_class("OraDate", &bind_oradate_class);
}
