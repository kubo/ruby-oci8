# @title Install for Oracle Instant Client

Install for Oracle Instant Client
=================================

Introduction
------------

This page explains the way to install ruby-oci8 for Oracle Instant Client.

Look at {file:docs/install-full-client.md} for Oracle Full Client.

Look at {file:docs/install-binary-package.md} for Windows unless you
have a special need to compile ruby-oci8 by yourself.

Look at {file:docs/install-on-osx.md} for OS X.

Install Oracle Instant Client Packages
--------------------------------------

### Donwload Instant Client Packages

Download the following packages from [Oracle Technology Network](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html).

* Instant Client Package - Basic or Basic Lite
* Instant Client Package - SDK
* Instant Client Package - SQL*Plus

Note: use 32-bit packages for 32-bit ruby even though the OS is 64-bit.

### UNIX zip packages

Unzip the packages as follows:

    mkdir /opt
    mkdir /opt/oracle
    cd /opt/oracle
    unzip path/to/instantclient-basic-OS-VERSION.zip
    unzip path/to/instantclient-sdk-OS-VERSION.zip
    unzip path/to/instantclient-sqlplus-OS-VERSION.zip

If /opt/oracle/instantclient12_1/libclntsh.so is not found, make
a symbolic link to link the library.

    cd /opt/oracle/instantclient12_1
    ln -s libclntsh.so.12.1 libclntsh.so

Note:

* use libclntsh.sl instead of libclntsh.so on HP-UX PA-RISC.
* skip this step for AIX.
* run `yum install libaio` also on Redhat.
* run `apt-get install libaio1` also on Ubuntu.

Set the library search path, whose name depends on the OS, to point to
the installed directory.

<table style="border: 1px #E3E3E3 solid; border-collapse: collapse; border-spacing: 0;">
<thead>
  <tr><th> OS                        </th><th> library search path                   </th></tr>
</thead>
<tbody>
  <tr><td> Linux                     </td><td> LD_LIBRARY_PATH                       </td></tr>
  <tr><td> Solaris 32-bit ruby       </td><td> LD_LIBRARY_PATH_32 or LD_LIBRARY_PATH </td></tr>
  <tr><td> Solaris 64-bit ruby       </td><td> LD_LIBRARY_PATH_64 or LD_LIBRARY_PATH </td></tr>
  <tr><td> HP-UX PA-RISC 32-bit ruby </td><td> SHLIB_PATH                            </td></tr>
  <tr><td> HP-UX PA-RISC 64-bit ruby </td><td> LD_LIBRARY_PATH                       </td></tr>
  <tr><td> HP-UX IA64                </td><td> LD_LIBRARY_PATH                       </td></tr>
  <tr><td> AIX                       </td><td> LIBPATH                               </td></tr>
</tbody>
</table>
Note: Look at {file:docs/install-on-osx.md} for OS X.

For example:

    $ LD_LIBRARY_PATH=/opt/oracle/instantclient_12_1
    $ export LD_LIBRARY_PATH

Though many instant client pages indicate that the environment varialbe
`ORACLE_HOME` should be set, it isn't necessary unless Oracle Net
configuration files such as `tnsnames.ora` and `sqlnet.ora` are in
`$ORACLE_HOME/network/admin/`.

### Linux rpm packages

Install the downloaded packages as follows:

    rpm -i path/to/oracle-instantclient-basic-VERSION-ARCH.rpm
    rpm -i path/to/oracle-instantclient-devel-VERSION-ARCH.rpm
    rpm -i path/to/oracle-instantclient-sqlplus-VERSION-ARCH.rpm

Set LD_LIBRARY_PATH to point to the directory where libclntsh.so is installed.

For example:

    $ LD_LIBRARY_PATH=/usr/lib/oracle/12.1/client/lib
    $ export LD_LIBRARY_PATH

### Windows

Unzip the packages and set `PATH` to point to the directory where `OCI.DLL` is installed.

If `require "ruby-oci8"` raises a load error such as "OCI.DLL: 126(The
specified module could not be found. )", either `OCI.DLL` or a DLL depended
by `OCI.DLL` could not be found in `PATH`.

If `OCI.DLL` is surely in `PATH`, the missing DLL is a Visual C++ runtime
library in most cases. You need to install "Microsoft Visual C++ Redistributable
Package" or copy the runtime library to the directory where `ruby.exe` resides.

| Oracle Version | Package | Runtime Library|
|---|---|---|
| 12.1.0.x | [Microsoft Visual C++ 2010 Redistributable Package][2010] | MSVCR100.DLL |
| 11.2.0.x | Microsoft Visual C++ 2005 SP1 Redistributable Package ([x86][2005SP1_x86], [x64][2005SP1_x64]) | MSVCR80.DLL(The file version must be 8.0.50727.762.) |
| 11.1.0.x | [No separate redistributable package][2003] | MSVCR71.DLL |
| 10.2.0.x | [No separate redistributable package][2003] | MSVCR71.DLL |

[2010]: http://www.microsoft.com/en-us/download/details.aspx?id=26999
[2005SP1_x86]: https://www.microsoft.com/en-us/download/details.aspx?id=5638
[2005SP1_x64]: https://www.microsoft.com/en-us/download/details.aspx?id=18471
[2003]: http://stackoverflow.com/questions/1596167/where-to-download-microsoft-visual-c-2003-redistributable#6132093

Check the environment
---------------------

### Oracle installation

Run the following command and confirm it works fine. If it doesn't
work well, check `LD_LIBRARY_PATH` or PATH.

    sqlplus USERNAME/PASSWORD

### ruby installation

Run the following command. If it ends with "can't find header files
for ruby" or "ruby: no such file to load -- mkmf (LoadError)", you need
to install ruby-devel(redhat) or ruby-dev(debian/ubuntu).

    ruby -r mkmf -e ""

### development tools

You need a C compiler and development tools such as make or nmake.
Note that they must be same with ones used to compile the ruby.
For example, you need Oracle Solaris Studio, not gcc, for ruby
compiled by Oracle Solaris Studio.

Installation
------------

If you get a problem in the following steps, look at {file:docs/platform-specific-issues.md}
and {file:docs/report-installation-issue.md}.

### gem package

Run the following command.

    gem install ruby-oci8

If you get a problem, look at {file:docs/platform-specific-issues.md}
and {file:docs/report-installation-issue.md}.

### tar.gz package

#### Download the source code

Download the latest tar.gz package from [download page][].

Note: if you are using Windows and you have no special need to compile
it by yourself, look at {file:docs/install-binary-packge.md}.

#### Run make and install

##### UNIX or Windows(mingw32, cygwin)

    gzip -dc ruby-oci8-VERSION.tar.gz | tar xvf -
    cd ruby-oci8-VERSION
    make
    make install

note: If you use '`sudo`', use it only when running '`make install`'.
'`sudo`' doesn't pass library search path to the executing command for security reasons.

##### Windows(mswin32)

    gzip -dc ruby-oci8-VERSION.tar.gz | tar xvf -
    cd ruby-oci8-VERSION
    nmake
    nmake install

[download page]: https://bintray.com/kubo/generic/ruby-oci8
