# @title Platform Specific Issues

Platform Specific Issues
========================

Linux
-----

### Ubuntu 11.10 (Oneiric Ocelot)

If the following error occurs even though libc6-dev is installed,

    Do you install glibc-devel(redhat) or libc6-dev(debian)?
    You need /usr/include/sys/types.h to compile ruby-oci8.

You need to use ruby-oci8 2.1.0 or upper. Otherwise, run the following command and re-install ruby-oci8.

    $ sudo ln -s /usr/include/linux/ /usr/include/sys

### General Linux

Use the same bit-width of libraries with ruby. For example, x86\_64
instant client for x86\_64 ruby and 32-bit instant client for 32-bit
ruby. It depends on the ruby but not on the OS. As for full client,
x86\_64 ruby cannot use with 32-bit full client, but 32-bit ruby can
use with 64-bit full client because 32-bit libraries are in
$ORACLE\_HOME/lib32.

To check which type of ruby do you use:

    file `which ruby`

Note: "`" is a back quote.

Mac OS X
--------

### OS X 10.7+

Use the latest 64-bit instant client. The older 64-bit (10.2.0.4) instant client [doesn't work](https://forums.oracle.com/forums/thread.jspa?threadID=2187558). The older 32-bit instant client will work but only with 32-bit ruby or jruby (using JDBC).

### Intel Mac (64-bit)

See [How to setup Ruby and Oracle Instant Client on Snow Leopard](http://blog.rayapps.com/2009/09/06/how-to-setup-ruby-and-oracle-instant-client-on-snow-leopard/).
Note that setting the environment variable RC\_ARCHS=x86\_64 instead of ARCHFLAGS="-arch x86\_64" will work fine also.

### Intel Mac (32-bit)

See [How to setup Ruby and Oracle Instant Client on Snow Leopard](http://blog.rayapps.com/2009/09/06/how-to-setup-ruby-and-oracle-instant-client-on-snow-leopard/). Note that you need to replace x86\_64 with i386 in the blog.

The Intel Instant client is for Mac OS X 10.5 Leopard. If you are using 10.4 Tiger,
use one of the following workarounds.

* compile ruby as ppc. (look at [How to setup Ruby and Oracle client on Intel Mac](http://blog.rayapps.com/2007/08/27/how-to-setup-ruby-and-oracle-client-on-intel-mac/))
* use [ruby-odbc](http://www.ch-werner.de/rubyodbc/) and a third party ODBC driver ([Actual Technologies](http://www.actualtechnologies.com) or [OpenLink Software](http://uda.openlinksw.com/)).
* use JRuby and Oracle JDBC driver.

### PowerPC Mac

See [How to setup Ruby and Oracle Instant Client on Snow Leopard](http://blog.rayapps.com/2009/09/06/how-to-setup-ruby-and-oracle-instant-client-on-snow-leopard/). Note that you need to replace x86\_64 with ppc in the blog.

Solaris
-------

You need a same compiler which is used to make ruby itself.
For example, if the ruby is compiled by gcc, you need gcc. If it is compiled by Oracle Solaris Studio
(formerly called as Sun Studio), you need Oracle Solaris Studio.

If you use Blastwave.org's ruby and want not to install Sun Studio,
you can edit rbconfig.rb by your self. [(look at here)](http://forum.textdrive.com/viewtopic.php?id=12630)

If you use Sunfreeware.com's ruby and 

    $ ruby -r rbconfig -e "p Config::CONFIG['GNU_LD']"

prints "yes", you may need to edit rbconfig.rb distributed with the ruby
as follows:

    from: CONFIG["LDFLAGS"] = "-L.  -Wl,-E"
    to:   CONFIG["LDFLAGS"] = "-L. "

FreeBSD
-------

Oracle8-client port isn't available because it is too old.
You need to use linux emulator.

I have not run ruby-oci8 on linux emulator, but I guess it will
run as follows. If it works, please tell me.

If FreeBSD has a cross-compiler which can generate linux binaries,
use it to compile ruby and ruby-oci8.

If FreeBSD doesn't have such a compiler, install a linux distribution
which FreeBSD can emulate to a PC machine, make ruby and ruby-oci8 in
the linux box and copy them to the FreeBSD box as follows.

On linux:

    # make ruby
    tar xvfz ruby-1.8.x.tar.gz
    cd ruby-1.8.x
    ./configure --prefix=/usr/local/ruby-1.8.x --enable-pthread
    make
    make install
    # setup instant client
      ....
    # make ruby-oci8
    PATH=/usr/local/ruby-1.8.x/bin:$PATH
    tar xvfz ruby-oci8-1.0.x.tar.gz
    cd ruby-oci8-1.0.x
    make
    make install
    cd /usr/local/
    tar cvfz ruby-1.8.x-linux.tar.gz ruby-1.8.x
 
Copy ruby-1.8.x-linux.tar.gz to the FreeBSD box.
 
On freebsd:

    cd /compat/linux/usr/local
    tar xvfz ruby-1.8.x-linux.tar.gz

HP-UX
-----

You need a ruby which is linked with ''libpthread'' and ''libcl''.

Run `configure`.

    tar xvfz ruby-1.9.x.tar.gz
    cd ruby-1.9.x
    ./configure

Then open `Makefile` to add '-lpthread -lcl' to `LIBS` and run `make`
and `make install`.

    make
    make install

The rest steps are described at {file:docs/install-full-client.md}.

Windows
-------

On some machines using a slow disk, you may get the following error.

    Permission denied - conftest.exe

Edit mkmf.rb for a workaround.

    def try_run(src, opt="")
      if try_link0(src, opt)
        xsystem("./conftest")
      else
        nil
      end
    ensure
      # add the following one line.
      sleep 1 if /mswin32|cygwin|mingw32|bccwin32/ =~ RUBY_PLATFORM
      rm_f "conftest*"
    end

See also: {http://abstractplain.net/blog/?p=788}.
