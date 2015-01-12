What is ruby-oci8
=================

Ruby-oci8 is a ruby interface for Oracle Database. The latest version
is available for all Oracle versions after Oracle 9i including Oracle
Instant Client.

If you need to use Oracle 8, use ruby-oci8 2.0.6.

What's new
==========

See {file:NEWS}.

Sample one-liner
================

Connect to scott/tiger, select `emp` and print as CSV format.

    ruby -r oci8 -e "OCI8.new('scott', 'tiger').exec('select * from emp') do |r| puts r.join(','); end"

If you install a ruby-oci8 gem package, you may need to add `-rubygems` before `-r oci8`.

Documentation
=============

* http://rubydoc.info/gems/ruby-oci8/

Installation
============

* {file:docs/install-full-client.md Install for Oracle Full Client}
* {file:docs/install-instant-client.md Install for Oracle Instant Client}
* {file:docs/install-binary-package.md Install Binary Package}

Report issues
=============

* {file:docs/report-installation-issue.md}
* [The issues page on github](https://github.com/kubo/ruby-oci8/issues)

License
=======

* {file:COPYING 2-clause BSD-style license} from ruby-oci8 2.1.3
* {file:COPYING_old old Ruby license} until 2.1.2
