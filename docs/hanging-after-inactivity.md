# @title Hanging After a Long Period of Inactivity

Hanging After a Long Period of Inactivity
=========================================

When a database connection hangs after a long period of inactivity,
this document may help you.

When a firewall resides between Oracle database server and client, the
firewall sometimes drops inactive connections as dead ones. If a
client connects to an Oracle server and tries to use the connection
after a long sleep (> 1 hour), the client may hang due to the
dropped connection. This issue will be solved by setting TCP keepalive
packets whose keepalive time parameter is smaller than the inactive
connection timeout in the firewall.

TCP keepalive is enabled by [(ENABLE=broken)][] in a connect
descriptor such as `(DESCRIPTION=(ENABLE=broken)...`.
If you use easy connect naming such as `//host/service_name`,
ruby-oci8 sets the parameter on behalf of you when
`OCI8.properties[:tcp_keepalive] = true`. (This is available since
ruby-oci8 2.2.4.)

The default TCP keepalive time is two hours, which is usually larger
than the inactive connection timeout in the firewall. The default
value in the system is configurable via [procfs and sysctl on Linux][]
or [registry parameters on Windows][]. If you have no privilege to
customize the system, you can change it in an application layer
by `OCI8.properties[:tcp_keepalive_time] = keepalive_time_in_second`.

`OCI8.properties[:tcp_keepalive_time]` is supported on the following
platforms since ruby-oci8 2.2.4.

* Linux i386 and x86_64
* macOS
* Windows x86 and x64
* Solaris x86_64.

It may be supported on the following platforms if someone helps me
to fix [plthook][] used by ruby-oci8.

* HP-UX IA64
* HP-UX PA-RISC 64
* Solaris SPARC

This feature is implemented with hackish way. When
`OCI8.properties[:tcp_keepalive_time]` is set, Oracle client library's
procedure linkage table is modified to intercept [setsockopt][] system
calls. When Oracle client library issues a system call which enables
TCP keepalive, ruby-oci8 changes the TCP keepalive time of the
connection immediately after the system call. I hope that Oracle adds
a new OCI handle attribute to support this feature natively in the future.

[(ENABLE=broken)]: https://docs.oracle.com/database/121/NETRF/tnsnames.htm#CHDCDGCE
[procfs and sysctl on Linux]: http://tldp.org/HOWTO/TCP-Keepalive-HOWTO/usingkeepalive.html
[registry parameters on Windows]: https://blogs.technet.microsoft.com/nettracer/2010/06/03/things-that-you-may-want-to-know-about-tcp-keepalives/
[plthook]: https://github.com/kubo/plthook
[setsockopt]: http://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html
