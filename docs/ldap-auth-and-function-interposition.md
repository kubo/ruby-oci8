# @title LDAP Authentication and Function Interposition

LDAP Authentication and Function Interposition
==============================================

Situation
---------

The following code may trigger segmentation faults or unexpected behaviours.

    require 'pg'
    require 'oci8'
    
    conn = OCI8.new('username/password@dbname.example.com')
    ...

It happens when all the following conditions are satisfied.

* The platform is Unix
* The PostgreSQL client library, which `pg` depends, was compiled with LDAP support.
* LDAP authentication is used to connect to an Oracle server.

It is caused by function interposition as follows:

* The ruby process loads `pq` and its depending libraries such as
  `libpq.so`(PostgreSQL client library) and `libldap_r.so`(LDAP library).
* Then it loads `oci8` and its depending libraries such as
  `libclntsh.so`(Oracle client library).
* When LDAP authentication is used, `libclntsh.so` tries to use
  LDAP functions in the library.
* However it uses LDAP functions in `libldap_r.so` because the function
  in the firstly loaded library is used when more than one library exports
  functions whose names are same.
* It triggers segmentation faults or unexpected behaviours because
  implementations of LDAP functions are different even though their names
  are same.

The reverse also causes same result by the following code.

    require 'oci8'
    require 'pg'
    
    ... connect to PostgreSQL using LDAP ...

Note for macOS: Libraries in two-level namespaces are free from function
interpositions. See the second paragraph of [this document][mach-o]. The LDAP
library depended by `pg` may or may not be in a two-level namespace. On the
other hand, Oracle client libraries are in a single-level (flat) namespace.

Solution 1
----------

If you don't connect to PostgreSQL using LDAP, use the following code.

    require 'oci8'  # This must be before "require 'pg'".
    require 'pg'
    
    conn = OCI8.new('username/password@dbname.example.com')
    ...
    ... connect to a PostgreSQL server ...

Oracle client library uses LDAP functions in `libclntsh.so` because `libclntsh.so`
is loaded before `libldap_r.so`.

Don't connect to PostgreSQL using LDAP because `libpq.so` tries to use
LDAP functions in `libldap_r.so` but faultily uses functions in `libclntsh.so`.

Note for macOS: This fixes all function interpositions issues if the LDAP library
in a two-level namespace.

Solution 2
----------

If LDAP is used to connect to both Oracle and PostgreSQL and the platform
is Linux or macOS, use the latest code in [github][], which will be included
in the next ruby-oci8 release, and use the following code.

    require 'pg'
    require 'oci8' # This must be after "require 'pg'".
    
    conn = OCI8.new('username/password@dbname.example.com')
    ...
    ... connect to a PostgreSQL server using LDAP ...

PostgreSQL client library uses LDAP functions in `libldap_r.so` because `libldap_r.so`
is loaded before `libclntsh.so`.

Oracle client library uses LDAP functions in `libclntsh.so` because ruby-oci8 in
[github][] forcedly modifies Procedure Linkage Table (PLT) entries to point to
functions in `libclntsh.so` if they point to functions in other libraries.

[github]: https://github.com/kubo/ruby-oci8/
[mach-o]: https://developer.apple.com/library/content/documentation/DeveloperTools/Conceptual/MachOTopics/1-Articles/executing_files.html
