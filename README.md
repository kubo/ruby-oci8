What is ruby-oci8
=================

Ruby-oci8 is a ruby interface for Oracle Database. The latest version
is available for all Oracle versions after Oracle 10g including Oracle
Instant Client.

Oracle Support Versions

| Oracle Version | ruby-oci8 Version | Notes |
|----------------|-------------------|------|
| Oracle 8       | ruby-oci8 2.0.6   | |
| Oracle 9i      | ruby-oci8 2.1.8   | |
| Oracle 11      | ruby-oci8 2.2.0.2 | Drops support for ruby 1.8 |
| Oracle 12      | ruby-oci8 2.2.1   | Drops support for Oracle 11g Client/Server |


What's new
==========

See {file:NEWS}.

Sample one-liner
================

Connect to scott/tiger, select `emp` and print as CSV format.

    ruby -r oci8 -e "OCI8.new('scott', 'tiger').exec('select * from emp') do |r| puts r.join(','); end"

Homepage
========

* http://www.rubydoc.info/github/kubo/ruby-oci8

Installation
============

* {file:docs/install-full-client.md Install for Oracle Full Client}
* {file:docs/install-instant-client.md Install for Oracle Instant Client}
* {file:docs/install-binary-package.md Install Binary Package}
* {file:docs/install-on-osx.md Install on OS X}

Report issues
=============

* {file:docs/report-installation-issue.md Report Installation Issues}
* [The issues page on github](https://github.com/kubo/ruby-oci8/issues)

License
=======

* {file:COPYING 2-clause BSD-style license} from ruby-oci8 2.1.3
* {file:COPYING_old old Ruby license} until 2.1.2
