# @title Install ruby-oci8 on macOS

Install ruby-oci8 on macOS
=========================

**Note: Ruby-oci8 doesn't run on Apple Silicon because Oracle instant client
for Apple Silicon has not been released yet.**

Prerequisite
------------

* Command line tools for Xcode or Xcode (by executing `xcode-select --install`) or [Xcode]

Install Oracle Instant Client Packages
--------------------------------------

If you have installed [Homebrew], use the following command:

```shell
$ brew tap InstantClientTap/instantclient
$ brew install instantclient-basic   # or instantclient-basiclite
$ brew install instantclient-sdk
$ brew install instantclient-sqlplus # (optionally)
```

Otherwise, look at this [page][OTN] and set the environment variable
`OCI_DIR` to point the the directory where instant client is installed.
Ruby-oci8 installation script checks the directory.

```shell
export OCI_DIR=$HOME/Downloads/instantclient_19_8  # for example
```

Install ruby-oci8
-----------------

Note that `/usr/bin/ruby` isn't available. You need to use [`rbenv`] or so.

```shell
$ gem install ruby-oci8
```

[Homebrew]: http://brew.sh/
[OTN]: https://www.oracle.com/database/technologies/instant-client/macos-intel-x86-downloads.html#ic_osx_inst
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835
[`rbenv`]: https://github.com/rbenv/rbenv
