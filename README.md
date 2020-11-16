Ruby-oci8
=========

[![Gem Version](https://badge.fury.io/rb/ruby-oci8.svg)](http://badge.fury.io/rb/ruby-oci8)
[![Build Status](https://travis-ci.org/kubo/ruby-oci8.svg?branch=master)](https://travis-ci.org/kubo/ruby-oci8)

What is ruby-oci8
-----------------

Ruby-oci8 is a ruby interface for Oracle Database. The latest version
is available for all Oracle versions after Oracle 10g including Oracle
Instant Client.

Use ruby-oci8 2.0.6 for Oracle 8 or use ruby-oci8 2.1.8 for Oracle 9i.

Note that ruby 1.8 support was dropped in ruby-oci8 2.2.0.
Use ruby-oci8 2.1.8 for ruby 1.8.

What's new
----------

See [NEWS](NEWS).

Sample one-liner
----------------

When you have an Oracle database server to which `sqlplus scott/tiger` can connect
and `scott` user has `emp` table, you can select `emp` and print rows
as CSV by the followig one liner.

    ruby -r oci8 -e "OCI8.new('scott', 'tiger').exec('select * from emp') do |r| puts r.join(','); end"

Homepage
--------

* http://www.rubydoc.info/github/kubo/ruby-oci8

Installation
------------

* [Install for Oracle Full Client](docs/install-full-client.md)
* [Install for Oracle Instant Client](docs/install-instant-client.md)
* [Install Binary Package](docs/install-binary-package.md)
* [Install on OS X](docs/install-on-osx.md)

Report issues
-------------

* [Report Installation Issues](docs/report-installation-issue.md)
* [The issues page on github](https://github.com/kubo/ruby-oci8/issues)

Other documents
---------------

* [Number Type Mapping between Oracle and Ruby](docs/number-type-mapping.md)
* [Timeout Parameters](docs/timeout-parameters.md)
* [Conflicts between Local Connections and Child Process Handling on Unix](docs/conflicts-local-connections-and-processes.md)
* [Hanging After a Long Period of Inactivity](docs/hanging-after-inactivity.md)
* [Bind an Array to IN-condition](docs/bind-array-to-in_cond.md)
* [LDAP Authentication and Function Interposition](docs/ldap-auth-and-function-interposition.md)

License
-------

* [2-clause BSD-style license from ruby-oci8 2.1.3](COPYING)
* [old Ruby license until 2.1.2](COPYING_old)
