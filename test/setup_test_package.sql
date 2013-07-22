create or replace package rb_test_pkg is
  package_version pls_integer := 1;

  type array_of_integer is array(50) of integer;
  type table_of_pls_integer is table of pls_integer;
  type table_of_boolean is table of boolean;
  type indexed_table_of_varchar2 is table of varchar2(10) index by varchar2(5);
  type rec1 is record (i pls_integer, j integer);
  type rec2 is record (b boolean, it indexed_table_of_varchar2, rec rec1);
  type table_of_rec1 is table of rec1;
  type table_of_rec2 is table of rec2;

  function sum_table_of_pls_integer(tbl in table_of_pls_integer) return pls_integer;
  function add_rec1_values(tbl in table_of_rec1) return pls_integer;
  procedure out_rec1_values(tbl out table_of_rec1);
end;
/
create or replace package body rb_test_pkg is
  function sum_table_of_pls_integer(tbl in table_of_pls_integer) return pls_integer is
    i pls_integer;
    ret pls_integer := 0;
  begin
    for i in tbl.first..tbl.last loop
      ret := ret + tbl(i);
    end loop;
    return ret;
  end;

  function add_rec1_values(tbl in table_of_rec1) return pls_integer is
    i pls_integer;
    ret pls_integer := 0;
  begin
    for i in tbl.first..tbl.last loop
      ret := ret + nvl(tbl(i).i, 0) + nvl(tbl(i).j, 0);
    end loop;
    return ret;
  end;

  procedure out_rec1_values(tbl out table_of_rec1) is
    i pls_integer;
    rec1val rec1;
  begin
    tbl := table_of_rec1();
    for i in 1..20 loop
      tbl.extend;
      if i mod 6 != 0 then
        if i mod 3 != 0 then
          rec1val.i := i;
          rec1val.j := i;
        else
          rec1val.i := null;
          rec1val.j := null;
        end if;
        tbl(i) := rec1val;
      end if;
    end loop;
  end;
end;
/
