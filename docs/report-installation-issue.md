# @title Report Installation Issues

Look at {file:docs/platform-specific-issues.md},
[the issues page on github][github] and [the ruby-oci8 help forum on rubyforge][rubyforge]
to check whether your issue is fixed or not.

If it is a new one, post the following information to [github][] or [rubyforge][].

[github]: https://github.com/kubo/ruby-oci8/issues
[rubyforge]: http://rubyforge.org/forum/forum.php?forum_id=1078

*    Messages printed out to the console

*    `gem_make.out` if you install a gem

*    Last 100 lines of 'ext/oci8/mkmf.log'

     Get them as follows.

         tail -100 ext/oci8/mkmf.log

*    The results of the following commands:

         file `which ruby`
         ruby --version
         ruby -r rbconfig -e "p Config::CONFIG['host']"
         ruby -r rbconfig -e "p Config::CONFIG['CC']"
         ruby -r rbconfig -e "p Config::CONFIG['CFLAGS']"
         ruby -r rbconfig -e "p Config::CONFIG['LDSHARED']"
         ruby -r rbconfig -e "p Config::CONFIG['LDFLAGS']"
         ruby -r rbconfig -e "p Config::CONFIG['DLDLAGS']"
         ruby -r rbconfig -e "p Config::CONFIG['LIBS']"
         ruby -r rbconfig -e "p Config::CONFIG['GNU_LD']"
         
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
