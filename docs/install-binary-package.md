# @title Install Binary Package

Windows 32-bit (mingw32 ruby)
=============================

Run the following command:

    gem install ruby-oci8

Windows 32-bit (mswin32 ruby)
=============================

Run the following command:

    gem install --platform x86-mingw32 ruby-oci8

Ruby-oci8 gem for x86-mingw32 works on mswin32 ruby.
If it doesn't work, see {file:docs/install-instant-client.md} or {file:docs/install-full-client.md}.

Other platforms
===============

We doesn't make binary gems for other platforms.
If you need to distribute a binary gem, compile ruby-oci8 and run the following command.

    gem build ruby-oci8.gemspec -- current

Note that you need to compile it on the oldest versions of the platform you support
and for the oldest versions of the Oracle databases.

If you could not fix the Oracle versions, compile ruby-oci8 and make a gem as follows:

    gzip -dc ruby-oci8-VERSION.tar.gz | tar xvf -
    cd ruby-oci8-VERSION
    ruby setup.rb config -- --with-runtime-check
    make
    gem build ruby-oci8.gemspec -- current

When the option `--with-runtime-check` is set, ruby-oci8 checks the Oracle version
and available OCI functions at runtime.
