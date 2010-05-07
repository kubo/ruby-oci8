#--
# connection_pool.rb -- OCI8::ConnectionPool
#
# Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>
#++

class OCI8
  class ConnectionPool

    # call-seq:
    #   timeout -> integer
    #
    # Connections idle for more than this time value (in seconds) are
    # terminated, to maintain an optimum number of open
    # connections. If it is zero, the connections are never timed out.
    # The default value is zero.
    #
    # <b>Note:</b> Shrinkage of the pool only occurs when there is a network
    # round trip. If there are no operations, then the connections
    # stay alive.
    def timeout
      attr_get_ub4(OCI_ATTR_CONN_TIMEOUT)
    end

    # call-seq:
    #   timeout = integer
    #
    # Changes the timeout in seconds to terminate idle connections.
    def timeout=(val)
      attr_set_ub4(OCI_ATTR_CONN_TIMEOUT, val)
    end

    # call-seq:
    #   nowait? -> true or false
    #
    # If true, an error is thrown when all the connections in the pool
    # are busy and the number of connections has already reached the
    # maximum. Otherwise the call waits till it gets a connection.
    # The default value is false.
    def nowait?
      attr_get_ub1(OCI_ATTR_CONN_NOWAIT) != 0
    end

    # call-seq:
    #   nowait = true or false
    #
    # Changes the behavior when all the connections in the pool
    # are busy and the number of connections has already reached the
    # maximum.
    def nowait=(val)
      attr_set_ub1(OCI_ATTR_CONN_NOWAIT, val ? 1 : 0)
    end

    # call-seq:
    #   busy_count -> integer
    #
    # Returns the number of busy connections.
    def busy_count
      attr_get_ub4(OCI_ATTR_CONN_BUSY_COUNT)
    end

    # call-seq:
    #   open_count -> integer
    #
    # Returns the number of open connections.
    def open_count
      attr_get_ub4(OCI_ATTR_CONN_OPEN_COUNT)
    end

    # call-seq:
    #   min -> integer
    #
    # Returns the number of minimum connections.
    def min
      attr_get_ub4(OCI_ATTR_CONN_MIN)
    end

    # call-seq:
    #   max -> integer
    #
    # Returns the number of maximum connections.
    def max
      attr_get_ub4(OCI_ATTR_CONN_MAX)
    end

    # call-seq:
    #   incr -> integer
    #
    # Returns the connection increment parameter.
    def incr
      attr_get_ub4(OCI_ATTR_CONN_INCR)
    end

    # 
    def destroy
      free
    end
  end
end
