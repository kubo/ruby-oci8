# @title Hanging After a Long Period of Inactivity

Hanging After a Long Period of Inactivity
=========================================

When a database connection hangs after a long period of inactivity,
this document may help you.

When a firewall or a NAT proxy resides between Oracle database server
and client, it sometimes drops inactive connections as dead ones. If a
client connects to an Oracle server and tries to use the connection
after a long sleep (> 1 hour), the client may hang in an effort to use the
dropped connection. This issue will be solved by setting TCP keepalive
packets whose keepalive time parameter is smaller than the inactive
connection timeout.

TCP keepalive is enabled by [(ENABLE=broken)][] in a connect
descriptor. If you use easy connect naming such as `//hostname/service_name`,
ruby-oci8 sets the parameter on behalf of you when the `tcp_keepalive`
property is set. (This is available since ruby-oci8 2.2.4.)

    OCI8.properties[:tcp_keepalive] = true
    conn = OCI8.new(username, password, '//hostname/service_name')

This is equivalent to the following:

    connect_descriptor = "(DESCRIPTION=(ENABLE=broken)(ADDRESS=(PROTOCOL=tcp)(HOST=hostname)(PORT=1521))(CONNECT_DATA=(SERVICE_NAME=service_name)))"
    conn = OCI8.new(username, password, connect_descriptor)

The default TCP keepalive time is two hours, which may be larger
than the inactive connection timeout. The default
value in the system is configurable via [procfs and sysctl on Linux][]
or [registry parameters on Windows][]. If you have no privilege to
customize the system, you can change per-connection keepalive time
by the `tcp_keepalive_time` property.

    OCI8.properties[:tcp_keepalive_time] = 600 # 10 minutes

It is supported on the following platforms since ruby-oci8 2.2.4.

* Linux i386 and x86_64
* macOS x86_64
* Windows x86 and x64
* Solaris x86_64

When it is set on unsupported platforms, you get `NotImplementedError`.

This feature is implemented with hackish way. When
`tcp_keepalive_time` is set, Oracle client library's
procedure linkage table is modified to intercept [setsockopt][] system
call. When Oracle client library issues the system call which enables
TCP keepalive, [ruby-oci8 changes][] the per-connection TCP keepalive time
immediately.

I hope that Oracle adds a new OCI handle attribute to support this
feature natively in the future.

[(ENABLE=broken)]: https://docs.oracle.com/database/121/NETRF/tnsnames.htm#CHDCDGCE
[procfs and sysctl on Linux]: http://tldp.org/HOWTO/TCP-Keepalive-HOWTO/usingkeepalive.html
[registry parameters on Windows]: https://blogs.technet.microsoft.com/nettracer/2010/06/03/things-that-you-may-want-to-know-about-tcp-keepalives/
[plthook]: https://github.com/kubo/plthook
[setsockopt]: http://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html
[ruby-oci8 changes]: https://github.com/kubo/ruby-oci8/blob/ruby-oci8-2.2.4/ext/oci8/hook_funcs.c#L302-L318
