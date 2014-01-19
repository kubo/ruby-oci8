# @title Install for Oracle Full Client

Introduction
============

This page explains the way to install ruby-oci8 for Oracle Full Client
installations.

For Oracle Instant Client, look at {file:docs/install-instant-client.md}.
For Windows, look at {file:docs/install-binary-package.md} unless you
have a special need to compile ruby-oci8 by yourself.

Check the environment
=====================

Oracle installation
-------------------

Run the following command and confirm it works fine. If it doesn't
work well, you need to ask to your database administrator.

    sqlplus USERNAME/PASSWORD

ruby installation
-----------------

Run the following command. If it ends with "can't find header files
for ruby" or "ruby: no such file to load -- mkmf (LoadError)", you need
to install ruby-devel(redhat) or ruby-dev(debian/ubuntu).

    ruby -r mkmf -e ""

development tools
-----------------

You need a C compiler and development tools such as make or nmake.
Note that they must be same with ones used to compile the ruby.
For example, you need Oracle Solaris Studio, not gcc, for ruby
compiled by Oracle Solaris Studio.

Installation
============

If you get a problem in the following steps, look at {file:docs/platform-specific-issues.md}
and {file:docs/report-installation-issue.md}.

Set the library search path
---------------------------

### UNIX

Set the library search path, whose name depends on the OS, to point to
$ORACLE\_HOME/lib. If the database is 64-bit and the ruby is 32-bit,
use $ORACLE\_HOME/lib32 instead.

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
  <tr><td> Mac OS X                  </td><td> DYLD_LIBRARY_PATH                     </td></tr>
  <tr><td> AIX                       </td><td> LIBPATH                               </td></tr>
</tbody>
</table>

Do not forget to export the variable as follows:

    $ LD_LIBRARY_PATH=$ORACLE_HOME/lib
    $ export LD_LIBRARY_PATH

### Windows(mswin32, mingw32, cygwin)

If sqlplus runs correctly, library search path has no problem.

gem package
-----------

Run the following command.

    gem install ruby-oci8

tar.gz package
--------------

### Download the source code

Download the latest tar.gz package from [download page][].

### Run make and install

#### UNIX or Windows(mingw32, cygwin)

    gzip -dc ruby-oci8-VERSION.tar.gz | tar xvf -
    cd ruby-oci8-VERSION
    make
    make install

note: If you use '`sudo`', use it only when running '`make install`'.
'`sudo`' doesn't pass library search path to the executing command for security reasons.

#### Windows(mswin32)

    gzip -dc ruby-oci8-VERSION.tar.gz | tar xvf -
    cd ruby-oci8-VERSION
    nmake
    nmake install

[download page]: https://bintray.com/kubo/generic/ruby-oci8
