/*
  oranumber.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OraNumber
This is not a numeric class, so algebra operations and setter methods
are not permitted, but a place to hold fetched date from Oracle server.

In fact as matter, this is a test class to check the algorithm to 
convert Oracle internal number format to Ruby's number, which is internally used by,
for example, ((<OCIStmt#attrGet|OCIHandle#attrGet>))(((<OCI_ATTR_MIN>))).
=end
*/
#include "oci8.h"
#include "math.h"

#define Get_Sign(on) ((on)->exponent & 0x80)
#define Get_Exp_Part(on) ((on)->exponent & 0x7F)
#define Get_Exponent(on) (Get_Sign(on) ? (Get_Exp_Part(on) - 65) : (~(Get_Exp_Part(on)) + 63))
#define Get_Mantissa_At(on, i) (Get_Sign(on) ? (on)->mantissa[i] - 1 : 101 - (on)->mantissa[i])

static ora_vnumber_t *get_ora_number(VALUE self)
{
  ora_vnumber_t *ovn;
  Data_Get_Struct(self, ora_vnumber_t, ovn);
  return ovn;
}

static VALUE make_ora_number(const ora_vnumber_t *ovn)
{
  VALUE obj;
  ora_vnumber_t *newovn;

  obj = Data_Make_Struct(cOraNumber, ora_vnumber_t, NULL, xfree, newovn);
  memcpy(newovn, ovn, sizeof(ora_vnumber_t));
  return obj;
}

/*
=begin
--- OraNumber.new()
=end
*/
static VALUE ora_number_s_new(int argc, VALUE *argv, VALUE klass)
{
  ora_vnumber_t ovn;

  ovn.size = 1;
  ovn.num.exponent = 0x80;
  memset(ovn.num.mantissa, 0, sizeof(ovn.num.mantissa));
  return make_ora_number(&ovn);
}

static VALUE ora_number_clone(VALUE self)
{
  return make_ora_number(get_ora_number(self));
}

/*
=begin
--- OraNumber#to_i()
=end
*/
static VALUE ora_number_to_i(VALUE self)
{
  ora_vnumber_t *ovn = get_ora_number(self);
  unsigned char buf[ORA_NUMBER_BUF_SIZE];

  ora_number_to_str(buf, NULL, &(ovn->num), ovn->size);
  return rb_cstr2inum(buf, 10);
}

/*
=begin
--- OraNumber#to_f()
=end
*/
static VALUE ora_number_to_f(VALUE self)
{
  ora_vnumber_t *ovn = get_ora_number(self);
  unsigned char buf[ORA_NUMBER_BUF_SIZE];

  ora_number_to_str(buf, NULL, &(ovn->num), ovn->size);
  return rb_float_new(rb_cstr_to_dbl(buf, Qfalse));
}

/*
=begin
--- OraNumber#to_s()
=end
*/
static VALUE ora_number_to_s(VALUE self)
{
  ora_vnumber_t *ovn = get_ora_number(self);
  unsigned char buf[ORA_NUMBER_BUF_SIZE];
  size_t len;

  ora_number_to_str(buf, &len, &(ovn->num), ovn->size);
  return rb_str_new(buf, len);
}

static VALUE ora_number_uminus(VALUE self)
{
  ora_vnumber_t *ovn = get_ora_number(self);
  VALUE obj;
  int i;

  if (ovn->num.exponent == 0x80)
    return self;
  obj = ora_number_clone(self);
  ovn = get_ora_number(obj);
  ovn->num.exponent = ~(ovn->num.exponent);
  for (i = 0;i < ovn->size - 1;i++)
    ovn->num.mantissa[i] = 102 - ovn->num.mantissa[i];
  if (Get_Sign(&(ovn->num))) {
    if (ovn->size != 21 || ovn->num.mantissa[19] == 0x00) {
      ovn->size--;
    }
  } else {
    if (ovn->size != 21) {
      ovn->num.mantissa[ovn->size - 1] = 102;
      ovn->size++;
    }
  }
  return obj;
}

void Init_ora_number(void)
{
  rb_define_singleton_method(cOraNumber, "new", ora_number_s_new, -1);
  rb_define_method(cOraNumber, "to_i", ora_number_to_i, 0);
  rb_define_method(cOraNumber, "to_f", ora_number_to_f, 0);
  rb_define_method(cOraNumber, "to_s", ora_number_to_s, 0);
  rb_define_method(cOraNumber, "-@", ora_number_uminus, 0);
}

void ora_number_to_str(unsigned char *buf, size_t *lenp, ora_number_t *on, unsigned char size)
{
  int exponent;
  int len = 0;
  int mentissa_size;
  int i, j;

  if (on->exponent == 0x80) {
    buf[0] = '0';
    buf[1] = '\0';
    if (lenp != NULL)
      *lenp = 1;
    return;
  }

  if (Get_Sign(on)) {
    mentissa_size = size - 1;
  } else {
    if (size == 21 && on->mantissa[19] != 102)
      mentissa_size = 20;
    else
      mentissa_size = size - 2;
    buf[len++] = '-';
  }
  exponent = Get_Exponent(on);
  if (exponent < 0) {
    buf[len++] = '0';
    buf[len++] = '.';
    for (i = exponent * 2;i < -2;i++)
      buf[len++] = '0';
    for (i = 0;i < mentissa_size;i++) {
      j = Get_Mantissa_At(on, i);
      buf[len++] = j / 10 + '0';
      if (i != mentissa_size - 1 || j % 10 != 0)
	buf[len++] = j % 10 + '0';
    }
  } else {
    for (i = 0;i < mentissa_size;i++) {
      j = Get_Mantissa_At(on, i);
      if (i == exponent + 1) {
	buf[len++] = '.';
      }
      if (i != 0 || j / 10 != 0)
	buf[len++] = j / 10 + '0';
      if ((i < exponent + 1) /* integer part */
	  || (i != mentissa_size - 1 || j % 10 != 0)  /* decimal fraction */)
	buf[len++] = j % 10 + '0';
    }
    for (;i <= exponent;i++) {
      buf[len++] = '0';
      buf[len++] = '0';
    }
  }
  buf[len] = '\0';
  if (lenp != NULL)
    *lenp = len;
}
