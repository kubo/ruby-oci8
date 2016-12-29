require 'oci8'
require File.dirname(__FILE__) + '/config'

class TestBindInteger < Minitest::Test

  def setup
    @conn = get_oci8_connection
  end

  POSITIVE_INTS = [
    100,
    2**30,
    2**31,
    2**32,
    2**33,
    2**62,
    2**63,
    2**64,
    ('9' * 38).to_i
  ]

  def bind_and_get(input_value, output_type)
    cursor = @conn.parse("BEGIN :out := :in; END;")
    cursor.bind_param(:out, output_type)
    cursor.bind_param(:in, input_value)
    cursor.exec
    result = cursor[:out]
    cursor.close
    result
  end

  (POSITIVE_INTS + [0] + POSITIVE_INTS.map(&:-@)).each do |num|
    define_method("test_bind_param_with_input_of '#{num}'") do
      assert_equal(OraNumber.new(num.to_s), bind_and_get(num, OraNumber))
    end

    define_method("test_bind_param_with_output_of '#{num}'") do
      result = bind_and_get(OraNumber.new(num.to_s), Integer)
      assert_equal(num, result)
      assert_kind_of(Integer, result)
    end
  end

  def teardown
    @conn.logoff
  end
end
