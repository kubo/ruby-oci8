# @title LDAP Authentication and Function Interposition

LDAP Authentication and Function Interposition
==============================================

Problems
--------

The following code may trigger segmentation faults or unexpected behaviours.

    require 'pg' # or any modules using LDAP such as ActiveLdap
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

The reverse may cause same results by the following code.

    require 'oci8'
    require 'pg'
    
    ... connect to PostgreSQL using LDAP ...

### Note for macOS

Libraries in two-level namespaces are free from function interpositions on macOS.
See the second paragraph of [this document][mach-o]. If `TWOLEVEL` is
found in the output of `otool -hV /path/to/library`, it is in a
two-level namespace. Otherwise it is in a single-level (flat) namespace.

Oracle client library (`libclntsh.dylib.12.1`) is in a flat namespace.
It suffers from function interpositions.

    $ otool -hV libclntsh.dylib.12.1
    Mach header
          magic cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
    MH_MAGIC_64  X86_64        ALL  0x00       DYLIB    19       2360 DYLDLINK NO_REEXPORTED_DYLIBS MH_HAS_TLV_DESCRIPTORS

PostgreSQL client library (`libpq.5.dylib`) installed by [brew][] depends on an OS-supplied LDAP library.

    $ otool -L libpq.5.dylib 
    libpq.5.dylib:
        /usr/local/opt/postgresql/lib/libpq.5.dylib (compatibility version 5.0.0, current version 5.9.0)
        /usr/local/opt/openssl/lib/libssl.1.0.0.dylib (compatibility version 1.0.0, current version 1.0.0)
        /usr/local/opt/openssl/lib/libcrypto.1.0.0.dylib (compatibility version 1.0.0, current version 1.0.0)
        /System/Library/Frameworks/Kerberos.framework/Versions/A/Kerberos (compatibility version 5.0.0, current version 6.0.0)
        /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP (compatibility version 1.0.0, current version 2.4.0)
        /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1238.0.0)

The OS-supplied LDAP library is in a two-level namespace.

    $ otool -hV /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP
    Mach header
          magic cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
    MH_MAGIC_64  X86_64        ALL  0x00       DYLIB    22       2528   NOUNDEFS DYLDLINK TWOLEVEL NO_REEXPORTED_DYLIBS APP_EXTENSION_SAFE

As a result, the PostgreSQL client library is free from function interpositions.

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
[github][] forcedly modifies PLT (Procedure Linkage Table) entries to point to
functions in `libclntsh.so` if they point to functions in other libraries.
(PLT is equivalent to IAT (Import Address Table) on Windows.)

[github]: https://github.com/kubo/ruby-oci8/
[mach-o]: https://developer.apple.com/library/content/documentation/DeveloperTools/Conceptual/MachOTopics/1-Articles/executing_files.html
[brew]: http://brew.sh/
