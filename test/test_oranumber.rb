# Low-level API
require 'oci8'
require 'test/unit'
require './config'

class TestOraNumber < Test::Unit::TestCase

  NUMBER_CHECK_TARGET = [
    "12345678901234567890123456789012345678",
    "1234567890123456789012345678901234567",
    "1234567890123456789012345678901234567.8",
    "12.345678901234567890123456789012345678",
    "1.2345678901234567890123456789012345678",
    "1.234567890123456789012345678901234567",
    "0.0000000000000000000000000000000000001",
    "0.000000000000000000000000000000000001",
    "0",
    "2147483647", # max of 32 bit signed value
    "2147483648", # max of 32 bit signed value + 1
    "-2147483648", # min of 32 bit signed value
    "-2147483649", # min of 32 bit signed value - 1
    "9223372036854775807",  # max of 64 bit signed value
    "9223372036854775808",  # max of 64 bit signed value + 1
    "-9223372036854775808",  # min of 64 bit signed value
    "-9223372036854775809",  # min of 64 bit signed value - 1
    "-12345678901234567890123456789012345678",
    "-1234567890123456789012345678901234567",
    "-123456789012345678901234567890123456",
    "-1234567890123456789012345678901234567.8",
    "-12.345678901234567890123456789012345678",
    "-1.2345678901234567890123456789012345678",
    "-0.0000000000000000000000000000000000001",
    "-0.000000000000000000000000000000000001",
    "-0.00000000000000000000000000000000001",
    "1",
    "20",
    "300",
    "-1",
    "-20",
    "-300",
    "1.123",
    "12.123",
    "123.123",
    "1.1",
    "1.12",
    "1.123",
    "-1.123",
    "-12.123",
    "-123.123",
    "-1.1",
    "-1.12",
    "-1.123",
  ]

  def setup
    @conn = $dbconn
  end

  def test_to_s
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      assert_equal(val, cursor[:number].to_s)
    end
  end

  def test_to_i
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      assert_equal(val.to_i, cursor[:number].to_i)
    end
  end

  def test_to_f
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      assert_in_delta(val.to_f, cursor[:number].to_f, val.to_f.abs * 1e-14)
    end
  end

  def test_uminus
    cursor = @conn.parse("BEGIN :number := - TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      assert_equal(val, (- (cursor[:number])).to_s)
    end
  end

  def test_dup
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      n = cursor[:number]
      assert_equal(n.to_s, n.dup.to_s)
      assert_equal(n.to_s, n.clone.to_s)
    end
  end

  def test_marshal
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      cursor[:str] = val
      cursor.exec
      n = cursor[:number]
      assert_equal(n.to_s, Marshal.load(Marshal.dump(n)).to_s)
    end
  end

  def test_new_from_string
    cursor = @conn.parse("BEGIN :number := TO_NUMBER(:str); END;")
    cursor.bind_param(:number, OraNumber)
    cursor.bind_param(:str, nil, String, 40)
    (NUMBER_CHECK_TARGET + ["1.230", "1.2300", "1.23000"]).each do |val|
      cursor[:str] = val
      cursor.exec
      assert_equal(cursor[:number].to_s, OraNumber.new(val).to_s)
    end
  end

  def teardown
    @conn.rollback
  end
end
