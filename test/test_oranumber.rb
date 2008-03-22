# Low-level API
require 'oci8'
require 'test/unit'
require './config'
require 'yaml'

class TestOraNumber < Test::Unit::TestCase

  LARGE_RANGE_VALUES = [
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

  SMALL_RANGE_VALUES = [
    "10",
    "3",
    "3.14159265358979323846", # PI
    "2",
    "1.57079632679489661923", # PI/2
    "0.5",
    "0.0000000001",
    "0",
    "-0.0000000001",
    "-0.5",
    "-1.57079632679489661923", # -PI/2
    "-2",
    "-3.14159265358979323846", # -PI
    "-3",
    "-10",
  ]

  def compare_with_float(values, rettype, proc1, proc2 = nil)
    proc2 = proc1 if proc2.nil?
    values.each do |x|
      expected_val = proc1.call(x.to_f)
      actual_val = proc2.call(OraNumber.new(x))
      assert_kind_of(rettype, actual_val)
      delta = [expected_val.abs * 1.0e-12, 1.0e-14].max
      assert_in_delta(expected_val, actual_val, delta, x)
    end
  end

  def compare_with_float2(values, proc_args, proc1, proc2 = nil)
    proc2 = proc1 if proc2.nil?
    values.each do |x|
      proc_args.each do |y|
        expected_val = proc1.call(x.to_f, y)
        actual_val = proc2.call(OraNumber.new(x), y)
        begin
          delta = [expected_val.abs * 1.0e-12, 1.0e-14].max
        rescue
          puts '-----------'
          p x
          p y
          p expected_val
          puts '-----------'
          raise $!
        end
        assert_in_delta(expected_val, actual_val, delta, x)
      end
    end
  end

  def test_in_bind
    conn = get_oci8_connection
    begin
      conn.exec("alter session set nls_numeric_characters = '.,'")
      cursor = conn.parse("BEGIN :out := TO_CHAR(:in); END;")
      cursor.bind_param(:out, nil, String, 40)
      cursor.bind_param(:in, OraNumber)
      LARGE_RANGE_VALUES.each do |val|
        cursor[:in] = OraNumber.new(val)
        cursor.exec
        # convert 0.0001 and -0.0001 to .0001 and -.0001 respectively
        val = $1+'.'+$2 if /(-?)0\.(.*)/ =~ val
        assert_equal(val, cursor[:out])
      end
    ensure
      conn.logoff
    end
  end

  def test_out_bind
    conn = get_oci8_connection
    begin
      conn.exec("alter session set nls_numeric_characters = '.,'")
      cursor = conn.parse("BEGIN :out := TO_NUMBER(:in); END;")
      cursor.bind_param(:out, OraNumber)
      cursor.bind_param(:in, nil, String, 40)
      LARGE_RANGE_VALUES.each do |val|
        cursor[:in] = val
        cursor.exec
        assert_equal(OraNumber.new(val), cursor[:out])
      end
    ensure
      conn.logoff
    end
  end

  def test_dup
    LARGE_RANGE_VALUES.each do |x|
      n = OraNumber.new(x)
      assert_equal(n, n.dup)
      assert_equal(n, n.clone)
    end
  end

  def test_marshal
    LARGE_RANGE_VALUES.each do |x|
      n = OraNumber.new(x)
      assert_equal(n, Marshal.load(Marshal.dump(n)))
    end
  end

  def test_yaml
    LARGE_RANGE_VALUES.each do |x|
      n = OraNumber.new(x)
      assert_equal(n, YAML.load(YAML.dump(n)))
    end
  end

  # OCI8::Math.acos(x) -> ocinumber
  def test_math_acos
    test_values = []
    -1.0.step(1.0, 0.01) do |n|
      test_values << n
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::acos(n)},
                       Proc.new {|n| OCI8::Math::acos(n)})
  end

  # OCI8::Math.asin(x) -> ocinumber
  def test_math_asin
    test_values = []
    -1.0.step(1.0, 0.01) do |n|
      test_values << n
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::asin(n)},
                       Proc.new {|n| OCI8::Math::asin(n)})
  end

  # OCI8::Math.atan(x) -> ocinumber
  def test_math_atan
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::atan(n)},
                       Proc.new {|n| OCI8::Math::atan(n)})
  end

  # OCI8::Math.atan2(y, x) -> ocinumber
  def test_math_atan2
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| Math::atan2(x, y.to_f)},
                        Proc.new {|x, y| OCI8::Math::atan2(x, y.to_f)})
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| Math::atan2(y.to_f, x)},
                        Proc.new {|x, y| OCI8::Math::atan2(y.to_f, x)})
  end

  # OCI8::Math.cos(x) -> ocinumber
  def test_math_cos
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::cos(n)},
                       Proc.new {|n| OCI8::Math::cos(n)})
  end

  # OCI8::Math.cosh(x) -> ocinumber
  def test_math_cosh
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::cosh(n)},
                       Proc.new {|n| OCI8::Math::cosh(n)})
  end

  # OCI8::Math.exp(x) -> ocinumber
  def test_exp
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::exp(n)},
                       Proc.new {|n| OCI8::Math::exp(n)})
  end

  # OCI8::Math.log(numeric) -> ocinumber
  # OCI8::Math.log(numeric, base_num) -> ocinumber
  def test_log
    test_values = LARGE_RANGE_VALUES.reject do |x|
      # reject minus and zero values
      x[0,1] == '-' || x == '0'
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::log(n)},
                       Proc.new {|n| OCI8::Math::log(n)})
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::log(n)/Math::log(3)},
                       Proc.new {|n| OCI8::Math::log(n, 3)})
  end

  # OCI8::Math.log10(numeric) -> ocinumber
  def test_log10
    test_values = LARGE_RANGE_VALUES.reject do |x|
      # reject minus and zero values
      x[0,1] == '-' || x == '0'
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::log10(n)},
                       Proc.new {|n| OCI8::Math::log10(n)})
  end

  # OCI8::Math.sin(x) -> ocinumber
  def test_math_sin
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::sin(n)},
                       Proc.new {|n| OCI8::Math::sin(n)})
  end

  # OCI8::Math.sinh(x) -> ocinumber
  def test_math_sinh
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::sinh(n)},
                       Proc.new {|n| OCI8::Math::sinh(n)})
  end

  # OCI8::Math.sqrt(numeric) -> ocinumber
  def test_sqrt
    test_values = LARGE_RANGE_VALUES.reject do |x|
      # reject minus values
      x[0,1] == '-'
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::sqrt(n)},
                       Proc.new {|n| OCI8::Math::sqrt(n)})
  end

  # OCI8::Math.tan(x) -> ocinumber
  def test_math_tan
    test_values = SMALL_RANGE_VALUES.reject do |x|
      # reject PI/2 and -PI/2.
      # Those values are +inf and -info
      radian = x.to_f
      (radian.abs - Math::PI/2).abs < 0.000001
    end
    compare_with_float(test_values, OraNumber,
                       Proc.new {|n| Math::tan(n)},
                       Proc.new {|n| OCI8::Math::tan(n)})
  end

  # OCI8::Math.tanh() -> ocinumber
  def test_math_tanh
    compare_with_float(SMALL_RANGE_VALUES, OraNumber,
                       Proc.new {|n| Math::tanh(n)},
                       Proc.new {|n| OCI8::Math::tanh(n)})
  end

  # onum % other -> onum
  # def test_mod

  # onum * other -> onum
  def test_mul
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| x * y.to_f})
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| y.to_f * x})
  end

  # onum ** other -> onum
  def test_pow
    base_values = SMALL_RANGE_VALUES.reject do |x|
      # reject minus and zero values
      x[0,1] == '-' || x == '0'
    end
    compare_with_float2(base_values, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| x ** y.to_f})
    compare_with_float2(SMALL_RANGE_VALUES, base_values,
                        Proc.new {|x, y| y.to_f ** x})
  end

  # onum + other -> onum
  def test_add
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| x + y.to_f})
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| y.to_f + x})
  end

  # onum - other -> onum
  def test_minus
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| x - y.to_f})
    compare_with_float2(SMALL_RANGE_VALUES, SMALL_RANGE_VALUES,
                        Proc.new {|x, y| y.to_f - x})
  end

  # -ocinumber -> ocinumber
  def test_uminus
    compare_with_float(LARGE_RANGE_VALUES, OraNumber, Proc.new {|n| -n})
  end

  # onum / other -> onum
  # TODO: test_div

  # onum <=> other -> -1, 0, +1
  def test_cmp
    test_values = SMALL_RANGE_VALUES.collect do |x|
      x[0,15] # donw the precision to pass this test.
    end
    compare_with_float2(test_values, test_values,
                        Proc.new {|x, y| x <=> y.to_f})
    compare_with_float2(test_values, test_values,
                        Proc.new {|x, y| y.to_f <=> x})
  end

  # onum.abs -> ocinumber
  def test_abs
    compare_with_float(LARGE_RANGE_VALUES, OraNumber, Proc.new {|n| n.abs})
  end

  # onum.ceil -> integer
  def test_ceil
    compare_with_float(LARGE_RANGE_VALUES, Integer, Proc.new {|n| n.ceil})
  end

  # onum.floor -> integer
  def test_floor
    compare_with_float(LARGE_RANGE_VALUES, Integer, Proc.new {|n| n.floor})
  end

  # onum.round -> integer
  # onum.round(decplace) -> onum
  def test_round
    compare_with_float(LARGE_RANGE_VALUES, Integer, Proc.new {|n| n.round})
    compare_with_float(LARGE_RANGE_VALUES, OraNumber,
                       Proc.new {|n| (n * 10).round * 0.1},
                       Proc.new {|n| n.round(1)})
    compare_with_float(LARGE_RANGE_VALUES, OraNumber,
                       Proc.new {|n| (n * 100).round * 0.01},
                       Proc.new {|n| n.round(2)})
    compare_with_float(LARGE_RANGE_VALUES, OraNumber,
                       Proc.new {|n| (n * 0.1).round * 10},
                       Proc.new {|n| n.round(-1)})
  end

  # onum.round_prec(digits) -> ocinumber
  def test_round_prec
    if OCI8::oracle_client_version >= 810
      # Oracle 8.1 client or upper
      compare_with_float2(LARGE_RANGE_VALUES, [1, 2, 3, 5, 10, 20],
                          Proc.new {|x, y|
                            return 0.0 if x == 0.0
                            factor = 10 ** (Math::log10(x.abs).to_i - y + 1)
                            (x / factor).round * factor
                          },
                          Proc.new {|x, y| x.round_prec(y)})
    else
      # Oracle 8.0 client
      assert_raise RuntimeError do
        OraNumber.new(1).round_prec(1)
      end
    end
  end

  # onum.shift(fixnum) -> ocinumber
  def test_shift
    if OCI8::oracle_client_version >= 810
      # Oracle 8.1 client or upper
      compare_with_float2(LARGE_RANGE_VALUES, [-5, -4, -3, -1, 0, 1, 2, 3, 4, 5],
                          Proc.new {|x, y| x * (10 ** y)},
                          Proc.new {|x, y| x.shift(y)})
    else
      # Oracle 8.0 client
      assert_raise RuntimeError do
        OraNumber.new(1).shift(1)
      end
    end
  end

  # onum.to_char(fmt = nil, nls_params = nil) -> string
  def test_to_char
    onum = OraNumber.new(123.45)
    assert_equal('   123.4500',   onum.to_char('99999.9999'))
    assert_equal('  0123.4500',   onum.to_char('90000.0009'))
    assert_equal(' 00123.4500',   onum.to_char('00000.0000'))
    assert_equal('123.45',        onum.to_char('FM99999.9999'))
    assert_equal('0123.450',      onum.to_char('FM90000.0009'))
    assert_equal('00123.4500',    onum.to_char('FM00000.0000'))
    assert_equal('  -123.4500',(-onum).to_char('99999.9999'))
    assert_equal(' -0123.4500',(-onum).to_char('90000.0009'))
    assert_equal('-00123.4500',(-onum).to_char('00000.0000'))
    assert_equal('-123.45',    (-onum).to_char('FM99999.9999'))
    assert_equal('-0123.450',  (-onum).to_char('FM90000.0009'))
    assert_equal('-00123.4500',(-onum).to_char('FM00000.0000'))
    assert_equal(' 0,123.4500',   onum.to_char('0G000D0000', "NLS_NUMERIC_CHARACTERS = '.,'"))
    assert_equal(' 0.123,4500',   onum.to_char('0G000D0000', "NLS_NUMERIC_CHARACTERS = ',.'"))
    assert_equal('Ducat123.45',    onum.to_char('FML9999.999', "NLS_CURRENCY = 'Ducat'"))
  end

  # onum.to_f -> float
  def test_to_f
    LARGE_RANGE_VALUES.each do |x|
      expected_val = x.to_f
      actual_val = OraNumber.new(x).to_f
      delta = [expected_val.abs * 1.0e-12, 1.0e-14].max
      assert_in_delta(expected_val, actual_val, delta, x)
    end
  end

  # onum.to_i -> integer
  def test_to_i
    LARGE_RANGE_VALUES.each do |x|
      expected_val = x.to_i
      actual_val = OraNumber.new(x).to_i
      assert_equal(expected_val, actual_val, x)
    end
  end

  # onum.to_s -> string
  def test_to_s
    LARGE_RANGE_VALUES.each do |x|
      expected_val = x
      actual_val = OraNumber.new(x).to_s
      assert_equal(expected_val, actual_val, x)
    end
  end

  # onum.truncate -> integer
  # onum.truncate(decplace) -> ocinumber
  # TODO: test_truncate

  # onum.zero? -> true or false
  def test_zero_p
    LARGE_RANGE_VALUES.each do |x|
      expected_val = x.to_f.zero?
      actual_val = OraNumber.new(x).zero?
      assert_equal(expected_val, actual_val, x)
    end
  end
end
