What is ruby-oci8
=================

Ruby-oci8 is a ruby interface for Oracle Database. The latest version
is available for all Oracle versions after Oracle 10g including Oracle
Instant Client.

Use ruby-oci8 2.0.6 for Oracle 8 or use ruby-oci8 2.1.8 for Oracle 9i.

Note that ruby 1.8 support was dropped in ruby-oci8 2.2.0.
Use ruby-oci8 2.1.8 for ruby 1.8.

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

Other documents
===============

* {file:docs/timeout-parameters.md Timeout Parameters}
* {file:docs/conflicts-local-connections-and-processes.md Conflicts between Local Connections and Child Process Handling on Unix}
* {file:docs/bind-array-to-in_cond.md Bind an Array to IN-condition}

License
=======

* {file:COPYING 2-clause BSD-style license} from ruby-oci8 2.1.3
* {file:COPYING_old old Ruby license} until 2.1.2
