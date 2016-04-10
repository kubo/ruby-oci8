#--
# connection_pool.rb -- OCI8::ConnectionPool
#
# Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>
#++

#
class OCI8

  # Connection pooling is the use of a group (the pool) of reusable
  # physical connections by several sessions to balance loads.
  # See: {Oracle Call Interface Manual}[http://docs.oracle.com/cd/E11882_01/appdev.112/e10646/oci09adv.htm#sthref1479]
  #
  # This is equivalent to Oracle JDBC Driver {OCI Connection Pooling}[http://docs.oracle.com/cd/E11882_01/java.112/e16548/ociconpl.htm#JJDBC28789].
  #
  # Note that this is different with generally called connection pooling.
  # Generally connection pooling caches connections in a pool.
  # When an application needs a new connection, a connection is got from
  # the pool. So that the amount of time to establish a connection is
  # reduced. However connection pooling in ruby-oci8 is different with
  # above.
  #
  # One Oracle connection is devided into two layers: One is physical
  # connection such as TCP/IP. The other is logical connection which
  # is used by an application. The connection pooling in ruby-oci8
  # caches physical connections in a pool, not logical connections.
  # When an application needs a new connection, a logical connection
  # is created via a physical connection, which needs time to
  # establish a connection unlike generally called connection pooling.
  # When an application sends a query via a logical connection, a
  # physical connection is got from the pool. The physical connection
  # returns to the pool automatically when the query finishes.
  # As long as logical connections are used sequentially, only one
  # physical connection is used. When an application sends
  # queries at a time, it needs more than one physical connection
  # because one physical connection cannot transmit more then one
  # query at a time.
  #
  # Example:
  #   # Create a connection pool.
  #   # The number of initial physical connections: 1
  #   # The number of maximum physical connections: 5
  #   # the connection increment parameter: 2
  #   # username and password are required to establish an implicit primary session.
  #   cpool = OCI8::ConnectionPool.new(1, 5, 2, username, password, database)
  #   
  #   #   The number of physical connections: 1
  #   #   The number of logical connections: 0
  #   
  #   # Create a session.
  #   # Pass the connection pool to the third argument.
  #   conn1 = OCI8.new(username, password, cpool)
  #   
  #   # One logical connection was created.
  #   #   The number of physical connections: 1
  #   #   The number of logical connections: 1
  #   
  #   # Create another session.
  #   conn2 = OCI8.new(username, password, cpool)
  #   
  #   # Another logical connection was created.
  #   #   The number of physical connections: 1
  #   #   The number of logical connections: 2
  #   
  #   # Use conn1 and conn2 sequentially
  #   conn1.exec('...')
  #   conn2.exec('...')
  #   
  #   # Both logical connections use one physical connection.
  #   #   The number of physical connections: 1
  #   #   The number of logical connections: 2
  #   
  #   thr1 = Thread.new do
  #      conn1.exec('...')
  #   end
  #   thr2 = Thread.new do
  #      conn2.exec('...')
  #   end
  #   thr1.join
  #   thr2.join
  #   
  #   # Logical connections cannot use one physical connection at a time.
  #   # So that the number of physical connections is incremented.
  #   #   The number of physical connections: 3
  #   #   The number of logical connections: 2
  #   
  #   conn1.logoff
  #
  #   # One logical connection was closed.
  #   #   The number of physical connections: 3
  #   #   The number of logical connections: 1
  #   
  #   conn2.logoff
  #   
  #   # All logical connections were closed.
  #   #   The number of physical connections: 3
  #   #   The number of logical connections: 0
  #
  class ConnectionPool

    # Connections idle for more than this time value (in seconds) are
    # terminated, to maintain an optimum number of open
    # connections. If it is zero, the connections are never timed out.
    # The default value is zero.
    #
    # <b>Note:</b> Shrinkage of the pool only occurs when there is a network
    # round trip. If there are no operations, then the connections
    # stay alive.
    #
    # @return [Integer]
    def timeout
      attr_get_ub4(OCI_ATTR_CONN_TIMEOUT)
    end

    # Changes the timeout in seconds to terminate idle connections.
    #
    # @param [Integer] val
    def timeout=(val)
      attr_set_ub4(OCI_ATTR_CONN_TIMEOUT, val)
    end

    # If true, an error is thrown when all the connections in the pool
    # are busy and the number of connections has already reached the
    # maximum. Otherwise the call waits till it gets a connection.
    # The default value is false.
    def nowait?
      attr_get_ub1(OCI_ATTR_CONN_NOWAIT) != 0
    end

    # Changes the behavior when all the connections in the pool
    # are busy and the number of connections has already reached the
    # maximum.
    #
    # @param [Boolean] val
    def nowait=(val)
      attr_set_ub1(OCI_ATTR_CONN_NOWAIT, val ? 1 : 0)
    end

    # Returns the number of busy physical connections.
    #
    # @return [Integer]
    def busy_count
      attr_get_ub4(OCI_ATTR_CONN_BUSY_COUNT)
    end

    # Returns the number of open physical connections.
    #
    # @return [Integer]
    def open_count
      attr_get_ub4(OCI_ATTR_CONN_OPEN_COUNT)
    end

    # Returns the number of minimum physical connections.
    #
    # @return [Integer]
    def min
      attr_get_ub4(OCI_ATTR_CONN_MIN)
    end

    # Returns the number of maximum physical connections.
    #
    # @return [Integer]
    def max
      attr_get_ub4(OCI_ATTR_CONN_MAX)
    end

    # Returns the connection increment parameter.
    #
    # @return [Integer]
    def incr
      attr_get_ub4(OCI_ATTR_CONN_INCR)
    end

    # 
    def destroy
      free
    end
  end
end
