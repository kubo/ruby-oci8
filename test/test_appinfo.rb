require 'oci8'
require 'test/unit'
require File.dirname(__FILE__) + '/config'

class TestAppInfo < Test::Unit::TestCase

  def setup
    @conn = get_oci8_connection
  end

  def test_set_client_identifier
    # set client_id
    client_id = "ruby-oci8:#{Process.pid()}"
    @conn.client_identifier = client_id
    assert_equal(client_id, @conn.select_one("SELECT SYS_CONTEXT('USERENV', 'CLIENT_IDENTIFIER') FROM DUAL")[0]);
    # check the first character
    assert_raise ArgumentError do
      @conn.client_identifier = ':bad_identifier'
    end

    # clear client_id
    @conn.client_identifier = nil
    assert_nil(@conn.select_one("SELECT SYS_CONTEXT('USERENV', 'CLIENT_IDENTIFIER') FROM DUAL")[0]);
  end

  def teardown
    @conn.logoff
  end
end
