# @title Install ruby-oci8 on macOS

Install ruby-oci8 on macOS
=========================

Prerequisite
------------

* Command line tools for Xcode or Xcode (by executing `xcode-select --install`) or [Xcode]


Download Oracle Instant Client Packages
---------------------------------------

Go [oracle site](https://www.oracle.com/database/technologies/instant-client/macos-arm64-downloads.html) and download:

* instantclient-basiclite-macos.arm64-23.3.0.23.09.dmg
* instantclient-sdk-macos.arm64-23.3.0.23.09.dmg
* instantclient-sqlplus-macos.arm64-23.3.0.23.09.dmg

Mount DMG package and prepare folder
------------------------------------

```bash
hdiutil mount ~/Downloads/instantclient-basiclite-macos.arm64-23.3.0.23.09.dmg
cd /Volumes/instantclient-basiclite-macos.arm64-23.3.0.23.09
sh ./install_ic.sh
```

```bash
hdiutil mount ~/Downloads/instantclient-sdk-macos.arm64-23.3.0.23.09.dmg
cd /Volumes/instantclient-sdk-macos.arm64-23.3.0.23.09
sh ./install_ic.sh
```

```bash
hdiutil mount ~/Downloads/instantclient-sqlplus-macos.arm64-23.3.0.23.09.dmg
cd /Volumes/instantclient-sqlplus-macos.arm64-23.3.0.23.09
sh ./install_ic.sh
```

```bash
sudo mv ~/Downloads/instantclient_23_3 /opt
```

Install ruby-oci8
-----------------

Note that `/usr/bin/ruby` isn't available. You need to use [`rbenv`] or so.

```bash
cd /usr/local/bin
sudo ln -s /opt/instantclient_23_3/sqlplus sqlplus
export OCI_DIR=/opt/instantclient_23_3
gem install ruby-oci8
```


Put tnsnames.ora
----------------

```bash
export TNS_ADMIN=/opt/instantclient_23_3/network/admin/
export NLS_LANG=AMERICAN_AMERICA.AL32UTF8 # avoid warning, optional
```

[Xcode]: https://apps.apple.com/us/app/xcode/id497799835
[`rbenv`]: https://github.com/rbenv/rbenv
