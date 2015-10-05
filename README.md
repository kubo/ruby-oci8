What is ruby-oci8
=================

Ruby-oci8 is a ruby interface for Oracle Database. The latest version
is available for all Oracle versions after Oracle 10g including Oracle
Instant Client.

Use ruby-oci8 2.0.6 for Oracle 8 or use ruby-oci8 2.1.8 for Oracle 9i.

What's new
==========

See [NEWS](NEWS).

Sample one-liner
================

Connect to scott/tiger, select `emp` and print as CSV format.

`ruby -r oci8 -e "OCI8.new('scott', 'tiger').exec('select * from emp') do |r| puts r.join(','); end"`

If you install a ruby-oci8 gem package, you may need to add `-rubygems` before `-r oci8`.

Documentation
=============

* http://rubydoc.info/gems/ruby-oci8/

Installation
============

* [Install for Oracle Full Client](docs/install-full-client.md)
* [Install for Oracle Instant Client](docs/install-instant-client.md)
* [Install Binary Package](docs/install-binary-package.md)
* [Install on OS X] (docs/install-on-osx.md)

Report issues
=============

* [Docs: Report Installation Issues] (docs/report-installation-issue.md)
* [The issues page on github](https://github.com/kubo/ruby-oci8/issues)

License
=======

* [BSD LICENSE](COPYING) from ruby-oci8 2.1.3
* [BSD LICENSE (OLD)](COPYING_OLD) until 2.1.2
