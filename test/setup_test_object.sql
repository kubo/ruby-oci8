drop table rb_test_obj_tab1 purge
/
drop table rb_test_obj_tab2 purge
/
drop type rb_test_obj
/
create or replace type rb_test_str_array as array(50) of varchar2(50)
/
create or replace type rb_test_num_array as array(50) of number(10,1)
/
create type rb_test_obj as object (
  integer_val integer,
  float_val float,
  string_val varchar2(50),
  str_array_val rb_test_str_array,
  num_array_val rb_test_num_array,

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
    self.integer_val := n;
    self.float_val := n;
    self.string_val := to_char(n);
    if self.integer_val != 1 then
      self.str_array_val := rb_test_str_array(to_char(n), to_char(n + 1), to_char(n + 2));
      self.num_array_val := rb_test_num_array(n, n + 1, n + 2);
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
    obj.integer_val := obj.integer_val + 1;
  end;

  member function member_func return integer is
  begin
    return integer_val;
  end;

  member procedure member_proc(n in integer) is
  begin
    self.integer_val := n;
  end;
end;
/
create table rb_test_obj_tab1 (
  n number primary key,
  obj rb_test_obj
)
/
create table rb_test_obj_tab2 of rb_test_obj (
  integer_val primary key
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
