# @title Install Binary Package

Install Binary Package
======================

Windows
-------

You need to install Oracle client in advance.
If you don't have installed Oracle client, install {file:docs/install-instant-client.md#Install_Oracle_Instant_Client_Packages instant client}. Only the Basic or Basic Lite package is needed to use ruby-oci8.
However it is better to install SQL*Plus also because it is usable to check whether
a problem is in Oracle setting or in ruby-oci8.

Run the following command to install ruby-oci8.

    gem install ruby-oci8

If you uses mswin32 ruby, use the following command instead.

    gem install --platform x86-mingw32 ruby-oci8
      or
    gem install --platform x64-mingw32 ruby-oci8

Other platforms
---------------

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
