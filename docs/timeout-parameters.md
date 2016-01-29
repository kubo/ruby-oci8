# @title Timeout Parameters

Timeout Parameters
==================

The following timeout parameters are available since ruby-oci8 2.2.2.

* tcp_connect_timeout
* connect_timeout
* send_timeout
* recv_timeout

For example:

    OCI8.properties[:tcp_connect_timeout] = 10
    OCI8.properties[:connect_timeout] = 15
    OCI8.properties[:send_timeout] = 60
    OCI8.properties[:recv_timeout] = 60

These parameters are applied only to TCP/IP connections.

The first two parameters `tcp_connect_timeout` and `connect_timeout`
are applied only to [connect descriptors][connect descriptor] using [Easy Connect Naming Method][EZCONNECT].
If you use easy connect naming method without any of `port`, `service_name`, `server` and `instance_name`,
you need to use `//host` to distinguish it from a net service name in `tnsnames.ora`.

The next two parameters `send_timeout` and `recv_timeout` are available on Oracle 11g client
or upper.

tcp_connect_timeout
-------------------

`tcp_connect_timeout` is equivalent to [TCP.CONNECT_TIMEOUT][] in the client-side `sqlnet.ora` and
[TRANSPORT_CONNECT_TIMEOUT][] in the address descriptor.
See description about [TCP.CONNECT_TIMEOUT][] and [TRANSPORT_CONNECT_TIMEOUT][].

connect_timeout
---------------

`connect_timeout` is equivalent to [SQLNET.OUTBOUND_CONNECT_TIMEOUT][] in the client-side `sqlnet.ora`
and [CONNECT_TIMEOUT][] in the address description.
See description about [SQLNET.OUTBOUND_CONNECT_TIMEOUT][] and [CONNECT_TIMEOUT][].

Note: this parameter isn't equivalent to login timeout. It need the following three
steps to establish a database connection.

1. Establish a TCP/IP connection.
2. Establish an Oracle Net connection.
3. Authenticate and authorize the database user.

`tcp_connect_timeout` sets the timeout of the first step.
`connect_timeout` sets the total timeout of the first and the second steps.
There is no timeout parameter to limit the maximum time of all three steps.

Use `send_timeout` and `recv_timeout` in case that a TCP/IP connection stalls
in the third step.

send_timeout
------------

`send_timeout` is equivalent to [SQLNET.SEND_TIMEOUT][] in the client-side `sqlnet.ora`.
See description about [SQLNET.SEND_TIMEOUT][].

See also {OCI8#send_timeout=}.

recv_timeout
------------

`recv_timeout` is equivalent to [SQLNET.RECV_TIMEOUT][] in the client-side `sqlnet.ora`.
See description about [SQLNET.RECV_TIMEOUT][].

See also {OCI8#recv_timeout=}.

Note: This parameter must be larger than the longest SQL execution time in your applications. 

[TCP.CONNECT_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#BIIDDACA
[SQLNET.OUTBOUND_CONNECT_TIMEOUT]: https://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF427
[SQLNET.SEND_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF228
[SQLNET.RECV_TIMEOUT]: http://docs.oracle.com/database/121/NETRF/sqlnet.htm#NETRF227
[connect descriptor]: https://docs.oracle.com/database/121/NETRF/glossary.htm#BGBEDFBF
[EZCONNECT]: https://docs.oracle.com/database/121/NETAG/naming.htm#NETAG255
[CONNECT_TIMEOUT]: https://docs.oracle.com/database/121/NETRF/tnsnames.htm#NETRF666
[TRANSPORT_CONNECT_TIMEOUT]: https://docs.oracle.com/database/121/NETRF/tnsnames.htm#NETRF1982
