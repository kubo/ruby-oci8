require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestProperties < Minitest::Test
  def test_tcp_keepalive_time
    begin
      oldval = OCI8.properties[:tcp_keepalive_time]
      begin
        OCI8.properties[:tcp_keepalive_time] = 600
        assert(true)
      ensure
        OCI8.properties[:tcp_keepalive_time] = oldval
      end
    rescue NotImplementedError
    end
  end
end
