# @title Report Installation Issues

Report Installation Issues
==========================

Look at {file:docs/platform-specific-issues.md} and [the issues page on github][github] to check whether your issue is fixed or not.

If it is a new one, post the following information to [github][].

[github]: https://github.com/kubo/ruby-oci8/issues

*    Messages printed out to the console

*    `gem_make.out` if you install a gem

*    Last 100 lines of 'ext/oci8/mkmf.log'

     Get them as follows.

         tail -100 ext/oci8/mkmf.log

*    The results of the following commands:

         file `which ruby`
         ruby --version
         ruby -r rbconfig -e "p RbConfig::CONFIG['host']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['CC']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['CFLAGS']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['LDSHARED']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['LDFLAGS']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['DLDLAGS']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['LIBS']"
         ruby -r rbconfig -e "p RbConfig::CONFIG['GNU_LD']"
         
         # if you use gcc,
         gcc --print-prog-name=ld
         gcc --print-prog-name=as
         
         # Oracle full client
         file $ORACLE_HOME/bin/oracle
         
         # Oracle Instant client. You need to change INSTANT_CLIENT_DIRECTORY.
         file INSTANT_CLIENT_DIRECTORY/libclntsh.*
         
         echo $LD_LIBRARY_PATH
         echo $LIBPATH              # AIX
         echo $SHLIB_PATH           # HP-UX PA-RISC 32-bit ruby
         echo $DYLD_LIBRARY_PATH    # Mac OS X
         echo $LD_LIBRARY_PATH_32   # Solaris 32-bit ruby
         echo $LD_LIBRARY_PATH_64   # Solaris 64-bit ruby
