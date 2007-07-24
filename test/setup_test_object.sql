drop table rb_test_obj_tab1 purge
/
drop table rb_test_obj_tab2 purge
/
drop type rb_test_obj
/
drop type rb_test_obj_elem_ary_of_ary
/
drop type rb_test_obj_elem_array
/
drop type rb_test_obj_elem
/
create type rb_test_obj_elem as object (
  x integer,
  y integer
)
/
create or replace type rb_test_int_array as array(50) of integer
/
create or replace type rb_test_flt_array as array(50) of float
/
create or replace type rb_test_num_array as array(50) of number(10,1)
/
create or replace type rb_test_bdbl_array as array(50) of binary_double
/
create or replace type rb_test_bflt_array as array(50) of binary_float
/
create or replace type rb_test_str_array as array(50) of varchar2(50)
/
create or replace type rb_test_raw_array as array(50) of raw(50)
/
create or replace type rb_test_obj_elem_array as array(50) of rb_test_obj_elem
/
create or replace type rb_test_obj_elem_ary_of_ary as array(50) of rb_test_obj_elem_array
/
create type rb_test_obj as object (
  int_val integer,
  flt_val float,
  num_val number(10,1),
  bdbl_val binary_double,
  bflt_val binary_float,
  str_val varchar2(50),
  raw_val raw(50),
  obj_val rb_test_obj_elem,
  int_array_val rb_test_int_array,
  flt_array_val rb_test_flt_array,
  num_array_val rb_test_num_array,
  bdbl_array_val rb_test_bdbl_array,
  bflt_array_val rb_test_bflt_array,
  str_array_val rb_test_str_array,
  raw_array_val rb_test_raw_array,
  obj_array_val rb_test_obj_elem_array,
  obj_ary_of_ary_val rb_test_obj_elem_ary_of_ary,

  constructor function rb_test_obj(n number) return self as result,
  static function class_func(n number) return rb_test_obj,
  static procedure class_proc1(obj out rb_test_obj, n number),
  static procedure class_proc2(obj in out rb_test_obj),
  member function member_func return integer,
  member procedure member_proc(n in integer)
)
/
create or replace type body rb_test_obj is

  constructor function rb_test_obj(n number) return self as result is
  begin
    self.int_val := n;
    self.flt_val := n;
    self.num_val := n;
    self.bdbl_val := n;
    self.bflt_val := n;
    self.str_val := to_char(n);
    self.raw_val := utl_raw.cast_to_raw(to_char(n));
    self.obj_val := rb_test_obj_elem(n, n + 1);
    if self.int_val != 1 then
      self.int_array_val := rb_test_int_array(n, n + 1, n + 2);
      self.flt_array_val := rb_test_flt_array(n, n + 1, n + 2);
      self.num_array_val := rb_test_num_array(n, n + 1, n + 2);
      self.bdbl_array_val := rb_test_bdbl_array(n, n + 1, n + 2);
      self.bflt_array_val := rb_test_bflt_array(n, n + 1, n + 2);
      self.str_array_val := rb_test_str_array(to_char(n), to_char(n + 1), to_char(n + 2));
      self.raw_array_val := rb_test_raw_array(utl_raw.cast_to_raw(to_char(n)),
                                              utl_raw.cast_to_raw(to_char(n + 1)),
                                              utl_raw.cast_to_raw(to_char(n + 2)));
      self.obj_array_val := rb_test_obj_elem_array(rb_test_obj_elem(n + 0, n + 1),
                                                   rb_test_obj_elem(n + 1, n + 2),
                                                   rb_test_obj_elem(n + 2, n + 3));
      self.obj_ary_of_ary_val := rb_test_obj_elem_ary_of_ary(self.obj_array_val);
    end if;
    return;
  end;

  static function class_func(n number) return rb_test_obj is
  begin
    return rb_test_obj(n);
  end;

  static procedure class_proc1(obj out rb_test_obj, n number) is
  begin
    obj := rb_test_obj(n);
  end;

  static procedure class_proc2(obj in out rb_test_obj) is
  begin
    obj.int_val := obj.int_val + 1;
  end;

  member function member_func return integer is
  begin
    return int_val;
  end;

  member procedure member_proc(n in integer) is
  begin
    self.int_val := n;
  end;
end;
/
create table rb_test_obj_tab1 (
  n number primary key,
  obj rb_test_obj
)
/
create table rb_test_obj_tab2 of rb_test_obj (
  int_val primary key
)
/
declare
  n number := 0;
  obj rb_test_obj;
begin
  loop
    n := n + 1.2;
    exit when n > 20;
    obj := rb_test_obj(n);
    insert into rb_test_obj_tab1 values (n, obj);
    insert into rb_test_obj_tab2 values (obj);
  end loop;
end;
/
commit
/
