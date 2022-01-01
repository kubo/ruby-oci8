CREATE USER ruby IDENTIFIED BY oci8 DEFAULT TABLESPACE users TEMPORARY TABLESPACE temp;
GRANT connect, resource, create view, create synonym, unlimited tablespace TO ruby;
GRANT EXECUTE ON dbms_lock TO ruby;

CONNECT ruby/oci8
@test/setup_test_object.sql
@test/setup_test_package.sql
