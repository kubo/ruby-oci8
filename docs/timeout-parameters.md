# @title Timeout Parameters

Timeout Parameters
==================

The following timeout parameters are available since ruby-oci8 2.2.2.

* tcp_connect_timeout
* outbound_connect_timeout
* send_timeout
* recv_timeout

For example:

    OCI8.properties[:tcp_connect_timeout] = 10
    OCI8.properties[:outbound_connect_timeout] = 15
    OCI8.properties[:send_timeout] = 60
    OCI8.properties[:recv_timeout] = 60

These parameters are equivalent to [TCP.CONNECT_TIMEOUT][], [SQLNET.OUTBOUND_CONNECT_TIMEOUT][],
[SQLNET.SEND_TIMEOUT][] and [SQLNET.RECV_TIMEOUT][] respectively in client-side `sqlnet.ora`.
They affect only TCP connections.

The first two parameters `tcp_connect_timeout` and `outbound_connect_timeout`
are applied only to [connect descriptors][connect descriptor] using [Easy Connect Naming Method][EZCONNECT].
If you use easy connect naming method without any of `port`, `service_name`, `server` and `instance_name`,
you need to use `//host` to distinguish it from a net service name in `tnsnames.ora`. If you need
these parameters without using easy connect naming, add equivalent parameters [CONNECT_TIMEOUT][]
and/or [TRANSPORT_CONNECT_TIMEOUT][] in your `tnsname.ora` instead.

The next two parameters `send_timeout` and `recv_timeout` are available on Oracle 11g client
or upper. These parameters can be changed by {OCI8#send_timeout=} and/or {OCI8#recv_timeout=}
after establishing a connection.

Note that establishing a connection consists of two phases. The first phase is connecting
to a service. The second phase is user authentication. The first two parameters
`tcp_connect_timeout` and `outbound_connect_timeout` affect only the first phase.
The next two parameters `send_timeout` and `recv_timeout` affect network round trips
since the second phase.

[TCP.CONNECT_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#BIIDDACA
[SQLNET.OUTBOUND_CONNECT_TIMEOUT]: https://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF427
[SQLNET.SEND_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF228
[SQLNET.RECV_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF227
[connect descriptor]: https://docs.oracle.com/database/121/NETRF/glossary.htm#BGBEDFBF
[EZCONNECT]: https://docs.oracle.com/database/121/NETAG/naming.htm#NETAG255
[CONNECT_TIMEOUT]: https://docs.oracle.com/database/121/NETRF/tnsnames.htm#NETRF666
[TRANSPORT_CONNECT_TIMEOUT] https://docs.oracle.com/database/121/NETRF/tnsnames.htm#NETRF1982
