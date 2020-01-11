Before running unit test:

1. Connect to Oracle as sys
   ```shell
   $ sqlplus sys/<password_of_sys> as sysdba
   SQL>
   ```
2. Create user ruby
   ```sql
   SQL> CREATE USER ruby IDENTIFIED BY oci8 DEFAULT TABLESPACE users TEMPORARY TABLESPACE temp;
   SQL> alter user ruby quota unlimited on users;
   ```
3. Grant the privilege to connect and execute.
   ```sql
    SQL> GRANT connect, resource, create view, create synonym TO ruby;
    SQL> GRANT execute ON dbms_lock TO ruby;
    ```
4. Connect as ruby user.
   ```shell
   $ sqlplus ruby/oci8
   SQL>
   ```
5. Create object types
   ```sql
   SQL> @test/setup_test_object.sql
   SQL> @test/setup_test_package.sql
   ```
6. change $dbname in test/config.rb.

Then run the following command:
```shell
$ make check
```
or
```
$ nmake check   (If your compiler is MS Visual C++.)
````
