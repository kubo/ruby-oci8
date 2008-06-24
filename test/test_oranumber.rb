# Low-level API
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require File.dirname(__FILE__) + '/config'

class TestOraNumber < RUNIT::TestCase

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
    @env, @svc, @stmt = setup_lowapi()
  end

  def test_to_s
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      assert_equal(val, number.get.to_s)
    end
  end

  def test_to_i
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      assert_equal(val.to_i, number.get.to_i)
    end
  end

  def test_to_f
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      assert_equal(val.to_f, number.get.to_f)
    end
  end

  def test_uminus
    @stmt.prepare("BEGIN :number := - TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      assert_equal(val, (- (number.get)).to_s)
    end
  end

  def test_dup
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      n = number.get
      assert_equal(n.to_s, n.dup.to_s)
      assert_equal(n.to_s, n.clone.to_s)
    end
  end

  def test_marshal
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    NUMBER_CHECK_TARGET.each do |val|
      str.set(val)
      @stmt.execute(@svc)
      n = number.get
      assert_equal(n.to_s, Marshal.load(Marshal.dump(n)).to_s)
    end
  end

  def test_new_from_string
    @stmt.prepare("BEGIN :number := TO_NUMBER(:str); END;")
    number = @stmt.bindByName(":number", OraNumber)
    str = @stmt.bindByName(":str", String, 40)
    (NUMBER_CHECK_TARGET + ["1.230", "1.2300", "1.23000"]).each do |val|
      str.set(val)
      @stmt.execute(@svc)
      assert_equal(number.get.to_s, OraNumber.new(val).to_s)
    end
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestOraNumber.suite())
end
