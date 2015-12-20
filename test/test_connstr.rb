require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestConnStr < Minitest::Test
  TEST_CASES =
    [
     # success cases:
     #  [ 'connect_string', expected result as an array]
     # error cases:
     #  [ 'connect_string', ExceptionClass]
     ["hr/hr@host/service_name", ["hr", "hr", "host/service_name", nil]],
     ["sys/syspw@host/service_name   AS    SYSdba ", ["sys", "syspw", "host/service_name", :SYSDBA]],
     ["sys/syspw@host:1521/service_name as sysdba", ["sys", "syspw", "host:1521/service_name", :SYSDBA]],
     # error cases
     ["service_name", ArgumentError],
     ["", ArgumentError],
     ["foo bar/baz", ArgumentError],
     ["foo@bar/baz", ArgumentError],
     # parse_connect_string doesn't check validity of privilege.
     ["foo/bar as foo_bar", ["foo", "bar", nil, :FOO_BAR]],

     ##
     ## following test cases are contributed by Shiwei Zhang.
     ##
     #"username/password"
     ["username/password", ["username", "password", nil, nil]],
     #"username/password@[//]host[:port][/service_name]"
     ["username/password@host", ["username", "password", "host", nil]],
     ["username/password@host/service_name", ["username", "password", "host/service_name", nil]],
     ["username/password@host:1521", ["username", "password", "host:1521", nil]],
     ["username/password@host:1521/service_name", ["username", "password", "host:1521/service_name", nil]],
     ["username/password@//host", ["username", "password", "//host", nil]],
     ["username/password@//host/service_name", ["username", "password", "//host/service_name", nil]],
     ["username/password@//host:1521", ["username", "password", "//host:1521", nil]],
     ["username/password@//host:1521/service_name", ["username", "password", "//host:1521/service_name", nil]],
     #"username/password as{sysoper|sysdba}"
     ["username/password as sysoper", ["username", "password", nil, :SYSOPER]],
     ["username/password as sysdba", ["username", "password", nil, :SYSDBA]],
     #"username/password@[//]host[:port][/service_name] as {sysoper|sysdba}"
     ["username/password@host as sysoper", ["username", "password", "host", :SYSOPER]],
     ["username/password@host as sysdba", ["username", "password", "host", :SYSDBA]],
     ["username/password@host/service_name as sysoper", ["username", "password", "host/service_name", :SYSOPER]],
     ["username/password@host/service_name as sysdba", ["username", "password", "host/service_name", :SYSDBA]],
     ["username/password@host:1521 as sysoper", ["username", "password", "host:1521", :SYSOPER]],
     ["username/password@host:1521 as sysdba", ["username", "password", "host:1521", :SYSDBA]],
     ["username/password@host:1521/service_name as sysoper", ["username", "password", "host:1521/service_name", :SYSOPER]],
     ["username/password@host:1521/service_name as sysdba", ["username", "password", "host:1521/service_name", :SYSDBA]],
     ["username/password@//host as sysoper", ["username", "password", "//host", :SYSOPER]],
     ["username/password@//host as sysdba", ["username", "password", "//host", :SYSDBA]],
     ["username/password@//host/service_name as sysoper", ["username", "password", "//host/service_name", :SYSOPER]],
     ["username/password@//host/service_name as sysdba", ["username", "password", "//host/service_name", :SYSDBA]],
     ["username/password@//host:1521 as sysoper", ["username", "password", "//host:1521", :SYSOPER]],
     ["username/password@//host:1521 as sysdba", ["username", "password", "//host:1521", :SYSDBA]],
     ["username/password@//host:1521/service_name as sysoper", ["username", "password", "//host:1521/service_name", :SYSOPER]],
     ["username/password@//host:1521/service_name as sysdba", ["username", "password", "//host:1521/service_name", :SYSDBA]],
     ["/passwd@192.168.19.19:1521/orcl as sysdba", ["", "passwd", "192.168.19.19:1521/orcl", :SYSDBA]],
     ["/", [nil, nil, nil, nil]],
     ["/ as sysdba", [nil, nil, nil, :SYSDBA]],
    ]

  def test_connstr
    obj = OCI8.allocate # create an uninitialized object.
    TEST_CASES.each do |test_case|
      case test_case[1]
      when Array
        # use instance_eval to call a private method parse_connect_string.
        result = obj.instance_eval { parse_connect_string(test_case[0]) }
        assert_equal(test_case[1], result, test_case[0])
      when Class
        assert_raises(test_case[1]) do
          result = obj.instance_eval { parse_connect_string(test_case[0]) }
        end
      else
        raise "unsupported testcase"
      end
    end
  end

  # https://docs.oracle.com/database/121/NETAG/naming.htm#BABJBFHJ
  DESC_TEST_CASE =
    [
     # Cannot distinguish net service names from easy connect strings in this case.
     ["sales-server", nil, nil, "sales-server"],
     # Easy Connect string with host.
     ["//sales-server", nil, nil, <<EOS],
(DESCRIPTION=
   (CONNECT_DATA=
       (SERVICE_NAME=))
   (ADDRESS=
       (PROTOCOL=TCP)
       (HOST=sales-server)
       (PORT=1521)))
EOS
     # Easy Connect string with host and port.
     ["sales-server:3456", nil, nil, <<EOS],
(DESCRIPTION=
   (CONNECT_DATA=
       (SERVICE_NAME=))
   (ADDRESS=
       (PROTOCOL=TCP)
       (HOST=sales-server)
       (PORT=3456)))
EOS
     # The host name is sales-server and the service name is sales.
     ["sales-server/sales", nil, nil, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
     (SERVICE_NAME=sales))
  (ADDRESS=
     (PROTOCOL=TCP)
     (HOST=sales-server)
     (PORT=1521)))
EOS
     # Easy Connect string with IPv6 address.
     ["[2001:0db8:0:0::200C:417A]:80/sales", nil, nil, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
      (SERVICE_NAME=sales))
  (ADDRESS=
      (PROTOCOL=TCP)
      (HOST=2001:0db8:0:0::200C:417A)
      (PORT=80)))
EOS
     # Easy Connect string with IPv6 host address.
     ["sales-server:80/sales", nil, nil, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
      (SERVICE_NAME=sales))
  (ADDRESS=
      (PROTOCOL=TCP)
      (HOST=sales-server)
      (PORT=80)))
EOS
     # Easy Connect string with host, service name, and server.
     ["sales-server/sales:dedicated/inst1", nil, nil, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
      (SERVICE_NAME=sales)
      (SERVER=dedicated)
      (INSTANCE_NAME=inst1))
  (ADDRESS=
      (PROTOCOL=TCP)
      (HOST=sales-server)
      (PORT=1521)))
EOS
      ["sales-server//inst1", nil, nil, <<EOS],
(DESCRIPTION=
   (CONNECT_DATA=
      (SERVICE_NAME=)
      (INSTANCE_NAME=inst1))
   (ADDRESS=
      (PROTOCOL=TCP)
      (HOST=sales-server)
      (PORT=1521)))
EOS
     # 
     ["sales-server/sales", 20, nil, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
     (SERVICE_NAME=sales))
  (ADDRESS=
     (PROTOCOL=TCP)
     (HOST=sales-server)
     (PORT=1521))
  (TRANSPORT_CONNECT_TIMEOUT=20))
EOS
     # 
     ["sales-server/sales", nil, 30, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
     (SERVICE_NAME=sales))
  (ADDRESS=
     (PROTOCOL=TCP)
     (HOST=sales-server)
     (PORT=1521))
  (CONNECT_TIMEOUT=30))
EOS
     #
     ["sales-server/sales", 20, 30, <<EOS],
(DESCRIPTION=
  (CONNECT_DATA=
     (SERVICE_NAME=sales))
  (ADDRESS=
     (PROTOCOL=TCP)
     (HOST=sales-server)
     (PORT=1521))
  (TRANSPORT_CONNECT_TIMEOUT=20)
  (CONNECT_TIMEOUT=30))
EOS
    ]

  def test_connect_descriptor
    obj = OCI8.allocate # create an uninitialized object.
    DESC_TEST_CASE.each do |test_case|
      easy_connect_string = test_case[0]
      tcp_connnect_timeout= test_case[1]
      outbound_connnect_timeout = test_case[2]
      expected_result = test_case[3].gsub(/\s/, '')
      # use instance_eval to call a private method to_connect_descriptor
      result = obj.instance_eval { to_connect_descriptor(easy_connect_string, tcp_connnect_timeout, outbound_connnect_timeout) }
      assert_equal(expected_result, result, easy_connect_string)
    end
  end
end
